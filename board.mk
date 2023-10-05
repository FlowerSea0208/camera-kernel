# Build camera kernel driver
ENABLE_CAMERA_DLKM := false

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
ifeq ($(TARGET_KERNEL_DLKM_CAMERA_OVERRIDE), true)
ENABLE_CAMERA_DLKM := true
endif
else
ENABLE_CAMERA_DLKM := true
endif

ifeq ($(ENABLE_CAMERA_DLKM),true)
ifneq ($(TARGET_BOARD_AUTO),true)
ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/camera.ko
endif
endif
endif
