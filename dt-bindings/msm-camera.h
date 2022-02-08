/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __MSM_CAMERA_H
#define __MSM_CAMERA_H

/* CPAS path data types */
#define CAM_CPAS_PATH_DATA_IFE_START_OFFSET 0
#define CAM_CPAS_PATH_DATA_IFE_LINEAR (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 0)
#define CAM_CPAS_PATH_DATA_IFE_VID (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 1)
#define CAM_CPAS_PATH_DATA_IFE_DISP (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 2)
#define CAM_CPAS_PATH_DATA_IFE_STATS (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 3)
#define CAM_CPAS_PATH_DATA_IFE_RDI0 (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 4)
#define CAM_CPAS_PATH_DATA_IFE_RDI1 (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 5)
#define CAM_CPAS_PATH_DATA_IFE_RDI2 (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 6)
#define CAM_CPAS_PATH_DATA_IFE_RDI3 (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 7)
#define CAM_CPAS_PATH_DATA_IFE_PDAF (CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 8)
#define CAM_CPAS_PATH_DATA_IFE_PIXEL_RAW \
	(CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 9)
#define CAM_CPAS_PATH_DATA_IFE_MAX_OFFSET \
	(CAM_CPAS_PATH_DATA_IFE_START_OFFSET + 31)

#define CAM_CPAS_PATH_DATA_IPE_START_OFFSET 32
#define CAM_CPAS_PATH_DATA_IPE_RD_IN (CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 0)
#define CAM_CPAS_PATH_DATA_IPE_RD_REF (CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 1)
#define CAM_CPAS_PATH_DATA_IPE_WR_VID (CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 2)
#define CAM_CPAS_PATH_DATA_IPE_WR_DISP (CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 3)
#define CAM_CPAS_PATH_DATA_IPE_WR_REF (CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 4)
#define CAM_CPAS_PATH_DATA_IPE_MAX_OFFSET \
	(CAM_CPAS_PATH_DATA_IPE_START_OFFSET + 31)

#define CAM_CPAS_PATH_DATA_OPE_START_OFFSET 64
#define CAM_CPAS_PATH_DATA_OPE_RD_IN (CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 0)
#define CAM_CPAS_PATH_DATA_OPE_RD_REF (CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 1)
#define CAM_CPAS_PATH_DATA_OPE_WR_VID (CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 2)
#define CAM_CPAS_PATH_DATA_OPE_WR_DISP (CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 3)
#define CAM_CPAS_PATH_DATA_OPE_WR_REF (CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 4)
#define CAM_CPAS_PATH_DATA_OPE_MAX_OFFSET \
	(CAM_CPAS_PATH_DATA_OPE_START_OFFSET + 31)

#define CAM_CPAS_PATH_DATA_SFE_START_OFFSET 96
#define CAM_CPAS_PATH_DATA_SFE_NRDI    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 0)
#define CAM_CPAS_PATH_DATA_SFE_RDI0    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 1)
#define CAM_CPAS_PATH_DATA_SFE_RDI1    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 2)
#define CAM_CPAS_PATH_DATA_SFE_RDI2    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 3)
#define CAM_CPAS_PATH_DATA_SFE_RDI3    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 4)
#define CAM_CPAS_PATH_DATA_SFE_RDI4    (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 5)
#define CAM_CPAS_PATH_DATA_SFE_STATS   (CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 6)
#define CAM_CPAS_PATH_DATA_SFE_MAX_OFFSET \
	(CAM_CPAS_PATH_DATA_SFE_START_OFFSET + 31)

#define CAM_CPAS_PATH_DATA_CONSO_OFFSET 256
#define CAM_CPAS_PATH_DATA_ALL (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 0)

/* IFE consolidated paths */
#define CAM_CPAS_PATH_DATA_IFE_LINEAR_PDAF (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 1)
#define CAM_CPAS_PATH_DATA_IFE_UBWC_STATS (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 2)
#define CAM_CPAS_PATH_DATA_IFE_PIXEL_ALL (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 3)
#define CAM_CPAS_PATH_DATA_IFE_RDI_PIXEL_RAW \
	(CAM_CPAS_PATH_DATA_CONSO_OFFSET + 4)
#define CAM_CPAS_PATH_DATA_IFE_RDI_ALL (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 5)
#define CAM_CPAS_PATH_DATA_IFE_UBWC (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 6)
#define CAM_CPAS_PATH_DATA_IFE_LINEAR_STATS \
	(CAM_CPAS_PATH_DATA_CONSO_OFFSET + 7)
#define CAM_CPAS_PATH_DATA_IFE_UBWC_LINEAR \
	(CAM_CPAS_PATH_DATA_CONSO_OFFSET + 8)

/* IPE Consolidated paths */
#define CAM_CPAS_PATH_DATA_IPE_WR_VID_DISP (CAM_CPAS_PATH_DATA_CONSO_OFFSET + 1)

/* CPAS transaction types */
#define CAM_CPAS_TRANSACTION_READ 0
#define CAM_CPAS_TRANSACTION_WRITE 1

/* CPAS traffic merge types */
#define CAM_CPAS_TRAFFIC_MERGE_SUM 0
#define CAM_CPAS_TRAFFIC_MERGE_SUM_INTERLEAVE 1

/* Feature bit type */
#define CAM_CPAS_FEATURE_TYPE_DISABLE 0
#define CAM_CPAS_FEATURE_TYPE_ENABLE 1
#define CAM_CPAS_FEATURE_TYPE_VALUE 2

/* Feature support bit positions in feature fuse register*/
#define CAM_CPAS_QCFA_BINNING_ENABLE           0
#define CAM_CPAS_SECURE_CAMERA_ENABLE          1
#define CAM_CPAS_MF_HDR_ENABLE                 2
#define CAM_CPAS_MP_LIMIT_FUSE                 3
#define CAM_CPAS_ISP_FUSE                      4
#define CAM_CPAS_ISP_PIX_FUSE                  5
#define CAM_CPAS_ISP_LITE_FUSE                 6
#define CAM_CPAS_CSIPHY_FUSE                   7
#define CAM_CPAS_IPE_VID_OUT_8BPP_LIMIT_ENABLE 8
#define CAM_CPAS_ICP_IPE_FUSE                  9
#define CAM_CPAS_ICP_BPS_FUSE                  10
#define CAM_CPAS_FUSE_FEATURE_MAX              11

#define CCI_MASTER_0         0
#define CCI_MASTER_1         1
#define CCI_MASTER_MAX       2

#endif
