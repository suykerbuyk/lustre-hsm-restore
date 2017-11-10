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

const char* const file_ea_name = "user.mlhsm_archive_fid";

typedef struct {
	uint32_t   time_low;
	uint16_t   time_mid;
	uint16_t   time_hi_and_version;
	uint8_t    clock_seq_hi_and_reserved;
	uint8_t    clock_seq_low;
	uint8_t    node[6];
} __attribute__ ((packed)) uuid_t;

struct m0_fid {
	uint64_t f_container;
	uint64_t f_key;
};

struct  m0_fsid {
	uint64_t p1;
	uint64_t p2;
};

struct ml_ct_fs_ea {
	struct m0_fid mfid;
	struct m0_fsid fsid;
}PACKED;


void ml_ct_ea_to_be(struct ml_ct_fs_ea *fs_attr) {
	fs_attr->mfid.f_container = __cpu_to_be64(fs_attr->mfid.f_container);
	fs_attr->mfid.f_key = __cpu_to_be64(fs_attr->mfid.f_key);
}

void ml_ct_ea_to_cpu(struct ml_ct_fs_ea *fs_attr) {
	fs_attr->mfid.f_container = __be64_to_cpu(fs_attr->mfid.f_container);
	fs_attr->mfid.f_key = __be64_to_cpu(fs_attr->mfid.f_key);
}

size_t size_of_m0_fid(void) {
	return sizeof(struct m0_fid);
}
// vim: tabstop=4 shiftwidth=4 softtabstop=4 smarttab colorcolumn=81
