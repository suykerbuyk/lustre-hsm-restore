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
 *       Filename:  lhsm-restore-mfid.h
 *
 *    Description:  Handles esoteric FID data structures.
 *
 *        Version:  1.0
 *        Created:  11/07/2017 03:38:26 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 */

#ifndef lhsm_restore_mfid_h
#define lhsm_restore_mfid_h
#include <sys/types.h>

extern const char* const file_ea_name;

struct m0_fid;
struct ml_ct_fs_ea;
size_t size_of_m0_fid(void);

#endif // lhsm_restore_mfid_h
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81

