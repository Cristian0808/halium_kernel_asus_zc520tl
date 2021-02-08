CFG_TEE_CORE_CORE_TARGET := armv7

#########################################################################
# Set Internal Variables                                                #
# May be modified to match your setup                                   #
#########################################################################
CFG_TEE_DRV_DEBUGFS?=0
CFG_TEE_CORE_LOG_LEVEL?=2
CFG_TEE_TA_LOG_LEVEL?=2

#ccflags-y+=-Werror
ccflags-y+=-I$(M)/include/linux
ccflags-y+=-I$(M)/include

#wliang
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/include
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/armtz
ccflags-y += -Idrivers/misc/mediatek/nutlet_linuxdriver/core

ccflags-y+=-DCFG_TEE_DRV_DEBUGFS=${CFG_TEE_DRV_DEBUGFS}
ccflags-y+=-DCFG_TEE_CORE_LOG_LEVEL=${CFG_TEE_CORE_LOG_LEVEL}
ccflags-y+=-DCFG_TEE_TA_LOG_LEVEL=${CFG_TEE_TA_LOG_LEVEL}

obj-y += nutlet.o

nutlet-objs:=   \
		tee_core.o  \
		tee_context.o  \
		tee_session.o  \
		tee_shm.o  \
		tee_supp_com.o  \
		tee_sysfs.o \
		tee_debugfs.o \
		tee_kernel_api.o \
		tee_mutex_wait.o


