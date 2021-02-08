#
# Makefile for Mediatek(R) MMC/SD host controller drivers
#

#add kernel source code path as head file search path
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/hibcore
obj-$(CONFIG_MMC_MTK) += $(subst ",,$(CONFIG_MTK_PLATFORM))/


ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/rpmb/drrpmb/public
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/rpmb/tlrpmb/public
endif

ccflags-y += -I$(srctree)
ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)
# zhaozhensen 20160524 begin
ifeq ($(CONFIG_NUTLET_TEE_SUPPORT),y)
#obj-$(CONFIG_MMC_MTK) += emmc_rpmb.o
else
obj-$(CONFIG_MMC_MTK) += emmc_rpmb.o
endif
# zhaozhensen 20160524 end

