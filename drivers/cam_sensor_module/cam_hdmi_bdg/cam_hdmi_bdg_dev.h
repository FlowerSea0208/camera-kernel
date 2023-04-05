/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_HDMI_DBG_DEV_H_
#define _CAM_HDMI_DBG_DEV_H_

/**
 * @brief : API to register HDMI bdg hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int32_t cam_hdmi_bdg_init_module(void);

/**
 * @brief : API to remove HDMI bdg Hw from platform framework.
 */
void cam_hdmi_bdg_exit_module(void);
#endif /*_CAM_HDMI_DBG_DEV_H_*/
