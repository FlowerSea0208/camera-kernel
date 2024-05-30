load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("//msm-kernel:target_variants.bzl", "get_all_variants")

def _define_module(target, variant):
    tv = "{}_{}".format(target, variant)
    deps = [
        ":camera_headers",
        ":camera_banner",
        "//msm-kernel:all_headers",
    ]
    if target == "anorak":
        deps.extend([
	    "//vendor/qcom/opensource/dsp-kernel:{}_frpc-adsprpc".format(tv),
        ])
    ddk_module(
        name = "{}_camera".format(tv),
        out = "camera.ko",
        srcs = [
            "drivers/cam_req_mgr/cam_fastrpc.c",
            "drivers/cam_req_mgr/cam_req_mgr_core.c",
            "drivers/cam_req_mgr/cam_req_mgr_dev.c",
            "drivers/cam_req_mgr/cam_req_mgr_util.c",
            "drivers/cam_req_mgr/cam_mem_mgr.c",
            "drivers/cam_req_mgr/cam_req_mgr_workq.c",
            "drivers/cam_req_mgr/cam_req_mgr_timer.c",
            "drivers/cam_req_mgr/cam_req_mgr_debug.c",
            "drivers/cam_utils/cam_soc_util.c",
            "drivers/cam_utils/cam_packet_util.c",
            "drivers/cam_utils/cam_debug_util.c",
            "drivers/cam_utils/cam_trace.c",
            "drivers/cam_utils/cam_common_util.c",
            "drivers/cam_utils/cam_compat.c",
            "drivers/cam_core/cam_context.c",
            "drivers/cam_core/cam_context_utils.c",
            "drivers/cam_core/cam_node.c",
            "drivers/cam_core/cam_rpmsg.c",
            "drivers/cam_core/cam_subdev.c",
            "drivers/cam_core/cam_rpmsg_pkt.c",
            "drivers/cam_smmu/cam_smmu_api.c",
            "drivers/cam_sync/cam_sync.c",
            "drivers/cam_sync/cam_sync_util.c",
            "drivers/cam_cpas/cpas_top/cam_cpastop_hw.c",
            "drivers/cam_cpas/camss_top/cam_camsstop_hw.c",
            "drivers/cam_cpas/cam_cpas_soc.c",
            "drivers/cam_cpas/cam_cpas_intf.c",
            "drivers/cam_cpas/cam_cpas_hw.c",
            "drivers/cam_cdm/cam_cdm_soc.c",
            "drivers/cam_cdm/cam_cdm_util.c",
            "drivers/cam_cdm/cam_cdm_intf.c",
            "drivers/cam_cdm/cam_cdm_core_common.c",
            "drivers/cam_cdm/cam_cdm_virtual_core.c",
            "drivers/cam_cdm/cam_cdm_hw_core.c",
            "drivers/cam_utils/cam_soc_icc.c",
            "drivers/camera_main.c",
        ],
        conditional_srcs = {
            "CONFIG_QCOM_CX_IPEAK": {
                True: ["drivers/cam_utils/cam_cx_ipeak.c"],
            },
            "CONFIG_INTERCONNECT_QCOM": {
                True: ["drivers/cam_utils/cam_soc_icc.c"],
            },
            "CONFIG_SPECTRA_ISP": {
                True: [
                    "drivers/cam_isp/isp_hw_mgr/hw_utils/cam_tasklet_util.c",
                    "drivers/cam_isp/isp_hw_mgr/hw_utils/cam_isp_packet_parser.c",
                    "drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller/cam_irq_controller.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_common.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_virt_csid_mod.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_mod.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_lite_mod.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_core.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_top/cam_sfe_top.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_rd.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_wr.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_soc.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_dev.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_core.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_rd_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_common.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver4.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver2.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_rdi.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_fe_ver1.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver3.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe17x/cam_vfe.c",
                    "drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe17x/cam_virt_vfe.c",
                    "drivers/cam_isp/isp_hw_mgr/cam_isp_hw_mgr.c",
                    "drivers/cam_isp/isp_hw_mgr/cam_ife_hw_mgr.c",
                    "drivers/cam_isp/cam_isp_dev.c",
                    "drivers/cam_isp/cam_isp_context.c",
                ],
            },
            "CONFIG_SPECTRA_ICP": {
                True: [
                    "drivers/cam_icp/icp_hw/icp_hw_mgr/cam_icp_hw_mgr.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_dev.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_core.c",
                    "drivers/cam_icp/icp_hw/ipe_hw/ipe_soc.c",
                    "drivers/cam_icp/icp_hw/a5_hw/a5_dev.c",
                    "drivers/cam_icp/icp_hw/a5_hw/a5_core.c",
                    "drivers/cam_icp/icp_hw/a5_hw/a5_soc.c",
                    "drivers/cam_icp/icp_hw/lx7_hw/lx7_dev.c",
                    "drivers/cam_icp/icp_hw/lx7_hw/lx7_core.c",
                    "drivers/cam_icp/icp_hw/lx7_hw/lx7_soc.c",
                    "drivers/cam_icp/utils/cam_icp_utils.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_dev.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_core.c",
                    "drivers/cam_icp/icp_hw/bps_hw/bps_soc.c",
                    "drivers/cam_icp/cam_icp_subdev.c",
                    "drivers/cam_icp/cam_icp_context.c",
                    "drivers/cam_icp/hfi.c",
                ],
            },
            "CONFIG_SPECTRA_JPEG": {
                True: [
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_dev.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_core.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_soc.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_dev.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_core.c",
                    "drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_soc.c",
                    "drivers/cam_jpeg/jpeg_hw/cam_jpeg_hw_mgr.c",
                    "drivers/cam_jpeg/cam_jpeg_dev.c",
                    "drivers/cam_jpeg/cam_jpeg_context.c",
                ],
            },
            "CONFIG_SPECTRA_FD": {
                True: [
                    "drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_dev.c",
                    "drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_core.c",
                    "drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_soc.c",
                    "drivers/cam_fd/fd_hw_mgr/cam_fd_hw_mgr.c",
                    "drivers/cam_fd/cam_fd_dev.c",
                    "drivers/cam_fd/cam_fd_context.c",
                ],
            },
            "CONFIG_SPECTRA_SENSOR": {
                True: [
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.c",
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_core.c",
                    "drivers/cam_sensor_module/cam_actuator/cam_actuator_soc.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_dev.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_core.c",
                    "drivers/cam_sensor_module/cam_cci/cam_cci_soc.c",
                    "drivers/cam_sensor_module/cam_sensor_lite/cam_sensor_lite_dev.c",
                    "drivers/cam_sensor_module/cam_sensor_lite/cam_sensor_lite_core.c",
                    "drivers/cam_sensor_module/cam_sensor_lite/cam_sensor_lite_soc.c",
                    "drivers/cam_sensor_module/cam_sensor_lite/cam_sensor_lite_pkt_utils.c",
                    "drivers/cam_sensor_module/cam_tpg/cam_tpg_dev.c",
                    "drivers/cam_sensor_module/cam_tpg/cam_tpg_core.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_0/tpg_hw_v_1_0.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_2/tpg_hw_v_1_2.c",
                    "drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_3/tpg_hw_v_1_3.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_soc.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_dev.c",
                    "drivers/cam_sensor_module/cam_csiphy/cam_csiphy_core.c",
                    "drivers/cam_sensor_module/cam_csiphy_remote/cam_csiphy_remote_soc.c",
                    "drivers/cam_sensor_module/cam_csiphy_remote/cam_csiphy_remote_dev.c",
                    "drivers/cam_sensor_module/cam_csiphy_remote/cam_csiphy_remote_core.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_dev.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_core.c",
                    "drivers/cam_sensor_module/cam_eeprom/cam_eeprom_soc.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_dev.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_core.c",
                    "drivers/cam_sensor_module/cam_ois/cam_ois_soc.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_dev.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_core.c",
                    "drivers/cam_sensor_module/cam_sensor/cam_sensor_soc.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_io.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_cci_i2c.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_qup_i2c.c",
                    "drivers/cam_sensor_module/cam_sensor_io/cam_sensor_spi.c",
                    "drivers/cam_sensor_module/cam_sensor_utils/cam_sensor_util.c",
                    "drivers/cam_sensor_module/cam_res_mgr/cam_res_mgr.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_dev.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_core.c",
                    "drivers/cam_sensor_module/cam_flash/cam_flash_soc.c",
                ],
            },
            "CONFIG_SPECTRA_CUSTOM": {
                True: [
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_soc.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_dev.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_core.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_csid/cam_custom_csid_dev.c",
                    "drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw_mgr.c",
                    "drivers/cam_cust/cam_custom_dev.c",
                    "drivers/cam_cust/cam_custom_context.c",
                ],
            },
            "CONFIG_QCOM_BUS_SCALING": {
                True: ["drivers/cam_utils/cam_soc_bus.c"],
            },
            "CONFIG_CAM_PRESIL": {
                # Sources need to be available to specify
                # True: [
                #     "drivers/cam_presil/presil/cam_presil_io_util.c",
                #     "drivers/cam_presil/presil/cam_presil_hw_access.c",
                #     "drivers/cam_presil/presil_framework_dev/cam_presil_framework_dev.c",
                # ],
                False: [
                    "drivers/cam_presil/stub/cam_presil_hw_access_stub.c",
                    "drivers/cam_utils/cam_io_util.c",
                ],
            },
        },
        copts = ["-include", "$(location :camera_banner)"],
        deps = deps,
        kconfig = "Kconfig",
        defconfig = "{}_defconfig".format(target),
        kernel_build = "//msm-kernel:{}".format(tv),
    )

    copy_to_dist_dir(
        name = "{}_camera_dist".format(tv),
        data = [":{}_camera".format(tv)],
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
    )

def define_camera_module():
    for (t, v) in get_all_variants():
        _define_module(t, v)
