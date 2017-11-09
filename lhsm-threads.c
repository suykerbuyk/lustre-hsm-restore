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
static struct ctx_worker_thread *worker_threads;
const  struct timespec poll_time = {0,100000000L};
int    thread_count = 16;

/* Worker thread function to restore a file */
static void* run_thread_ctx(void* param) {
	int rc;
	struct ctx_worker_thread* pctx = (struct ctx_worker_thread*) param;
	pctx->tstate = ctx_idle;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	zlog_info(zctx_dbg, "ENTER: run_restore_ctx");
	while (!shutdown_flag) {
		while(pctx->tstate == ctx_idle) {
			rc = nanosleep(&poll_time, NULL);
			if (rc != 0) {
				zlog_warn(zctx_dbg, "run_restore_ctx: Cancel signaled!");
				shutdown_flag = 1;
				break;
			}
		}
		if (shutdown_flag) {
			zlog_warn(zctx_dbg, "run_restore_ctx SHUTDOWN FLAG IS SET!");
			break;
		}
		zlog_info(zctx_dbg, "run_restore_ctx: calling functor");
		pctx->pfunc(&pctx->fctx);
		zlog_info(zctx_dbg, "run_thread_ctx functor returned.");
		pctx->tstate = ctx_idle;
	}
	pctx->tstate = ctx_dead;
	zlog_info(zctx_dbg, "EXIT: run_restore_ctx");
	pthread_exit(pctx);
	return(NULL);
}


int threads_init(int threads, file_functor_ptr pfunc) {
	int rc;
	int idx = 0;
	struct ctx_worker_thread* pctx;
	zlog_info(zctx_dbg, "BEGIN: threads_init");
	worker_threads =\
		calloc(thread_count , sizeof(struct ctx_worker_thread));
	if (NULL == worker_threads) {
		zlog_error(zctx_dbg, "Could not allocate memory for thread contexts.");
		return(ENOMEM);
	}
	while (idx < threads) {
		pctx = &worker_threads[idx];
		pctx->pfunc = pfunc;
		pctx->tstate = ctx_dead;
		if ((rc = pthread_create(&pctx->tcb, NULL,\
				run_thread_ctx,  &worker_threads[idx] )) != 0) {
			zlog_error(zctx_dbg, "error %d launching thread %d", rc, idx);
			return(rc);
		}
		zlog_info(zctx_dbg, "Launched thread %d", idx);
		idx++;
	}
	zlog_info(zctx_dbg, "EXIT: threads_init Launched %d threads", idx);
	return(0);
}

void threads_halt(void) {
	int idx = 0;
	zlog_info(zctx_dbg, "ENTER: threads_halt");
	/* tell the threads it's time to quit */
	shutdown_flag = 1;
	nanosleep(&poll_time, NULL);
	/* send the cancel signal, will be caught by nanosleep */
	while (idx < thread_count) {
		pthread_cancel(worker_threads[idx].tcb);
		nanosleep(&poll_time, NULL);
		idx++;
	}
	idx = 0;
	/* Join each thread as they shutdown */
	while (idx < thread_count) {
		zlog_info(zctx_dbg, "Joining thread %d", idx);
		pthread_join(worker_threads[idx].tcb,NULL);
		idx++;
	}
	zlog_info(zctx_dbg, "EXIT: threads_halt");
}
struct ctx_worker_thread* find_idle(void) {
	static int idx=0;
	zlog_info(zctx_dbg,"ENTER: find_idle");
	while(1) {
		if (worker_threads[idx].tstate == ctx_idle) {
			zlog_info(zctx_dbg, "EXIT: returning idle thread %d", idx);
			return (&worker_threads[idx]);
		}
		if (idx == (thread_count-1)) {
			if (nanosleep(&poll_time, NULL) < 0) {
				zlog_warn(zctx_dbg, "signaled while looking for idle threads.");
				break;
			}
			zlog_debug(zctx_dbg, "No idle threads found.  Still searching.");
		}
		idx++;
		if (idx >= thread_count)
			idx=0;
	}
	zlog_error(zctx_dbg, "EXIT: FAILED to find idle thread");
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
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
