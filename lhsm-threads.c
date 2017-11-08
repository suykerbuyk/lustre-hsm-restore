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
 *       Filename:  lhsm-threads.c
 *
 *    Description:  impliments threaded functions for hsm restore.
 *
 *        Version:  1.0
 *        Created:  11/05/2017 07:34:42 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  John Suykerbuyk
 *   Organization:  Seagate PLC
 *
 * =============================================================================
 */

#define _GNU_SOURCE 1
#define __SIGNED__

#include <time.h>
#include <pthread.h>
#include "lhsm-threads.h"
#include "lhsm-restore-stub.h"

/* global shutdown flag */
static int volatile shutdown_flag = 0;

/* List of worker thread contextx */
static struct ctx_hsm_restore_thread *hsm_worker_threads;
const  struct timespec poll_time = {0,100000000L};
int    thread_count = 16;
const  char* const zlog_category = "lhsm_log";
const  char* const zlog_conf_file = "lhsm-restore.conf";
zlog_category_t* zctx = NULL;

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
				//for(int i =0; i < 50000; i++) {
					str2md5(iobuff, sizeof(iobuff), &md5str);
				//}
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

void str2md5(const char *str, int length, struct md5result* pres) {
	int n;
	MD5_CTX c;
	unsigned char digest[16];
	char *out = pres->str;

	MD5_Init(&c);

	while (length > 0) {
		if (length > 512) {
			MD5_Update(&c, str, 512);
		} else {
			MD5_Update(&c, str, length);
		}
		length -= 512;
		str += 512;
		}

	MD5_Final(digest, &c);

	for (n = 0; n < 16; ++n) {
		snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
	}
}

void run_as_thread(file_functor_ptr function, void* functor_ctx) {
	function(functor_ctx);
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
