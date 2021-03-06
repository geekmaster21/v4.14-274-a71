/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _IOT_SIM_DEFS_I_H_
#define _IOT_SIM_DEFS_I_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_list.h>
#include <qdf_util.h>
#include <qdf_types.h>
#include <qdf_timer.h>
#include <wbuf.h>
#include <wlan_iot_sim_utils_api.h>

#define iot_sim_fatal(format, args...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_err(format, args...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_warn(format, args...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_info(format, args...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_debug(format, args...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_IOT_SIM, format, ## args)

#define iot_sim_fatal_nofl(format, args...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_err_nofl(format, args...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_warn_nofl(format, args...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_info_nofl(format, args...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_debug_nofl(format, args...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)

#define iot_sim_fatal_rl(format, args...) \
	QDF_TRACE_FATAL_RL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_err_rl(format, args...) \
	QDF_TRACE_ERROR_RL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_warn_rl(format, args...) \
	QDF_TRACE_WARN_RL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_info_rl(format, args...) \
	QDF_TRACE_INFO_RL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_debug_rl(format, args...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_IOT_SIM, format, ## args)

#define iot_sim_fatal_rl_nofl(format, args...) \
	QDF_TRACE_FATAL_RL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_err_rl_nofl(format, args...) \
	QDF_TRACE_ERROR_RL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_warn_rl_nofl(format, args...) \
	QDF_TRACE_WARN_RL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_info_rl_nofl(format, args...) \
	QDF_TRACE_INFO_RL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)
#define iot_sim_debug_rl_nofl(format, args...) \
	QDF_TRACE_DEBUG_RL_NO_FL(QDF_MODULE_ID_IOT_SIM, format, ## args)

#define N_FRAME_TYPE 0x4
#define N_FRAME_SUBTYPE 0xF
#define MAX_SEQ 0x4
#define MAX_ACTION 0x3
#define IOT_SIM_DEBUGFS_FILE_NUM 3
#define FRAME_TYPE_IS_BEACON(type, subtype) ((type) == 0 && (subtype) == 8)
#define FRAME_TYPE_IS_ACTION(type, subtype) ((type) == 0 && (subtype) == 13)

enum iot_sim_action_cat_type {
	CAT_BA,
	CAT_SA_QUERY,
	IOT_SIM_MAX_CAT,
};

/*
 * struct iot_sim_debugfs - contains dentry pointer for opened
 *			    iot sim files and directory
 * @iot_sim_dir_de - dentry pointer to pdev specific folder
 * @iot_sim_file_de - dentry pointer representing operation specific files
 */
struct iot_sim_debugfs {
	struct dentry *iot_sim_dir_de;
	struct dentry *iot_sim_file_de[IOT_SIM_DEBUGFS_FILE_NUM];
};

/*
 * struct iot_sim_rule - represent user configured rules
 * @len - Length of the content provided by user
 * @offset - offset at which modification done in capture frame
 * @frm_content - actual user data in hex
 * @drop - frame marked for drop
 * @delay_dur - duration of delay
 */
struct iot_sim_rule {
	uint16_t len;
	uint16_t offset;
	uint8_t *frm_content;
	bool drop;
	uint16_t delay_dur;
	uint8_t rule_bitmap;
};

/*
 * struct iot_sim_rule_per_seq - rule structure per sequence iot sim files
 *				 and directory
 *
 * @rule_per_type - 2d array of iot_sim_rule per type subtype
 * @use_count - usage reference
 */
struct iot_sim_rule_per_seq {
	struct iot_sim_rule *rule_per_type[N_FRAME_TYPE][N_FRAME_SUBTYPE];
	struct iot_sim_rule *rule_per_action_frm[IOT_SIM_MAX_CAT][MAX_ACTION];
	uint8_t use_count;
};

/*
 * struct iot_sim_rule_per_peer - peer specific structure for iot sim ops
 *
 * @addr - address of peer
 * @iot_sim_lock - spinlock
 * @rule_per_seq - array of iot_sim_rule_per_seq
 * @p_list - list variable
 */
struct iot_sim_rule_per_peer {
	struct qdf_mac_addr addr;
	qdf_spinlock_t iot_sim_lock;
	struct iot_sim_rule_per_seq *rule_per_seq[MAX_SEQ];
	qdf_list_t p_list;
};

/**
 * struct iot_sim_context - iot_sim pdev private object
 * @pdev_obj:Reference to pdev global object
 * @iot_sim_peer_list: peer list for peer specific rules
 * @bcast_peer: broadcast peer entry for storing rules for all peers
 * @p_iot_sim_target_handle: handle to iot_sim target_if
 * @iot_sim_operation_handler: callback for iot sim operation handler
 */
struct iot_sim_context {
	struct wlan_objmgr_pdev *pdev_obj;
	/* IOT_SIM Peer list & Bcast Peer */
	struct iot_sim_rule_per_peer *iot_sim_peer_list, bcast_peer;
	void *p_iot_sim_target_handle;
	struct iot_sim_debugfs iot_sim_dbgfs_ctx;
	QDF_STATUS (*iot_sim_operation_handler)(struct wlan_objmgr_pdev *pdev,
						wbuf_t wbuf);
};

/* enum iot_sim_operations - iot sim operations
 *
 * @INVALID_OPERATION - invalid operation
 * @CONTENT_CHANGE - Frame Content Change operation
 * @DROP - Frame drop operation
 * @DELAY - Frame delay operation
 * @IOT_SIM_MAX_OPERATION - iot sim max operation
 */
enum iot_sim_operations {
	INVALID_OPERATION,
	CONTENT_CHANGE = 1,
	DROP,
	DELAY,
	IOT_SIM_MAX_OPERATION
};

#endif /* _IOT_SIM_DEFS_I_H_ */
