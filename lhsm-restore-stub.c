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
 *       Filename:  lshm-restore-stub.c
 *
 *    Description:  Impliments stub functions to emulate lhsm-restore timing.
 *
 *        Version:  1.0
 *        Created:  11/05/2017 11:11:54 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  John Suykerbuyk
 *   Organization:  Seagate PLC
 *
 * ============================================================================
 */
#include "lhsm-restore-stub.h"

/* stub function to fake a real call to llapi api's */
int llapi_hsm_state_get(const char *path, struct hsm_user_state *hus) {
	if (rand() % 16 > 8)
		hus->hus_states  = HS_RELEASED | HS_ARCHIVED;
	else
		hus->hus_states  = HS_EXISTS | HS_ARCHIVED;
	return 0;
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab


