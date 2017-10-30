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


//#include <lustre/lustreapi.h>
#include <linux/types.h>
#include "lhsm-restore-stub.h"

int llapi_hsm_state_get(const char *path, struct hsm_user_state *hus) {
	hus->hus_in_progress_state = HS_RELEASED;
	return 0;
}


/* time spec for polling set to 1/10th of one second */
const struct timespec poll_time = {0,100000000L};
/* sum tally of all files */
unsigned long long file_size_tally = 0;
/* Number of worker threads to spawn */
static const int thread_count = 4;
static struct ctx_hsm_restore_thread *hsm_worker_threads;

void* run_restore_ctx(void* context);

/* state of the running context */
enum ctx_tstate {
	ctx_dead   = -1, /*  Can only be set by worker thread */
	ctx_idle   =  0, /*  Can only be set by worker thread */
	ctx_work   =  1  /*  Can only be set by parent thread */
};

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
	int  shutdown_flag;     /* Set only by parent OR if interrupted           */
	char path[PATH_MAX];    /* File path to restore                           */
};
struct ctx_hsm_restore_thread {
	pthread_t         tcb;
	struct ctx_worker ctx;
};
int restore_threads_init(int threads) {
	int rc;
	int idx = 0;
	struct ctx_hsm_restore_thread* prthrd;
	hsm_worker_threads = calloc(thread_count , sizeof(struct ctx_hsm_restore_thread));
	if (NULL == hsm_worker_threads)
		return(ENOMEM);
	while (idx < threads) {
		prthrd=&(hsm_worker_threads[idx]);
		if ((rc = pthread_create(&(prthrd->tcb), NULL, run_restore_ctx, &prthrd->ctx )) != 0) {
			return(rc);
		}
		idx++;
	}
	return(0);
}
void restore_threads_halt(void) {
	int idx = 0;
	fprintf(stderr, "Shutting down worker threads\n");
	while (idx < thread_count) {
		hsm_worker_threads[idx].ctx.shutdown_flag = 1;
		nanosleep(&poll_time, NULL);
		pthread_cancel(hsm_worker_threads[idx].tcb);
	}
	idx = 0;
	while (idx < thread_count) {
		hsm_worker_threads[idx].ctx.shutdown_flag = 1;
		nanosleep(&poll_time, NULL);
		pthread_cancel(hsm_worker_threads[idx].tcb);
		idx++;
	}
	idx = 0;
	while (idx < thread_count) {
		fprintf(stderr, "Joining thread %d\n", idx);
		pthread_join(hsm_worker_threads[idx].tcb,NULL);
		idx++;
	}
}
struct ctx_hsm_restore_thread* restore_threads_find_idle(struct ctx_hsm_restore_thread** previous) {
	return(NULL);
}
/* Worker thread function to restore a file */
void* run_restore_ctx(void* context) {
	int rc;
	struct ctx_worker* pctx = (struct ctx_worker*) context;
	char  iobuff[4096];
	int   fd;
	pctx->tstate = ctx_idle;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	while (0 == pctx->shutdown_flag) {
		pctx->fstate=ctx_unknown;
		while(pctx->tstate == ctx_idle) {
			rc = nanosleep(&poll_time, NULL);
			if (rc != 0) {
				/* We recieved a interrupt */
				int idx=0;
				while (idx < thread_count) {
					hsm_worker_threads[idx].ctx.shutdown_flag = 1;
				}
				break;
			}
		}
		if (0 != pctx->shutdown_flag)
			break;
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
				pctx->fstate = ctx_recovered;
			}
		close(fd);
		}
		pctx->tstate = ctx_idle;
	}
	pctx->tstate = ctx_dead;
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
	}

	while ((entry = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		char filebuff[4096];
		struct hsm_user_state hus;
		int rc;
		int released;

		if (*entry->d_name =='/') 
			snprintf(path, sizeof(path), "%s%s", name, entry->d_name);
		else
			snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
		if (entry->d_type == DT_DIR) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			hsm_walk_dir(path);
		} else if (entry->d_type == DT_REG) {
			rc = llapi_hsm_state_get(path, &hus);
			if (rc != 0) {
				printf("Error: get hsm state rc %d errno %d for %s\n", rc, errno, path);
				break;
			} else {
				//printf("Got hsm state for %s\n", path);
			}
			released = hus.hus_states & HS_RELEASED;
			if (released) {
				int fd = open(path, O_RDONLY);
				if (fd == 0) {
					fprintf(stderr,"FailOpen: %d %s\n", errno, path);
				} else {
					printf("Recover: %s\n", path);
					rc = read(fd, &filebuff, sizeof(filebuff));
					if (rc < 0) { 
						fprintf(stderr,"Lost: %d %s\n", errno, path); 
					} else {
						fprintf(stdout,"Recovered: %s \n", path);
					}
				close(fd);
				}
			} else {
				fprintf(stdout, "Found: %s\n", path);
			}
		}
	}
	closedir(dir);
}

int main(int argc, char** argv) {
	if (argc >1) {
		char* path=argv[1];
		size_t plen=strlen(path);
		/* trim trailing path delimiter if provided */
		if ((plen > 0) && (path[plen-1] == '/')) {
			plen--;
			path[plen] = 0;
		}
		hsm_walk_dir(path);
	}
	else
	   hsm_walk_dir(".");
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab
