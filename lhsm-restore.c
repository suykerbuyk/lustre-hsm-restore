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
 * =====================================================================================
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
 * =====================================================================================
 */

#define _GNU_SOURCE 1
#define __SIGNED__



#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <pthread.h>
#include "zlog/src/zlog.h"
#include <linux/types.h>

#ifdef REAL_LUSTRE
#include <lustre/lustreapi.h>
#else
#include "lhsm-restore-stub.h"

/* stub function to fake a real call to llapi api's */
int llapi_hsm_state_get(const char *path, struct hsm_user_state *hus) {
	if (rand() % 16 > 8)
		hus->hus_states  = HS_RELEASED | HS_ARCHIVED;
	else
		hus->hus_states  = HS_EXISTS | HS_ARCHIVED;
	return 0;
}
#endif


/* time spec for polling set to 1/10th of one second */
const struct timespec poll_time = {0,100000000L};

/* sum tally of all files */
unsigned long long file_size_tally = 0;

/* Number of worker threads to spawn */
static const int thread_count = 16;

/* List of worker thread contextx */
static struct ctx_hsm_restore_thread *hsm_worker_threads;

/* function run by a worker thread */
void* run_restore_ctx(void* context);

/* state of the running context */
enum ctx_tstate {
	ctx_dead   = -1, /*  Can only be set by worker thread */
	ctx_idle   =  0, /*  Can only be set by worker thread */
	ctx_work   =  1  /*  Can only be set by parent thread */
};

/* global shutdown flag */
static int volatile shutdown_flag = 0;

/* zlog category context.  We only use one for this */
static zlog_category_t* zctx = NULL;
/* Category of logging definitions */
const char* const zlog_category = "lhsm_log";
/* The config file that controls logging  */
const char* const zlog_conf_file = "lhsm-restore.conf";

/* state context's file */
enum ctx_fstate {
	ctx_unknown   = 0,
	ctx_found     = 1,
	ctx_lost      = 2,
	ctx_recovered = 3,
	ctx_open_fail = 4
};

/* The operational context of a worker thread */
struct ctx_worker {
	enum ctx_tstate tstate; /* running state of thread context, set by worker */
	enum ctx_fstate fstate; /* state of recovered file, set by worker         */
	int  error;             /* errno of last failed call                      */
	char path[PATH_MAX];    /* File path to restore                           */
};
struct ctx_hsm_restore_thread {
	pthread_t         tcb;
	struct ctx_worker ctx;
};
int restore_threads_init(int threads) {
	int rc;
	int idx = 0;
	pthread_t* ptcb;
	struct ctx_worker* pctx;
	zlog_info(zctx, "BEGIN: restore_threads_init");
	hsm_worker_threads =\
		calloc(thread_count , sizeof(struct ctx_hsm_restore_thread));
	if (NULL == hsm_worker_threads) {
		zlog_error(zctx, "Could not allocate memory for thread contexts.");
		return(ENOMEM);
	}
	while (idx < threads) {
		ptcb=&(hsm_worker_threads[idx].tcb);
		pctx=&(hsm_worker_threads[idx].ctx);
		if ((rc = pthread_create(ptcb, NULL, run_restore_ctx, pctx )) != 0) {
			zlog_error(zctx, "error %d launching thread %d", rc, idx);
			return(rc);
		}
		zlog_info(zctx, "Launched thread %d", idx);
		idx++;
	}
	zlog_info(zctx, "EXIT: restore_theads_init Launched %d threads", idx);
	return(0);
}
void restore_threads_halt(void) {
	int idx = 0;
	zlog_info(zctx, "ENTER: restore_threads_halt");
	/* tell the threads it's time to quit */
	shutdown_flag = 1;
	nanosleep(&poll_time, NULL);
	/* send the cancel signal, will be caught by nanosleep */
	while (idx < thread_count) {
		pthread_cancel(hsm_worker_threads[idx].tcb);
		nanosleep(&poll_time, NULL);
		idx++;
	}
	idx = 0;
	/* Join each thread as they shutdown */
	while (idx < thread_count) {
		zlog_info(zctx, "Joining thread %d", idx);
		pthread_join(hsm_worker_threads[idx].tcb,NULL);
		idx++;
	}
	zlog_info(zctx, "EXIT: restore_threads_halt");
}
struct ctx_hsm_restore_thread* restore_threads_find_idle(void) {
	static int idx=0;
	zlog_info(zctx,"ENTER: restore_threads_find_idle");
	struct ctx_worker* pctx = &hsm_worker_threads[idx].ctx;
	while(1) {
		if (pctx->tstate == ctx_idle) {
			zlog_info(zctx, "EXIT: returning idle thread %d", idx);
			return (&hsm_worker_threads[idx]);
		}
		if (idx == (thread_count-1)) {
			nanosleep(&poll_time, NULL);
			zlog_debug(zctx, "No idle threads found.  Still searching.");
		}
		idx++;
		if (idx >= thread_count)
			idx=0;
		pctx = &hsm_worker_threads[idx].ctx;
	}
	zlog_error(zctx, "EXIT: FAILED to find idle thread");
	return(NULL);
}
/* Worker thread function to restore a file */
void* run_restore_ctx(void* context) {
	int rc;
	struct ctx_worker* pctx = (struct ctx_worker*) context;
	char  iobuff[4096];
	int   fd;
	pctx->tstate = ctx_idle;
	struct md5result md5str;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	zlog_info(zctx, "ENTER: run_restore_ctx");
	while (!shutdown_flag) {
		pctx->fstate=ctx_unknown;
		while(pctx->tstate == ctx_idle) {
			rc = nanosleep(&poll_time, NULL);
			if (rc != 0) {
				zlog_warn(zctx, "run_restore_ctx: Cancel signaled!");
				shutdown_flag = 1;
				break;
			}
		}
		zlog_info(zctx, "run_restore_ctx has path %s", pctx->path);
		if (shutdown_flag) {
			zlog_warn(zctx, "run_restore_ctx SHUTDOWN FLAG IS SET!");
			break;
		}
		fd = open(pctx->path, O_RDONLY);
		if (fd == 0) {
			pctx->error = errno;
			pctx->fstate = ctx_open_fail;
		} else {
			rc = read(fd, &iobuff, sizeof(iobuff));
			if (rc < 0) {
				pctx->error = errno;
				pctx->fstate = ctx_lost;
			} else {
				for(int i =0; i < 50000; i++) {
					str2md5(iobuff, sizeof(iobuff), &md5str);
				}
				pctx->fstate = ctx_recovered;
			}
		close(fd);
		fprintf(stdout, "%s %s\n", md5str.str, pctx->path);
		}
		pctx->tstate = ctx_idle;
	}
	pctx->tstate = ctx_dead;
	zlog_info(zctx, "EXIT: run_restore_ctx");
	pthread_exit(context);
	return(NULL);
}

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
