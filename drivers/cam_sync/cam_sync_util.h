/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_SYNC_UTIL_H__
#define __CAM_SYNC_UTIL_H__


#include "cam_sync_private.h"
#include "cam_debug_util.h"

extern struct sync_device *sync_dev;

/**
 * @brief: Finds an empty row in the sync table and sets its corresponding bit
 * in the bit array
 *
 * @param sync_dev : Pointer to the sync device instance
 * @param idx      : Pointer to an long containing the index found in the bit
 *                   array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_util_find_and_set_empty_row(struct sync_device *sync_dev,
	long *idx);

/**
 * @brief: Function to initialize an empty row in the sync table. This should be
 *         called only for individual sync objects.
 *
 * @param table : Pointer to the sync objects table
 * @param idx   : Index of row to initialize
 * @param name  : Optional string representation of the sync object. Should be
 *                63 characters or less
 * @param type  : type of row to be initialized
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_init_row(struct sync_table_row *table,
	uint32_t idx, const char *name, uint32_t type);

/**
 * @brief: Function to uninitialize a row in the sync table
 *
 * @param table : Pointer to the sync objects table
 * @param idx   : Index of row to initialize
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_deinit_object(struct sync_table_row *table, uint32_t idx);

/**
 * @brief: Function to re-initialize a row in the sync table
 *
 * @param table : Pointer to the sync objects table
 * @param idx   : Index of row to initialize
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_reinit_object(struct sync_table_row *table, uint32_t idx);

/**
 * @brief: Function to initialize a row in the sync table when the object is a
 *         group object, also known as a merged sync object
 *
 * @param table     : Pointer to the sync objects table
 * @param idx       : Index of row to initialize
 * @param sync_objs : Array of sync objects which will merged
 *                    or grouped together
 * @param num_objs  : Number of sync objects in the array
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_sync_init_group_object(struct sync_table_row *table,
	uint32_t idx,
	uint32_t *sync_objs,
	uint32_t num_objs);

int cam_sync_deinit_object(struct sync_table_row *table, uint32_t idx);

/**
 * @brief: Function to dispatch a kernel callback for a sync callback
 *
 * @param cb_dispatch_work : Pointer to the work_struct that needs to be
 *                           dispatched
 *
 * @return None
 */
void cam_sync_util_cb_dispatch(struct work_struct *cb_dispatch_work);

/**
 * @brief: Function to dispatch callbacks for a signaled sync object
 *
 * @cam_sync_signal_param  : Cam sync signal parameter information
 * @timestamp              : Sync timestamp information
 * @return None
 */
void cam_sync_util_dispatch_signaled_cb(struct cam_sync_signal_param *param,
	struct cam_sync_timestamp *timestamp);

/**
 * @brief: Function to send V4L event to user space
 * @param id       : V4L event id to send
 * @param sync_obj : Sync obj for which event needs to be sent
 * @param status   : Status of the event
 * @param req_id   : sensor req_id/dropped ife req_id sent as part of fence callback
 * @payload        : Payload that needs to be sent to user space
 * @len            : Length of the payload
 * @evt_param      : Event Paramenter
 * @time_stamp     : Sync timestamp information
 * @return None
 */
void cam_sync_util_send_v4l2_event(uint32_t id,
	uint32_t sync_obj,
	int status,
	uint64_t req_id,
	void *payload,
	int len,
	uint32_t evt_param,
	struct cam_sync_timestamp *time_stamp);

/**
 * @brief: Function which gets the next state of the sync object based on the
 *         current state and the new state
 *
 * @param current_state : Current state of the sync object
 * @param new_state     : New state of the sync object
 *
 * @return Next state of the sync object
 */
int cam_sync_util_update_parent_state(struct sync_table_row *parent_row,
	int new_state);

/**
 * @brief: Function to clean up the children of a sync object
 * @row                 : Row whose child list to clean
 * @list_clean_type     : Clean specific object or clean all objects
 * @sync_obj            : Sync object to be clean if list clean type is
 *                          SYNC_LIST_CLEAN_ONE
 *
 * @return None
 */
void cam_sync_util_cleanup_children_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj);

/**
 * @brief: Function to clean up the parents of a sync object
 * @row                 : Row whose parent list to clean
 * @list_clean_type     : Clean specific object or clean all objects
 * @sync_obj            : Sync object to be clean if list clean type is
 *                          SYNC_LIST_CLEAN_ONE
 *
 * @return None
 */
void cam_sync_util_cleanup_parents_list(struct sync_table_row *row,
	uint32_t list_clean_type, uint32_t sync_obj);

/**
 * @brief: Function to send v4l2 event to exit polling thread
 *
 * @return Status of operation.
 */
int cam_sync_util_send_exit_poll_event(void);

/**
 * @brief    : Checks if the uid is equal to or greater than the current uid
 * @sync_var : The sync uid and row id of the sync object
 *
 * @return Whether the sync uid is for an old object, current obj or a new one
 */
enum sync_is_uid_valid cam_sync_check_uid_valid(uint32_t sync_var);

#endif /* __CAM_SYNC_UTIL_H__ */
