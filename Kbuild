# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_QCOM_CAMERA_DEBUG), y)
$(info "CAMERA_KERNEL_ROOT is: $(CAMERA_KERNEL_ROOT)")
$(info "KERNEL_ROOT is: $(KERNEL_ROOT)")
endif

# Include Architecture configurations
ifeq ($(CONFIG_ARCH_TRINKET), y)
include $(CAMERA_KERNEL_ROOT)/config/trinket.mk
endif

# List of all camera-kernel headers
cam_include_dirs := $(shell dirname `find $(CAMERA_KERNEL_ROOT) -name '*.h'` | uniq)

# Include UAPI headers
USERINCLUDE +=                              \
	-I$(CAMERA_KERNEL_ROOT)/include/uapi/
# Include Kernel headers
LINUXINCLUDE +=                                 \
	-I$(KERNEL_ROOT)                            \
	-I$(KERNEL_ROOT)/arch/arm64/include         \
	-I$(KERNEL_ROOT)/include                    \
	-I$(KERNEL_ROOT)/include/media              \
	$(addprefix -I,$(cam_include_dirs))         \
	-I$(CAMERA_KERNEL_ROOT)/include/uapi/camera \
	-I$(CAMERA_KERNEL_ROOT)/include \
	-I$(CAMERA_KERNEL_ROOT)/

# After creating lists, add content of 'ccflags-m' variable to 'ccflags-y' one.
ccflags-y += ${ccflags-m}

camera-y += drivers/msm.o \
	drivers/camera/camera.o \
	drivers/common/cam_soc_icc.o \
	drivers/common/cam_hw_ops.o \
	drivers/common/cam_soc_api.o \
	drivers/common/msm_camera_io_util.o \
	drivers/common/msm_cam_cx_ipeak.o \
	drivers/common/msm_camera_tz_util.o \
	drivers/common/cam_smmu_api.o \
	drivers/msm_vb2/msm_vb2.o \
	drivers/msm_buf_mgr/msm_generic_buf_mgr.o \
	drivers/sensor/cci/msm_cci.o \
	drivers/sensor/io/msm_camera_cci_i2c.o \
	drivers/sensor/io/msm_camera_dt_util.o \
	drivers/sensor/io/msm_camera_qup_i2c.o \
	drivers/sensor/io/msm_camera_spi.o \
	drivers/sensor/io/msm_camera_tz_i2c.o \
	drivers/sensor/csid/msm_csid.o \
	drivers/sensor/csiphy/msm_csiphy.o \
	drivers/sensor/actuator/msm_actuator.o \
	drivers/sensor/eeprom/msm_eeprom.o \
	drivers/sensor/ois/msm_ois.o \
	drivers/sensor/flash/msm_flash.o \
	drivers/sensor/ir_led/msm_ir_led.o \
	drivers/sensor/ir_cut/msm_ir_cut.o \
	drivers/sensor/laser_led/msm_laser_led.o \
	drivers/sensor/msm_sensor.o \
	drivers/sensor/msm_sensor_driver.o \
	drivers/sensor/msm_sensor_init.o \
	drivers/isp/msm_buf_mgr.o  \
	drivers/isp/msm_isp_util.o  \
	drivers/isp/msm_isp_axi_util.o \
	drivers/isp/msm_isp_stats_util.o \
	drivers/isp/msm_isp48.o  \
	drivers/isp/msm_isp47.o  \
	drivers/isp/msm_isp46.o  \
	drivers/isp/msm_isp44.o  \
	drivers/isp/msm_isp40.o  \
	drivers/isp/msm_isp.o \
	drivers/pproc/cpp/msm_cpp.o \
	drivers/pproc/cpp/msm_cpp_soc.o \
	drivers/ispif/msm_ispif.o \
	drivers/jpeg_10/msm_jpeg_core.o \
	drivers/jpeg_10/msm_jpeg_dev.o \
	drivers/jpeg_10/msm_jpeg_hw.o \
	drivers/jpeg_10/msm_jpeg_platform.o \
	drivers/jpeg_10/msm_jpeg_sync.o \
	drivers/jpeg_dma/msm_jpeg_dma_dev.o \
	drivers/jpeg_dma/msm_jpeg_dma_hw.o \
	drivers/fd/msm_fd_dev.o \
	drivers/fd/msm_fd_hw.o

obj-m += camera.o
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/camera.ko
