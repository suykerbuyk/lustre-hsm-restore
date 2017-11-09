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

struct file_func_ctx {
	char file_path[PATH_MAX];
	int  failed;
};

/* generic function prototype for thread to run. */
typedef void(*file_functor_ptr)(struct file_func_ctx*);

/* state of the running context */
enum ctx_tstate {
	ctx_dead   = -1, /*  Can only be set by worker thread */
	ctx_idle   =  0, /*  Can only be set by worker thread */
	ctx_work   =  1  /*  Can only be set by parent thread */
};

struct ctx_worker_thread {
	pthread_t             tcb;
	enum ctx_tstate       tstate;
	file_functor_ptr      pfunc;
	struct file_func_ctx  fctx;
	int                   thrdid;
};


/* zlog category context.  We only use one for this */
extern zlog_category_t* zctx_log;
extern zlog_category_t* zctx_dbg;

/* Category of logging definitions */
extern const char* const zlog_category_log;
extern const char* const zlog_category_dbg;

/* The config file that controls logging  */
extern const char* const zlog_conf_file;

int  threads_init(int threads, file_functor_ptr pfunc);
void threads_halt(void);
struct ctx_worker_thread* find_idle(void);


#endif // LHSM_THREADS_H
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
