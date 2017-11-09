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
 *       Filename:  lhsm-threads.h
 *
 *    Description:  defines threaded functions for hsm restore.
 *
 *        Version:  1.0
 *        Created:  11/05/2017 06:51:38 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  John Suykerbuyk
 *   Organization:  Seagate PLC
 *
 * ============================================================================
 */
#ifndef LHSM_THREADS_H
#define LHSM_THREADS_H
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <linux/types.h>
#include "zlog/src/zlog.h"

/* time spec for polling set to 1/10th of one second */
extern const struct timespec poll_time;

/* Number of worker threads to spawn */
extern int thread_count;



/* state context's file */
enum ctx_fstate {
	ctx_unknown   = 0,
	ctx_found     = 1,
	ctx_lost      = 2,
	ctx_recovered = 3,
	ctx_open_fail = 4
};

/* state of the running context */
enum ctx_tstate {
	ctx_dead   = -1, /*  Can only be set by worker thread */
	ctx_idle   =  0, /*  Can only be set by worker thread */
	ctx_work   =  1  /*  Can only be set by parent thread */
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


/* zlog category context.  We only use one for this */
extern zlog_category_t* zctx_log;
extern zlog_category_t* zctx_dbg;
/* Category of logging definitions */
extern const char* const zlog_category_log;
extern const char* const zlog_category_dbg;
/* The config file that controls logging  */
extern const char* const zlog_conf_file;

int restore_threads_init(int threads);
void restore_threads_halt(void);
struct ctx_hsm_restore_thread* restore_threads_find_idle(void);

/* function run by a worker thread */
void* run_restore_ctx(void* context);

typedef void(*file_functor_ptr)(void*);

void run_as_thread(file_functor_ptr function, void* functor_ctx);

#endif // LHSM_THREADS_H
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
