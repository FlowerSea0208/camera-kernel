ENABLE_CAMERA_DLKM := false

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
ifeq ($(TARGET_KERNEL_DLKM_CAMERA_OVERRIDE), true)
ENABLE_CAMERA_DLKM := true
endif
else
ENABLE_CAMERA_DLKM := true
endif

ifeq ($(ENABLE_CAMERA_DLKM), true)
PRODUCT_PACKAGES += camera.ko
endif
