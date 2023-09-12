/* Copyright (c) 2013-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_BUF_GENERIC_MNGR_H__
#define __MSM_BUF_GENERIC_MNGR_H__

#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include <media/msmb_generic_buf_mgr.h>

#include "msm.h"
#include "msm_sd.h"

struct msm_get_bufs {
	struct list_head entry;
	struct vb2_v4l2_buffer *vb2_v4l2_buf;
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t index;
};

struct msm_buf_mngr_device {
	struct list_head buf_qhead;
	spinlock_t buf_q_spinlock;
	struct msm_sd_subdev subdev;
	struct msm_sd_req_vb2_q vb2_ops;
	struct list_head cont_qhead;
	struct mutex cont_mutex;
};

struct msm_buf_mngr_user_buf_cont_info {
	struct list_head entry;
	uint32_t sessid;
	uint32_t strid;
	uint32_t index;
	int32_t main_fd;
	struct msm_camera_user_buf_cont_t *paddr;
	uint32_t cnt;
	struct dma_buf *dmabuf;
};

#ifdef CONFIG_COMPAT

struct msm_buf_mngr_info32_t {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t frame_id;
	struct old_timeval32 timestamp;
	uint32_t index;
	uint32_t reserved;
	enum msm_camera_buf_mngr_buf_type type;
	struct msm_camera_user_buf_cont_t user_buf;
};

#define VIDIOC_MSM_BUF_MNGR_GET_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_FLUSH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 39, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_BUF_ERROR32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 41, struct msm_buf_mngr_info32_t)
#endif

/* kernel space functions*/
struct msm_cam_buf_mgr_req_ops {
	int (*msm_cam_buf_mgr_ops)(unsigned int cmd, void *argp);
};

/* API to register callback from client. This assumes cb_struct is allocated by
 * client.
 */
int msm_cam_buf_mgr_register_ops(struct msm_cam_buf_mgr_req_ops *cb_struct);

int32_t msm_buf_mngr_init(void);
void msm_buf_mngr_exit(void);
#endif
