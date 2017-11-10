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
 * =============================================================================
 *
 *       Filename:  lhsm-restore-stub.h
 *
 *    Description:  Defines stub functions to emulate lhsm-restore timing.
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
#ifndef LHSM_RESTORE_STUB_H
#define LHSM_RESTORE_STUB_H
#include <stdlib.h>
#include <sys/types.h>
#include <linux/types.h>
#include "md5.h"

struct md5result {
	char str[34];
};

/** HSM per-file state
 * See HSM_FLAGS below.
 */
enum hsm_states {
	HS_NONE      = 0x00000000,
	HS_EXISTS    = 0x00000001,
	HS_DIRTY     = 0x00000002,
	HS_RELEASED  = 0x00000004,
	HS_ARCHIVED  = 0x00000008,
	HS_NORELEASE = 0x00000010,
	HS_NOARCHIVE = 0x00000020,
	HS_LOST      = 0x00000040,
};

struct hsm_extent {
	__u64 offset;
	__u64 length;
} __attribute__((packed));

/**
 * Current HSM states of a Lustre file.
 *
 * This structure purpose is to be sent to user-space mainly. It describes the
 * current HSM flags and in-progress action.
 */
struct hsm_user_state {
	__u32			hus_states;
	__u32			hus_archive_id;
	__u32			hus_in_progress_state;
	__u32			hus_in_progress_action;
	struct hsm_extent	hus_in_progress_location;
	char			hus_extended_info[];
};

int llapi_hsm_state_get(const char *path, struct hsm_user_state *hus);
void str2md5(const char *str, int length, struct md5result* pres);

#endif // LHSM_RESTORE_STUB_H
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab


