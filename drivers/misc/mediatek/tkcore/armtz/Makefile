
ifneq ($(strip $(M)),)
	obj-m += tkcore_drv.o
else
	obj-y += tkcore_drv.o
endif

M ?= $(srctree)/drivers/misc/mediatek/tkcore

ccflags-y+=-I$(M)/include/arm_common
ccflags-y+=-I$(M)/include/linux
ccflags-y+=-I$(M)/include
ccflags-y+=-I$(M)/core

ifneq (,$(filter $(CONFIG_ARCH_MT6572) $(CONFIG_ARCH_MT6582) $(CONFIG_ARCH_MT6732) $(CONFIG_ARCH_MT6735) $(CONFIG_ARCH_MT6735M) $(CONFIG_ARCH_MT6737) $(CONFIG_ARCH_MT6738), y))
$(info Disable BL Schedule For Platform)
else
$(info Enable BL Schedule For Platform)
ccflags-y += -DTKCORE_BL
endif

ifeq ($(CONFIG_TRUSTKERNEL_TEE_FP_SUPPORT),y)
	tkcore-objs += tee_fp.o
	ccflags-y += -DTKCORE_FP_SUPPORT
endif

ifeq ($(CONFIG_TRUSTKERNEL_TEE_RPMB_SUPPORT),y)
ccflags-y += -DRPMB_SUPPORT
endif

tkcore_drv-objs:= \
		tee_tz_drv.o \
		tee_mem.o \
		handle.o \
		tee_procfs.o

ifeq ($(CONFIG_ARM),y)
# "smc" assembly intruction requires dedicated "armv7 secure extension"
secext := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_tee_smc-arm.o := -Wa,-march=armv7-a$(secext)
tkcore_drv-objs += \
	tee_smc-arm.o
endif

ifeq ($(CONFIG_ARM64),y)
tkcore_drv-objs += \
	tee_smc-arm64.o
endif

