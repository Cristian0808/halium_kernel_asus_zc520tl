
#########################################################################
# Set Internal Variables                                                #
# May be modified to match your setup                                   #
#########################################################################
CFG_TEE_DRV_DEBUGFS?=0
CFG_TEE_CORE_LOG_LEVEL?=2
CFG_TEE_TA_LOG_LEVEL?=2

#ccflags-y+=-Werror
ccflags-y+=-I$(M)/include/arm_common
ccflags-y+=-I$(M)/include/linux
ccflags-y+=-I$(M)/include
ccflags-y+=-I$(M)/core

#wliang
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/include
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/armtz
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/core
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/armtz/libopensslrsa

ccflags-y+=-DCFG_TEE_DRV_DEBUGFS=${CFG_TEE_DRV_DEBUGFS}
ccflags-y+=-DCFG_TEE_CORE_LOG_LEVEL=${CFG_TEE_CORE_LOG_LEVEL}
ccflags-y+=-DCFG_TEE_TA_LOG_LEVEL=${CFG_TEE_TA_LOG_LEVEL}
ccflags-y+=-DCONFIG_NL_TCORE_SUPPORT


obj-y += libopensslrsa/

obj-y += nutlet_armtz.o



nutlet_armtz-objs:=   \
		tee_tz_drv.o \
		tee_mem.o \
		handle.o \
		tee_log.o


ifeq ($(CONFIG_ARM),y)
# "smc" assembly intruction requires dedicated "armv7 secure extension"
secext := $(call as-instr,.arch_extension sec,+sec)
AFLAGS_tee_smc-arm.o := -Wa,-march=armv7-a$(secext)
nutlet_armtz-objs += \
	tee_smc-arm.o
endif

ifeq ($(CONFIG_ARM64),y)
nutlet_armtz-objs += \
	tee_smc-arm64.o
endif

