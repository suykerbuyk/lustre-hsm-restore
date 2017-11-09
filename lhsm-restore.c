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
static pthread_mutex_t thrd_mtx =  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

const char* const default_conf_content = \
"[formats]\n" \
"simple = \"%m%n\" \n" \
"normal = \"%d(%F %T) %m%n\"" \
"\n" \
"\n" \
"[rules]\n"\
"lhsm_log_dbg.DEBUG  \"lhsm-restore-dbg.log\", 10MB * 3, \"lhsm-restore-dbg.#r.log\"; normal\n" \
"lhsm_log.* \"lhsm-restore.log\"; simple\n";


int confirm_conf_file(void) {
	int rc = 0;
	int fd = 0;
	int fmask;
	if (!access(zlog_conf_file, F_OK)) {
		return 0;
	} else {
		fmask = umask(0);
		fprintf(stderr, "Missing or unreadable conf file %s\n", zlog_conf_file);
		fd = open(zlog_conf_file, O_RDWR | O_CREAT,
				S_IRUSR | S_IWUSR |
				S_IRGRP | S_IWGRP |
				S_IROTH | S_IWOTH );
		if (fd < 0) {
			rc = errno;
		} else {
			rc = 0;
			dprintf(fd, "%s", default_conf_content);
			close(fd);
		}
		umask(fmask);
	}
	return rc;
}
int restore_file(const char* const file_path) {
	int fd;
	int rc;
	char iobuf[4096];
	fd = open(file_path, O_RDONLY);
	if (fd <= 0) {
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
		close(fd	);
		}
	return rc;
}

int validate_file(const char* const file_path) {
	int rc;
	rc = 0;
	return rc;
}

void file_functor(struct file_func_ctx* fctx) {
	int rc;
	struct hsm_user_state hus;
	rc = llapi_hsm_state_get(fctx->file_path, &hus);
	if (rc != 0) {
		zlog_error(zctx_log,"ERROR: llapi fail errno %d for %s", rc, fctx->file_path);
		fctx->failed = rc;
	} else if (hus.hus_states & HS_RELEASED) {
		if ((rc = restore_file(fctx->file_path)) != 0) {
			zlog_warn(zctx_log, "LOST: restore fail errno %d for %s",
				rc, fctx->file_path);
			fctx->failed = 1;
		} else {
			zlog_info(zctx_log,"RESTORED: %s", fctx->file_path);
		}
	} else {
		zlog_info(zctx_log,"OK: %s", fctx->file_path);
	}
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
				pthread_mutex_lock( &thrd_mtx );
				zlog_info(zctx_dbg, "hsmwalk found released: %s", path);
				struct ctx_worker_thread* \
					pwctx = find_idle();
				if (NULL == pwctx) {
					zlog_error(zctx_dbg, "ERROR: No idle threads found");
				} else {
					struct file_func_ctx* pfctx = &pwctx->fctx;
					strncpy(pfctx->file_path, path, sizeof(pfctx->file_path));
					pwctx->tstate = ctx_work;
				}
				pthread_mutex_unlock( &thrd_mtx );
			}
		}
	}
	closedir(dir);
}

int main(int argc, char** argv) {
	int rc;
	srand(time(NULL));
	if ((rc = confirm_conf_file()) != 0) {
		fprintf(stderr, "Failed to create %s\n", zlog_conf_file);
		return 1;
	}
	rc = zlog_init(zlog_conf_file);
	if (rc) {
		fprintf(stderr, "zlog init failed with errno %d while opening %s\n",\
				rc, zlog_conf_file);
		return 1;
	}
	zctx_dbg = zlog_get_category(zlog_category_dbg);
	zctx_log = zlog_get_category(zlog_category_log);
	if (NULL == zctx_dbg) {
		fprintf(stderr, "zlog failed to get category %s from %s\n",\
				zlog_category_dbg, zlog_conf_file);
	}
	//rc = run_self_test();
	//run_as_thread(test_functor, "Hello, from message");
	//printf("run_self_test = %d\n", rc);
	zlog_info(zctx_dbg, "BEGIN: main");
	threads_init(thread_count, file_functor);
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
	threads_halt();
	zlog_info(zctx_dbg, "EXIT: main");
	zlog_fini();
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
