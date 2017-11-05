/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * GPL HEADER END
 */
/*
 * ============================================================================
 *
 *       Filename:  lhsm-restore.c
 *
 *    Description:  implements small utility to restore released lustre files.
 *
 *        Version:  1.0
 *        Created:  10/30/2017 04:30:39 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  John Suykerbuyk
 *   Organization:  Seagate PLC
 *
 * ============================================================================
 */

#define _GNU_SOURCE 1
#define __SIGNED__



#include "lhsm-threads.h"

#ifdef REAL_LUSTRE
#include <lustre/lustreapi.h>
#else
#include "lhsm-restore-stub.h"

#endif



void hsm_walk_dir(const char *name)
{
	DIR *dir;
	struct dirent *entry;
	if (!(dir = opendir(name))) {
		return;
	} else {
		// printf("working on %s\n", name);
		zlog_info(zctx, "working on %s", name);
	}

	while ((entry = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct hsm_user_state hus;
		int rc;
		int released;

		if (*entry->d_name =='/') 
			snprintf(path, sizeof(path), "%s%s", name, entry->d_name);
		else
			snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
		if (entry->d_type == DT_DIR) {
			if (strcmp(entry->d_name, ".") == 0\
					|| strcmp(entry->d_name, "..") == 0)
				continue;
			hsm_walk_dir(path);
		} else if (entry->d_type == DT_REG) {
			rc = llapi_hsm_state_get(path, &hus);
			if (rc != 0) {
				printf("Error: get hsm state rc %d errno %d for %s\n",\
						rc, errno, path);
				break;
			} else {
				//printf("Got hsm state for %s\n", path);
			}
			released = hus.hus_states & HS_RELEASED;
			if (released) {
				zlog_info(zctx, "hsmwalk found released: %s", path);
				struct ctx_hsm_restore_thread* \
					pthrd = restore_threads_find_idle();
				if (NULL == pthrd) {
					zlog_error(zctx, "ERROR: No idle threads found");
				} else {
					strncpy(pthrd->ctx.path, path, sizeof(pthrd->ctx.path));
					pthrd->ctx.tstate = ctx_work;
				}
			}
		}
	}
	closedir(dir);
}

int main(int argc, char** argv) {
	int rc;
	srand(time(NULL));
	rc = zlog_init(zlog_conf_file);
	if (rc) {
		fprintf(stderr, "zlog init failed with errno %d while opening %s\n",\
				rc, zlog_conf_file);
		return -1;
	}
	zctx = zlog_get_category(zlog_category);
	if (NULL == zctx) {
		fprintf(stderr, "zlog failed to get category %s from %s\n",\
				zlog_category, zlog_conf_file);
	}
	zlog_info(zctx, "BEGIN: main");
	restore_threads_init(thread_count);
	if (argc >1) {
		char* path=argv[1];
		size_t plen=strlen(path);
		/* trim trailing path delimiter if provided */
		if ((plen > 0) && (path[plen-1] == '/')) {
			plen--;
			path[plen] = 0;
		}
		hsm_walk_dir(path);
	} else {
	   hsm_walk_dir(".");
	}
	restore_threads_halt();
	zlog_fini();
	zlog_info(zctx, "EXIT: main");
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
