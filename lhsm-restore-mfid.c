/*
 *
 *       Filename:  lhsm-restore-mfid.c
 *
 *    Description:  impliments mfid handling
 *
 *        Version:  1.0
 *        Created:  11/07/2017 03:45:34 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 */
#include "lhsm-restore-mfid.h"

#include <asm/byteorder.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

const char* const file_ea_name = "user.mlhsm_archive_fid";

struct m0_fid {
	uint64_t f_container;
	uint64_t f_key;
};

struct ml_ct_fs_ea {
	struct m0_fid fid;
	uuid_t fsid;
}PACKED;


void ml_ct_ea_to_be(struct ml_ct_fs_ea *fs_attr) {
	fs_attr->fid.f_container = __cpu_to_be64(fs_attr->fid.f_container);
	fs_attr->fid.f_key = __cpu_to_be64(fs_attr->fid.f_key);
}

void ml_ct_ea_to_cpu(struct ml_ct_fs_ea *fs_attr) {
	fs_attr->fid.f_container = __be64_to_cpu(fs_attr->fid.f_container);
	fs_attr->fid.f_key = __be64_to_cpu(fs_attr->fid.f_key);
}

size_t size_of_m0_fid(void) {
	return sizeof(struct m0_fid);
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81

