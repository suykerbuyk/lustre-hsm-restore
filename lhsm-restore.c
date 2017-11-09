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


#include "lhsm-restore-test.h"
#include "lhsm-threads.h"

#ifdef REAL_LUSTRE
#include <lustre/lustreapi.h>
#else
#include "lhsm-restore-stub.h"

#endif

const  char* const zlog_category_log = "lhsm_log";
const  char* const zlog_category_dbg = "lhsm_log_dbg";
const  char* const zlog_conf_file = "lhsm-restore.conf";
zlog_category_t* zctx_log = NULL;
zlog_category_t* zctx_dbg = NULL;

struct file_func_ctx {
	char file_path[PATH_MAX];
	char result[PATH_MAX *2];
	int failed;

};

int restore_file(const char* const file_path) {
	int fd;
	int rc;
	char iobuf[4096];
	fd = open(file_path, O_RDONLY);
	if (fd == 0) {
		rc = errno;
	} else {
		rc = read(fd, &iobuf, sizeof(iobuf));
		if (rc < 0) {
			/* convert to positive errno value */
			rc = -errno;
		} else {
			/* Positive value means successful bytes read */
			rc = 0; 
		}
	}
	close(fd);
	return rc;
}

int validate_file(const char* const file_path) {
	int rc;
	rc = 0;
	return rc;
}

struct file_func_ctx* file_functor(struct file_func_ctx* fctx) {
	int rc;
	struct hsm_user_state hus;
	memset(fctx->result, 0, sizeof(fctx->result));
	while(1) {
		rc = llapi_hsm_state_get(fctx->file_path, &hus);
		if (rc != 0) {
			snprintf(fctx->result, sizeof(fctx->result),
					"ERROR: LLAPI Fail %d for %s", rc, fctx->file_path);
			fctx->failed = 1;
			break;
		}
		if (hus.hus_states & HS_RELEASED) {
			if ((rc = restore_file(fctx->file_path)) != 0) {
				snprintf(fctx->result, sizeof(fctx->result),
					"ERROR: Restore Fail %d for %s", rc, fctx->file_path);
				fctx->failed = 1;
				break;
			}
		}
	}
	return fctx;
}

void hsm_walk_dir(const char *name)
{
	DIR *dir;
	struct dirent *entry;
	int rc;
	if (!(dir = opendir(name))) {
		return;
	} else {
		zlog_info(zctx_dbg, "working on %s", name);
	}
	while ((entry = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		struct hsm_user_state hus;
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
				zlog_info(zctx_dbg, "hsmwalk found released: %s", path);
				struct ctx_hsm_restore_thread* \
					pthrd = restore_threads_find_idle();
				if (NULL == pthrd) {
					zlog_error(zctx_dbg, "ERROR: No idle threads found");
				} else {
					strncpy(pthrd->ctx.path, path, sizeof(pthrd->ctx.path));
					pthrd->ctx.tstate = ctx_work;
				}
			}
		}
	}
	closedir(dir);
}

void test_functor(void* some_thing) {
	char* msg=(char*) some_thing;
	printf("MSG FROM FUNCTOR: %s\n", msg);
	return;
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
	zctx_dbg = zlog_get_category(zlog_category_dbg);
	zctx_log = zlog_get_category(zlog_category_log);
	rc = run_self_test();
	run_as_thread(test_functor, "Hello, from message");
	printf("run_self_test = %d\n", rc);
	if (NULL == zctx_dbg) {
		fprintf(stderr, "zlog failed to get category %s from %s\n",\
				zlog_category_dbg, zlog_conf_file);
	}
	zlog_info(zctx_dbg, "BEGIN: main");
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
	zlog_info(zctx_dbg, "EXIT: main");
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
