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
 *
 *       Filename:  lhsm-restore-test.c
 *
 *    Description:  Generates test directory for lhsm-restore test
 *
 *        Version:  1.0
 *        Created:  11/06/2017 06:06:26 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  John Suykerbuyk
 *   Organization:  Seagate PLC
 *
 */
#define _GNU_SOURCE 1
#define __SIGNED__

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char* const filename_pattern = "%s/test_file_0x%04x.dat";
static const char* const directory_pattern= "%s/dir_0x%02x-0x%04x";
static const int sparse_file_size = 1024 * 1024;

static int get_or_make_dir(const char* const dirpath) {
	int rc;
	DIR* pdir;
	errno = 0;
	int default_dmask;
	if ((NULL == dirpath) || (*dirpath == 0))
		return EINVAL;
	pdir = opendir(dirpath);
	rc = errno;
	closedir(pdir);
	if ((NULL == pdir) && (ENOENT == rc)) {
		default_dmask = umask(0);
		rc = mkdir (dirpath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		umask(default_dmask);
	}
	return rc;
}

static int generate_testdir
(const char* const topdir, size_t max_depth,
 size_t subdirs_count, size_t file_count) {
	int rc;
	int fd;
	int len;
	int fmask;
	char dirpath[PATH_MAX];
	char next_path[PATH_MAX];
	char* eop = dirpath;

	if (NULL == topdir)
		return EINVAL;
	if (0 == *topdir)
		return EINVAL;
	eop = stpncpy(dirpath, topdir, sizeof(dirpath));
	len = eop-dirpath;
	/* trim trailing slash */
	if ((len > 1) && (*(eop-1) == '/'))
		*(--eop) = 0;
	if ( 0 != (rc=get_or_make_dir(dirpath)))
		return rc;
	for(unsigned int subdir_num=0;
			subdir_num < subdirs_count;
			subdir_num++) {
		snprintf(next_path, sizeof(next_path),
				directory_pattern, dirpath, (unsigned) max_depth, subdir_num);
		if (max_depth > 0)
			rc = generate_testdir(next_path, max_depth -1,
				subdirs_count, file_count);
		if (rc)
			break;
	}
	fmask=umask(0);
	for (unsigned int filenum = 0;
			(filenum < file_count);
			filenum++) {
		snprintf(next_path, sizeof(next_path),
				filename_pattern, dirpath, filenum);
		fd = open(next_path, O_RDWR | O_CREAT,
				S_IRUSR | S_IWUSR |
				S_IRGRP | S_IWGRP |
				S_IROTH | S_IWOTH );
		if (fd < 0) {
			rc = errno;
			break;
		}
		close(fd);
		if (truncate(next_path, sparse_file_size) < 0) {
			rc = errno;
			break;
		}
	}
	umask(fmask);
	return rc;
}

int run_self_test(void) {
	int rc;
	rc = 0;
	const char* const topdir = "test";
	const size_t dir_depth = 5;
	const size_t dir_count = 3;
	const size_t file_count = 10;
	rc =generate_testdir(topdir, dir_depth, dir_count, file_count);
	return(rc);
}

// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
