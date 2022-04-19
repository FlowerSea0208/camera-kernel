/* Copyright (c) 2017-2018, 2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_IFE_CSID_DEV_H_
#define _AIS_IFE_CSID_DEV_H_

#include "ais_isp_hw.h"

irqreturn_t ais_ife_csid_irq(int irq_num, void *data);

int ais_ife_csid_probe(struct platform_device *pdev);
int ais_ife_csid_remove(struct platform_device *pdev);

/**
 * @brief : API to register CSID hw to platform framework.
 * @return struct platform_device pointer on success, or ERR_PTR() on error.
 */
int ais_ife_csid17x_init_module(void);

/**
 * @brief : API to remove CSID Hw from platform framework.
 */
void ais_ife_csid17x_exit_module(void);

#endif /*_AIS_IFE_CSID_DEV_H_ */
