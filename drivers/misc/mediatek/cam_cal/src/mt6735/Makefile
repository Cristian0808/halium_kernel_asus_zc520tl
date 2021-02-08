#
# Makefile for misc devices that really don't fit anywhere else.
# zhaozhensen 20160307 begin
include $(srctree)/drivers/misc/mediatek/imgsensor/src/Makefile.custom
# zhaozhensen 20160307 end
custom_cam_cal := $(addsuffix /, $(shell echo $(CONFIG_CUSTOM_KERNEL_CAM_CAL_DRV)))
obj-y += $(custom_cam_cal)
