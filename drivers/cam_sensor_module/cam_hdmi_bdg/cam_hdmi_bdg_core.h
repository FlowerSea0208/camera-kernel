/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "cam_sensor_dev.h"

#define HDMI_BDG_SENSOR_ID 0x1704
#define HDMI_BDG_FW_VERSION 0x000001
#define HDMI_BDG_HDMI_CONNECTED    0x55
#define HDMI_BDG_HDMI_DISCONNECTED 0x88

/**
 * @s_ctrl: Sensor ctrl structure
 *
 * This API set lt6911 camera struct for irq handle.
 */
int cam_hdmi_bdg_set_cam_ctrl(struct cam_sensor_ctrl_t *s_ctrl);

/**
 * This API unset lt6911 camera struct for irq handle.
 */
void cam_hdmi_bdg_unset_cam_ctrl(void);

int cam_hdmi_bdg_get_src_resolution(bool *signal_stable,
		int *width, int *height, int *id);
