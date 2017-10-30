/*
 * =====================================================================================
 *
 *       Filename:  lhsm-restore-stub.h
 *
 *    Description:  stubbed lustre functions for testing on non-lustre clients.
 *
 *        Version:  1.0
 *        Created:  10/30/2017 04:30:39 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

/** HSM per-file state
 * See HSM_FLAGS below.
 */
enum hsm_states {
	HS_NONE		= 0x00000000,
	HS_EXISTS	= 0x00000001,
	HS_DIRTY	= 0x00000002,
	HS_RELEASED	= 0x00000004,
	HS_ARCHIVED	= 0x00000008,
	HS_NORELEASE	= 0x00000010,
	HS_NOARCHIVE	= 0x00000020,
	HS_LOST		= 0x00000040,
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

