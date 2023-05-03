ifneq ($(TARGET_DISABLE_AIS_DLKM),true)
# Build camera kernel driver
ifneq ($(TARGET_IS_HEADLESS),true)
ifeq ($(TARGET_BOARD_AUTO),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ais.ko
endif
endif #TARGET_IS_HEADLESS
endif # TARGET_DISABLE_AIS_DLKM
