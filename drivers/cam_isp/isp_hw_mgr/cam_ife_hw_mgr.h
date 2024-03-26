/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_IFE_HW_MGR_H_
#define _CAM_IFE_HW_MGR_H_

#include <linux/completion.h>
#include <linux/time.h>
#include "cam_isp_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_tasklet_util.h"
#include "cam_cdm_intf_api.h"
#include "cam_cpas_api.h"

/*
 * enum cam_ife_ctx_master_type - HW master type
 * CAM_IFE_CTX_TYPE_NONE: IFE ctx/stream directly connected to CSID
 * CAM_IFE_CTX_TYPE_CUSTOM: IFE ctx/stream connected to custom HW
 * CAM_IFE_CTX_TYPE_SFE: IFE ctx/stream connected to SFE
 */
enum cam_ife_ctx_master_type {
	CAM_IFE_CTX_TYPE_NONE,
	CAM_IFE_CTX_TYPE_CUSTOM,
	CAM_IFE_CTX_TYPE_SFE,
	CAM_IFE_CTX_TYPE_MAX,
};

/* IFE resource constants */
#define CAM_IFE_HW_IN_RES_MAX            (CAM_ISP_IFE_IN_RES_MAX & 0xFF)
#define CAM_IFE_HW_RES_POOL_MAX          64

/* IFE_HW_MGR ctx config */
#define CAM_IFE_CTX_CFG_FRAME_HEADER_TS   BIT(0)
#define CAM_IFE_CTX_CFG_SW_SYNC_ON        BIT(1)
#define CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON BIT(2)

#define CAM_IFE_HW_STATE_STOPPED  0
#define CAM_IFE_HW_STATE_STARTING 1
#define CAM_IFE_HW_STATE_STARTED  2
#define CAM_IFE_HW_STATE_BUSY     3
#define CAM_IFE_HW_STATE_STOPPING 4

/**
 * struct cam_ife_hw_mgr_debug - contain the debug information
 *
 * @dentry:                    Debugfs entry
 * @csid_debug:                csid debug information
 * @rx_capture_debug:          rx capture debug info
 * @enable_recovery:           enable recovery
 * @camif_debug:               camif debug info
 * @enable_csid_recovery:      enable csid recovery
 * @sfe_debug:                 sfe debug config
 * @sfe_sensor_diag_cfg:       sfe sensor diag config
 * @csid_test_bus:             csid test bus config
 * @sfe_cache_debug:           sfe cache debug info
 * @ife_perf_counter_val:      ife perf counter values
 * @sfe_perf_counter_val:      sfe perf counter values
 * @enable_req_dump:           Enable request dump on HW errors
 * @per_req_reg_dump:          Enable per request reg dump
 * @disable_ubwc_comp:         Disable UBWC compression
 * @disable_ife_mmu_prefetch:  Disable MMU prefetch for IFE bus WR
 * @rx_capture_debug_set:      If rx capture debug is set by user
 * @disable_isp_drv:           Disable ISP DRV config
 *
 */
struct cam_ife_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       rx_capture_debug;
	uint32_t       enable_recovery;
	uint32_t       camif_debug;
	uint32_t       enable_csid_recovery;
	uint32_t       sfe_debug;
	uint32_t       sfe_sensor_diag_cfg;
	uint32_t       csid_test_bus;
	uint32_t       sfe_cache_debug[CAM_SFE_HW_NUM_MAX];
	uint32_t      *ife_perf_counter_val;
	uint32_t      *sfe_perf_counter_val;
	bool           enable_req_dump;
	bool           per_req_reg_dump;
	bool           disable_ubwc_comp;
	bool           disable_ife_mmu_prefetch;
	bool           rx_capture_debug_set;
	bool           disable_isp_drv;
};

/**
 * struct cam_ife_hw_mgr_ctx_pf_info - pf buf info
 *
 * @out_port_id: Out port id
 * @mid: MID value
 */
struct cam_ife_hw_mgr_ctx_pf_info {
	uint32_t       out_port_id;
	uint32_t       mid;
};

/**
 * struct cam_ife_sfe_scratch_buf_info - Scratch buf info
 *
 * @width: Width in pixels
 * @height: Height in pixels
 * @stride: Stride in pixels
 * @slice_height: Height in lines
 * @io_addr: Buffer address
 * @res_id: Resource type
 * @offset: Buffer offset
 * @config_done: To indicate if RDIx received scratch cfg
 * @is_secure: secure scratch buffer
 */
struct cam_ife_sfe_scratch_buf_info {
	uint32_t   width;
	uint32_t   height;
	uint32_t   stride;
	uint32_t   slice_height;
	dma_addr_t io_addr;
	uint32_t   res_id;
	uint32_t   offset;
	bool       config_done;
	bool       is_secure;
};

/**
 * struct cam_sfe_scratch_buf_cfg - Scratch buf info
 *
 * @num_configs: Total Number of scratch buffers provided
 * @streamon_buf_mask: Mask to indicate which ports have received buffer
 *                     from userspace, apply scratch for other ports
 * @updated_num_exp: Current num of exposures
 * @buf_info: Info on each of the buffers
 * @skip_scratch_cfg_streamon: Determine if scratch cfg needs to be programmed at stream on
 *
 */
struct cam_sfe_scratch_buf_cfg {
	uint32_t                            num_config;
	uint32_t                            streamon_buf_mask;
	uint32_t                            updated_num_exp;
	struct cam_ife_sfe_scratch_buf_info buf_info[
		CAM_SFE_FE_RDI_NUM_MAX];
	bool                                skip_scratch_cfg_streamon;
};

/**
 * struct cam_ife_scratch_buf_cfg - Scratch buf info
 *
 * @num_config: Total Number of scratch buffers provided
 * @streamon_buf_mask: Mask to indicate which ports have received buffer
 *                     from userspace, apply scratch for other ports
 * @buf_info: Info on each of the buffers
 * @skip_scratch_cfg_streamon: Determine if scratch cfg needs to be programmed at stream on
 *
 */
struct cam_ife_scratch_buf_cfg {
	uint32_t                            num_config;
	uint32_t                            streamon_buf_mask;
	struct cam_ife_sfe_scratch_buf_info buf_info[
		CAM_IFE_SCRATCH_NUM_MAX];
	bool                                skip_scratch_cfg_streamon;
};


/**
 * struct cam_ife_hw_mgr_ctx_scratch_buf_info - Scratch buffer info
 *
 * @num_fetches:               Indicate number of SFE fetches for this stream
 * @sfe_scratch_config:        Scratch buffer config if any for SFE ports
 * @ife_scratch_config:        Scratch buffer config if any for IFE ports
 */
struct cam_ife_hw_mgr_ctx_scratch_buf_info {
	uint32_t                        num_fetches;
	struct cam_sfe_scratch_buf_cfg *sfe_scratch_config;
	struct cam_ife_scratch_buf_cfg *ife_scratch_config;
};

/**
 * struct cam_ife_hw_mgr_ctx_flags - IFE HW mgr ctx flags
 *
 * @ctx_in_use:          flag to tell whether context is active
 * @init_done:           indicate whether init hw is done
 * @is_fe_enabled:       indicate whether fetch engine\read path is enabled
 * @is_dual:             indicate whether context is in dual VFE mode
 * @is_offline:          indicate whether context is for offline IFE
 * @dsp_enabled:         indicate whether dsp is enabled in this context
 * @internal_cdm:        indicate whether context uses internal CDM
 * @pf_mid_found:        in page fault, mid found for this ctx.
 * @need_csid_top_cfg:   Flag to indicate if CSID top cfg is needed.
 * @is_rdi_only_context: flag to specify the context has only rdi resource
 * @is_lite_context:     flag to specify the context has only uses lite
 *                       resources
 * @is_sfe_shdr:         indicate if stream is for SFE sHDR
 * @is_sfe_fs:           indicate if stream is for inline SFE FS
 * @dump_on_flush:       Set if reg dump triggered on flush
 * @dump_on_error:       Set if reg dump triggered on error
 * @custom_aeb_mode:     Set if custom AEB stream
 * @rdi_lcr_en:          To indicate if RDI LCR is enabled
 * @sys_cache_usage:     Per context sys cache usage
 *                       The corresponding index will be set
 *                       for the cache type
 * @rdi_pd_context:      Flag to specify the context has
 *                       only rdi and PD resource without PIX port.
 *
 */
struct cam_ife_hw_mgr_ctx_flags {
	bool   ctx_in_use;
	bool   init_done;
	bool   is_fe_enabled;
	bool   is_dual;
	bool   is_offline;
	bool   dsp_enabled;
	bool   internal_cdm;
	bool   pf_mid_found;
	bool   need_csid_top_cfg;
	bool   is_rdi_only_context;
	bool   is_lite_context;
	bool   is_sfe_shdr;
	bool   is_sfe_fs;
	bool   dump_on_flush;
	bool   dump_on_error;
	bool   is_aeb_mode;
	bool   rdi_lcr_en;
	bool   sys_cache_usage[CAM_LLCC_MAX];
	bool   rdi_pd_context;
};

/**
 * struct cam_ife_cdm_user_data - IFE HW user data with CDM
 *
 * @prepare:                   hw_update_data
 * @request_id:                Request id
 */
struct cam_ife_cdm_user_data {
	struct cam_isp_prepare_hw_update_data    *hw_update_data;
	uint64_t                                  request_id;
};

/**
 * struct cam_ife_mgr_bw_data - contain data to calc bandwidth for context
 *
 * @format:                    image format
 * @width:                     image width
 * @height:                    image height
 * @framerate:                 framerate
 *
 */
struct cam_ife_mgr_bw_data {
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t framerate;
};

/**
 * struct cam_ife_hw_mgr_ctx - IFE HW manager Context object
 *
 * concr_ctx:             HW Context currently used from this manager context
 * hw_mgr:                IFE hw mgr which owns this context
 * event_cb:              event callbacks
 * cb_priv:               event callbacks data
 * ctx_in_use:            indicates if context is active
 * is_offline:            indicates if context is used for offline processing
 * ctx_idx:               index of this context
 * num_in_ports:          number of context input ports
 * in_ports:              context input ports
 * bw_data:               contains data for BW usage calculation
 *
 */
struct cam_ife_hw_mgr_ctx {
	struct cam_ife_hw_concrete_ctx       *concr_ctx;
	struct cam_ife_hw_mgr                *hw_mgr;
	cam_hw_event_cb_func                  event_cb[CAM_ISP_HW_EVENT_MAX];
	void                                 *cb_priv;
	uint32_t                              ctx_in_use;
	bool                                  is_offline;
	uint32_t                              ctx_idx;
	uint32_t                              num_in_ports;
	struct cam_isp_in_port_generic_info  *in_ports;
	struct cam_ife_mgr_bw_data            bw_data;
};


/**
 * struct cam_ife_hw_concrete_ctx - IFE HW manager Context object
 *
 * @list:                   used by the ctx list.
 * @common:                 common acquired context data
 * @ctx_index:              acquired context id.
 * @left_hw_idx:            hw index for master core [left]
 * @right_hw_idx:           hw index for slave core [right]
 * @hw_mgr:                 IFE hw mgr which owns this context
 * @res_list_csid:          CSID resource list
 * @res_list_ife_src:       IFE input resource list
 * @res_list_sfe_src        SFE input resource list
 * @res_list_ife_in_rd      IFE/SFE input resource list for read path
 * @res_list_ife_out:       IFE output resoruces array
 * @res_list_sfe_out:       SFE output resources array
 * @num_acq_vfe_out:        Number of acquired VFE out resources
 * @num_acq_sfe_out:        Number of acquired SFE out resources
 * @free_res_list:          Free resources list for the branch node
 * @res_pool:               memory storage for the free resource list
 * @irq_status0_mask:       irq_status0_mask for the context
 * @irq_status1_mask:       irq_status1_mask for the context
 * @base                    device base index array contain the all IFE HW
 *                          instance associated with this context.
 * @num_base                number of valid base data in the base array
 * @cdm_handle              cdm hw acquire handle
 * @cdm_hw_idx:             Physical CDM in use
 * @cdm_ops                 cdm util operation pointer for building
 *                          cdm commands
 * @cdm_cmd                 cdm base and length request pointer
 * @cdm_id                  cdm id of the acquired cdm
 * @sof_cnt                 sof count value per core, used for dual VFE
 * @epoch_cnt               epoch count value per core, used for dual VFE
 * @eof_cnt                 eof count value per core, used for dual VFE
 * @overflow_pending        flat to specify the overflow is pending for the
 *                          context
 * @cdm_done                flag to indicate cdm has finished writing shadow
 *                          registers
 * @last_cdm_done_req:      Last cdm done request
 * @config_done_complete    indicator for configuration complete
 * @reg_dump_buf_desc:      cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:       Count of descriptors in reg_dump_buf_desc
 * @applied_req_id:         Last request id to be applied
 * @ctx_type                Type of IFE ctx [CUSTOM/SFE etc.]
 * @ctx_config              ife ctx config  [bit field]
 * @ts                      captured timestamp when the ctx is acquired
 * @hw_enabled              Array to indicate active HW
 * @buf_done_controller     Buf done controller.
 * @scratch_buf_info        Scratch buf [SFE/IFE] info pertaining to this stream
 * @flags                   Flags pertainting to this ctx
 * @bw_config_version       BW Config version
 * @recovery_id:            Unique ID of the current valid scheduled recovery
 * @current_mup:            Current MUP val, scratch will then apply the same as previously
 *                          applied request
 * @curr_num_exp:           Current num of exposures
 * @try_recovery_cnt:       Retry count for overflow recovery
 * @recovery_req_id:        The request id on which overflow recovery happens
 *
 */
struct cam_ife_hw_concrete_ctx  {
	struct list_head                          list;
	struct cam_isp_hw_mgr_ctx                 common;

	void                                     *tasklet_info;
	uint32_t                                  ctx_index;
	uint32_t                                  left_hw_idx;
	uint32_t                                  right_hw_idx;
	struct cam_ife_hw_mgr                    *hw_mgr;
	uint32_t                                  acquired_hw_id;
	uint32_t                                  served_ctx_id[2];
	uint32_t                                  served_ctx_r;
	uint32_t                                  served_ctx_w;

	struct cam_isp_hw_mgr_res                  res_list_ife_in;
	struct list_head                           res_list_ife_csid;
	struct list_head                           res_list_ife_src;
	struct list_head                           res_list_sfe_src;
	struct list_head                           res_list_ife_in_rd;
	struct cam_isp_hw_mgr_res                 *res_list_ife_out;
	struct cam_isp_hw_mgr_res                 *res_list_sfe_out;
	struct list_head                           free_res_list;
	struct cam_isp_hw_mgr_res                  res_pool[CAM_IFE_HW_RES_POOL_MAX];
	uint32_t                                   num_acq_vfe_out;
	uint32_t                                   num_acq_sfe_out;

	uint32_t                                   irq_status0_mask[CAM_IFE_HW_NUM_MAX];
	uint32_t                                   irq_status1_mask[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_ctx_base_info               base[CAM_IFE_HW_NUM_MAX +
								CAM_SFE_HW_NUM_MAX];
	uint32_t                                   num_base;
	uint32_t                                   cdm_handle;
	int32_t                                    cdm_hw_idx;
	struct cam_cdm_utils_ops                  *cdm_ops;
	struct cam_cdm_bl_request                 *cdm_cmd;
	enum cam_cdm_id                            cdm_id;
	uint32_t                                   sof_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                                   epoch_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                                   eof_cnt[CAM_IFE_HW_NUM_MAX];
	atomic_t                                   overflow_pending;
	atomic_t                                   cdm_done;
	uint64_t                                   last_cdm_done_req;
	struct completion                          config_done_complete;
	uint32_t                                   hw_version;
	struct cam_cmd_buf_desc                    reg_dump_buf_desc[
						CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                                   num_reg_dump_buf;
	uint64_t                                   applied_req_id;
	enum cam_ife_ctx_master_type               ctx_type;
	uint32_t                                   ctx_config;
	struct timespec64                          ts;
	void                                      *buf_done_controller;
	struct cam_ife_hw_mgr_ctx_scratch_buf_info scratch_buf_info;
	struct cam_ife_hw_mgr_ctx_flags            flags;
	struct cam_ife_hw_mgr_ctx_pf_info          pf_info;
	struct cam_ife_cdm_user_data               cdm_userdata;
	uint32_t                                   bw_config_version;
	atomic_t                                   recovery_id;
	uint32_t                                   current_mup;
	uint32_t                                   curr_num_exp;
	uint32_t                                   try_recovery_cnt;
	uint64_t                                   recovery_req_id;
	atomic_t                                   ctx_state;
	bool                                       is_offline;
	uint32_t                                   offline_clk;
};

/**
 * struct cam_ife_offline_hw - Offline ife context allocation information
 *
 * @ctx_idx:                context index
 * @custom_enabled:         update the flag if context is connected to custom HW
 * @use_frame_header_ts     obtain qtimer ts using frame header
 * @acquired_hw_id:         Acquired hardware mask
 * @acquired_hw_path:       Acquired path mask for an input
 *                          if input splits into multiple paths,
 *                          its updated per hardware
 * valid_acquired_hw:       Valid num of acquired hardware
 * @ife_ctx:                HW context connected to that HW
 *
 */
struct cam_ife_offline_hw {
	uint32_t                        ctx_idx;
	bool                            custom_enabled;
	bool                            use_frame_header_ts;
	uint32_t                        acquired_hw_id[CAM_MAX_ACQ_RES];
	uint32_t                        acquired_hw_path[CAM_MAX_ACQ_RES][
						CAM_MAX_HW_SPLIT];
	uint32_t                        valid_acquired_hw;
	struct cam_ife_hw_concrete_ctx *ife_ctx;
};

/**
 * struct cam_ife_mgr_offline_in_queue - Offline IFE input request queue element
 *
 * @list:                   list private data;
 * @request_id:             request id
 * @ctx_idx:                context owning this request
 * @hw_id:                  ID of hardware core servicing this request
 * @prepare:                prepare requset data
 * @cfg:                    config request data
 * @ready:                  indicates if request is ready to be processed
 *
 */
struct cam_ife_mgr_offline_in_queue {
	struct list_head                  list;
	uint64_t                          request_id;
	uint32_t                          ctx_idx;
	uint32_t                          hw_id;
	struct cam_hw_prepare_update_args prepare;
	struct cam_hw_config_args         cfg;
	bool                              ready;
};

/**
 * struct cam_isp_bus_hw_caps - BUS capabilities
 *
 * @max_vfe_out_res_type  :  max ife out res type value from hw
 * @max_sfe_out_res_type  :  max sfe out res type value from hw
 * @num_ife_perf_counters :  max ife perf counters supported
 * @num_sfe_perf_counters :  max sfe perf counters supported
 * @support_consumed_addr :  indicate whether hw supports last consumed address
 */
struct cam_isp_ife_sfe_hw_caps {
	uint32_t     max_vfe_out_res_type;
	uint32_t     max_sfe_out_res_type;
	uint32_t     num_ife_perf_counters;
	uint32_t     num_sfe_perf_counters;
	bool         support_consumed_addr;
};

/*
 * struct cam_isp_sys_cache_info:
 *
 * @Brief:                   ISP Bus sys cache info. Placeholder for all cache ids and their
 *                           types
 *
 * @type:                    Cache type
 * @scid:                    Cache slice ID
 */
struct cam_isp_sys_cache_info {
	enum cam_sys_cache_config_types type;
	int32_t                         scid;
};

/*
 * struct cam_isp_sfe_cache_info:
 *
 * @Brief:                   SFE cache info. Placeholder for:
 *                           1. Supported cache IDs which are populated during
 *                           probe based on large and small.
 *                           2. keeps track for the current cache id used for a
 *                           particular exposure type
 *                           3. keeps track of acvitated exposures.
 *                           Based on this data, we can toggle the SCIDs for a particular
 *                           hw whenever there is a hw halt. Also we don't change
 *                           the SCID in case of dynamic exposure switches.
 *
 * @supported_scid_idx:      Bit mask for IDs supported for each exposure type
 * @curr_idx:                Index of Cache ID in use for each exposure
 * @activated:               Maintains if the cache is activated for a particular exposure
 */
struct cam_isp_sfe_cache_info {
	int      supported_scid_idx;
	int      curr_idx[CAM_ISP_EXPOSURE_MAX];
	bool     activated[CAM_ISP_EXPOSURE_MAX];
};

/**
 * struct cam_ife_hw_mgr - IFE HW Manager
 *
 * @mgr_common:            common data for all HW managers
 * @csid_devices;          csid device instances array. This will be filled by
 *                         HW manager during the initialization.
 * @ife_devices:           IFE device instances array. This will be filled by
 *                         HW layer during initialization
 * @sfe_devices:           SFE device instance array
 * @ctx_mutex:             mutex for the hw context pool
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 * @csid_hw_caps           csid hw capability stored per core
 * @ife_dev_caps           ife device capability per core
 * @work q                 work queue for IFE hw manager
 * @debug_cfg              debug configuration
 * @ctx_lock               context lock
 * @hw_pid_support         hw pid support for this target
 * @csid_rup_en            Reg update at CSID side
 * @csid_global_reset_en   CSID global reset enable
 * @csid_camif_irq_support CSID camif IRQ support
 * @cam_ddr_drv_support    DDR DRV support
 * @isp_caps               Capability of underlying SFE/IFE HW
 * @path_port_map          Mapping of outport to IFE mux
 * @num_caches_found       Number of caches supported
 * @sys_cache_info         Sys cache info
 * @sfe_cache_info         SFE Cache Info
 * @offline_clk:               last set clock for offline processing
 * @max_clk_threshold:         min clock threshold
 * @nom_clk_threshold:         nom clock threshold
 * @min_clk_threshold:         max clock threshold
 * @bytes_per_clk:             bytes per clock processed
 */
struct cam_ife_hw_mgr {
	struct cam_isp_hw_mgr          mgr_common;
	struct cam_hw_intf            *csid_devices[CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_isp_hw_intf_data   *ife_devices[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_hw_intf_data   *sfe_devices[CAM_SFE_HW_NUM_MAX];
	struct cam_soc_reg_map        *cdm_reg_map[CAM_IFE_HW_NUM_MAX];

	struct mutex                     ctx_mutex;
	atomic_t                         active_ctx_cnt;
	struct list_head                 free_ctx_list;
	struct list_head                 used_ctx_list;
	struct cam_ife_hw_concrete_ctx   ctx_pool[CAM_IFE_CTX_MAX];
	struct cam_ife_hw_mgr_ctx        virt_ctx_pool[CAM_IFE_CTX_MAX];

	struct cam_ife_csid_hw_caps      csid_hw_caps[
						CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_vfe_hw_get_hw_cap     ife_dev_caps[CAM_IFE_HW_NUM_MAX];
	struct cam_req_mgr_core_workq   *workq;
	struct cam_ife_hw_mgr_debug      debug_cfg;
	spinlock_t                       ctx_lock;
	atomic_t                         num_acquired_offline_ctx;
	struct cam_ife_offline_hw        acquired_hw_pool[CAM_IFE_CTX_MAX];
	struct cam_ife_mgr_offline_in_queue   input_queue;
	struct cam_ife_mgr_offline_in_queue   in_proc_queue;
	uint32_t                         starting_offline_cnt;
	bool                             hw_pid_support;
	bool                             csid_rup_en;
	bool                             csid_global_reset_en;
	bool                             csid_camif_irq_support;
	bool                             cam_ddr_drv_support;
	struct cam_isp_ife_sfe_hw_caps   isp_caps;
	struct cam_isp_hw_path_port_map  path_port_map;

	uint32_t                         num_caches_found;
	struct cam_isp_sys_cache_info    sys_cache_info[CAM_LLCC_MAX];
	struct cam_isp_sfe_cache_info    sfe_cache_info[CAM_SFE_HW_NUM_MAX];

	uint32_t                         offline_clk;
	uint32_t                         max_clk_threshold;
	uint32_t                         nom_clk_threshold;
	uint32_t                         min_clk_threshold;
	uint32_t                         bytes_per_clk;
};

/**
 * struct cam_ife_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 * @id:                       Unique ID associated with this recovery data (per HW context)
 *
 */
struct cam_ife_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_ISP_HW_NUM_MAX];
	struct cam_ife_hw_concrete_ctx *affected_ctx[CAM_IFE_CTX_MAX];
	uint32_t                   no_of_context;
	uint32_t                   id[CAM_IFE_CTX_MAX];
};

/**
 * struct cam_ife_hw_mini_dump_ctx - Mini dump data
 *
 * @base:                   device base index array contain the all IFE HW
 * @pf_info:                Page Fault Info
 * @csid_md:                CSID mini dump data
 * @vfe_md:                 VFE mini dump data
 * @flags:                  Flags pertainting to this ctx
 * @ctx_priv:               Array of the hardware contexts that are affected
 * @last_cdm_done_req:      Last cdm done request
 * @applied_req_id:         Last request id to be applied
 * @cdm_handle:             cdm hw acquire handle
 * @ctx_index:              acquired context id.
 * @left_hw_idx:            hw index for master core [left]
 * @right_hw_idx:           hw index for slave core [right]
 * @num_base:               number of valid base data in the base array
 * @cdm_id:                 cdm id of the acquired cdm
 * @ctx_type:               Type of IFE ctx [CUSTOM/SFE etc.]
 * @overflow_pending:       flat to specify the overflow is pending for the
 * @cdm_done:               flag to indicate cdm has finished writing shadow
 *                          registers
 */
struct cam_ife_hw_mini_dump_ctx {
	struct cam_isp_ctx_base_info          base[CAM_IFE_HW_NUM_MAX +
				         	CAM_SFE_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx_pf_info     pf_info;
	void                                 *csid_md[CAM_IFE_HW_NUM_MAX];
	void                                 *vfe_md[CAM_IFE_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx_flags       flags;
	void                                 *ctx_priv;
	uint64_t                              last_cdm_done_req;
	uint64_t                              applied_req_id;
	uint32_t                              cdm_handle;
	uint8_t                               ctx_index;
	uint8_t                               left_hw_idx;
	uint8_t                               right_hw_idx;
	uint8_t                               num_base;
	enum cam_cdm_id                       cdm_id;
	enum cam_ife_ctx_master_type          ctx_type;
	bool                                  overflow_pending;
	bool                                  cdm_done;
};

/**
 * struct cam_ife_hw_mini_dump_data - Mini dump data
 *
 * @num_ctx:                  Number of context dumped
 * @ctx:                      Array of context
 *
 */
struct cam_ife_hw_mini_dump_data {
	uint32_t                            num_ctx;
	struct cam_ife_hw_mini_dump_ctx    *ctx[CAM_IFE_CTX_MAX];
};

/**
 * cam_ife_hw_mgr_init()
 *
 * @brief:              Initialize the IFE hardware manger. This is the
 *                      etnry functinon for the IFE HW manager.
 *
 * @hw_mgr_intf:        IFE hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);
void cam_ife_hw_mgr_deinit(void);

#endif /* _CAM_IFE_HW_MGR_H_ */
