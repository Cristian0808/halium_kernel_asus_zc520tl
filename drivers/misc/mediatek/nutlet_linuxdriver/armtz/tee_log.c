/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "linux_nutlet/tee_client_api.h"
#include "tee_shm.h"
#include "tee_supp_com.h"
#include "tee_mutex_wait.h"

#include "arm_common/teesmc.h"
#include "arm_common/teesmc_st.h"

#include "tee_mem.h"
#include "tee_tz_op.h"
#include "tee_tz_priv.h"
#include "handle.h"



#define DEBUG_MAX_RW_BUF 4096

/* TEE_LOG_BUF_SIZE = 32K */
#define TEE_LOG_BUF_SIZE 0x8000


/* TZ Diagnostic Area legacy version number */
#define TZBSP_DIAG_MAJOR_VERSION_LEGACY	2
/*
 * Preprocessor Definitions and Constants
 */
#define TZBSP_CPU_COUNT 0x02
/*
 * Number of VMID Tables
 */
#define TZBSP_DIAG_NUM_OF_VMID 16
/*
 * VMID Description length
 */
#define TZBSP_DIAG_VMID_DESC_LEN 7
/*
 * Number of Interrupts
 */
#define TZBSP_DIAG_INT_NUM  32
/*
 * Length of descriptive name associated with Interrupt
 */
#define TZBSP_MAX_INT_DESC 16
/*
 * VMID Table
 */
struct tzdbg_vmid_t {
	uint8_t vmid; /* Virtual Machine Identifier */
	uint8_t desc[TZBSP_DIAG_VMID_DESC_LEN];	/* ASCII Text */
};
/*
 * Boot Info Table
 */
struct tzdbg_boot_info_t {
	uint32_t wb_entry_cnt;	/* Warmboot entry CPU Counter */
	uint32_t wb_exit_cnt;	/* Warmboot exit CPU Counter */
	uint32_t pc_entry_cnt;	/* Power Collapse entry CPU Counter */
	uint32_t pc_exit_cnt;	/* Power Collapse exit CPU counter */
	uint32_t warm_jmp_addr;	/* Last Warmboot Jump Address */
	uint32_t spare;	/* Reserved for future use. */
};
/*
 * Reset Info Table
 */
struct tzdbg_reset_info_t {
	uint32_t reset_type;	/* Reset Reason */
	uint32_t reset_cnt;	/* Number of resets occured/CPU */
};
/*
 * Interrupt Info Table
 */
struct tzdbg_int_t {
	/*
	 * Type of Interrupt/exception
	 */
	uint16_t int_info;
	/*
	 * Availability of the slot
	 */
	uint8_t avail;
	/*
	 * Reserved for future use
	 */
	uint8_t spare;
	/*
	 * Interrupt # for IRQ and FIQ
	 */
	uint32_t int_num;
	/*
	 * ASCII text describing type of interrupt e.g:
	 * Secure Timer, EBI XPU. This string is always null terminated,
	 * supporting at most TZBSP_MAX_INT_DESC characters.
	 * Any additional characters are truncated.
	 */
	uint8_t int_desc[TZBSP_MAX_INT_DESC];
	uint64_t int_count[TZBSP_CPU_COUNT]; /* # of times seen per CPU */
};

/*
 * Log ring buffer position
 */
struct tzdbg_log_pos_t {
	uint16_t wrap;
	uint16_t offset;
};

 /*
 * Log ring buffer
 */
struct tzdbg_log_t {
	struct tzdbg_log_pos_t	log_pos;
	/* open ended array to the end of the 4K IMEM buffer */
	uint8_t					log_buf[];
};

/*
 * Diagnostic Table
 */
struct tzdbg_t {
	uint32_t magic_num;
	uint32_t version;
	/*
	 * Number of CPU's
	 */
	uint32_t cpu_count;
	/*
	 * Offset of VMID Table
	 */
	uint32_t vmid_info_off;
	/*
	 * Offset of Boot Table
	 */
	uint32_t boot_info_off;
	/*
	 * Offset of Reset info Table
	 */
	uint32_t reset_info_off;
	/*
	 * Offset of Interrupt info Table
	 */
	uint32_t int_info_off;
	/*
	 * Ring Buffer Offset
	 */
	uint32_t ring_off;
	/*
	 * Ring Buffer Length
	 */
	uint32_t ring_len;
	/*
	 * VMID to EE Mapping
	 */
	struct tzdbg_vmid_t vmid_info[TZBSP_DIAG_NUM_OF_VMID];
	/*
	 * Boot Info
	 */
	struct tzdbg_boot_info_t  boot_info[TZBSP_CPU_COUNT];
	/*
	 * Reset Info
	 */
	struct tzdbg_reset_info_t reset_info[TZBSP_CPU_COUNT];
	uint32_t num_interrupts;
	struct tzdbg_int_t  int_info[TZBSP_DIAG_INT_NUM];
	/*
	 * We need at least 2K for the ring buffer
	 */
	struct tzdbg_log_t ring_buffer;	/* TZ Ring Buffer */
};

/*
 * Enumeration order for VMID's
 */
enum tzdbg_stats_type {
//	TZDBG_BOOT = 0,
//	TZDBG_RESET,
//	TZDBG_INTERRUPT,
//	TZDBG_VMID,
//	TZDBG_GENERAL,
//	TZDBG_LOG,
	TZDBG_TEE_LOG = 0,
	TZDBG_STATS_MAX
};

struct tzdbg_stat {
	char *name;
	char *data;
};

struct tzdbg {
	void __iomem *virt_iobase;
	struct tzdbg_t *diag_buf;
	char *disp_buf;
	int debug_tz[TZDBG_STATS_MAX];
	struct tzdbg_stat stat[TZDBG_STATS_MAX];
};

struct logbuf_t
{
	struct list_head l;
	char *buf;
	int buf_len;
};

static struct tzdbg tzdbg = {
//	.stat[TZDBG_BOOT].name = "boot",
//	.stat[TZDBG_RESET].name = "reset",
//	.stat[TZDBG_INTERRUPT].name = "interrupt",
//	.stat[TZDBG_VMID].name = "vmid",
//	.stat[TZDBG_GENERAL].name = "general",
//	.stat[TZDBG_LOG].name = "log",
	.stat[TZDBG_TEE_LOG].name = "tee_log",
};


//static struct tzdbg_log_t *g_tee_log;


struct list_head logbuf_list;
struct dentry	*dent_dir;


/*
 * Debugfs data structure and functions
 */



static ssize_t tzdbgfs_read(struct file *file, char __user *buf,
	size_t count, loff_t *offp)
{
	int len = 0;
//	int *tee_id =  file->private_data;

	struct logbuf_t *lb;
	*offp = 0;

	if (list_empty(&logbuf_list))
	{
		return 0;
	}

	
	lb = list_first_entry(&logbuf_list, struct logbuf_t, l);

	len = simple_read_from_buffer(buf, lb->buf_len, offp, lb->buf, lb->buf_len);

	pr_info("[tzdbgfs_read] %s, lb->buf_len = %d, len = %d", lb->buf, lb->buf_len, len);

	list_del(&lb->l);
	kfree(lb);

	return len;
}

static int tzdbgfs_open(struct inode *inode, struct file *pfile)
{
	pfile->private_data = inode->i_private;
	return 0;
}

const struct file_operations tzdbg_fops = {
	.owner   = THIS_MODULE,
	.read    = tzdbgfs_read,
	.open    = tzdbgfs_open,
};

static int tzdbgfs_init(void)
{
	int rc = 0;
	int i;
	struct dentry           *dent;

	dent_dir = debugfs_create_dir("tzdbg", NULL);
	if (dent_dir == NULL) {
		pr_err("tzdbg debugfs_create_dir failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < TZDBG_STATS_MAX; i++) {
		tzdbg.debug_tz[i] = i;
		dent = debugfs_create_file(tzdbg.stat[i].name,
				S_IRUGO, dent_dir,
				&tzdbg.debug_tz[i], &tzdbg_fops);
		if (dent == NULL) {
			pr_err("TZ debugfs_create_file failed\n");
			rc = -ENOMEM;
			goto err;
		}
	}

	return 0;
err:
	debugfs_remove_recursive(dent_dir);

	return rc;
}

static void tzdbgfs_exit(void)
{
	debugfs_remove_recursive(dent_dir);
}

static void *tee_memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}


int tee_log_add(char *log, int len)
{
	struct logbuf_t *new_log;

	if (NULL == log)
	{
		pr_err("tee_log_add , inval buf.\n");
		return -EINVAL;
	}
	
	new_log = (struct logbuf_t *)kmalloc(sizeof(struct logbuf_t) + len, GFP_KERNEL);
	if (NULL == new_log)
	{
		pr_err("tee_log_add , no memory.\n");
		return -ENOMEM;
	}
	
	new_log->buf = (char *)new_log + sizeof(struct logbuf_t);
	new_log->buf_len = len;
	
	tee_memcpy(new_log->buf, log, len);

	list_add_tail(&new_log->l, &logbuf_list);
	
	pr_info("[tee_log_add] %s, lb->buf_len = %d, len = %d", new_log->buf, new_log->buf_len, len);
	
	return 0;
}


static int log_sys_ready_notify(struct tee_tz *ptee)
{
	struct smc_param param = { 0 };
	int ret = 0;

//	BUG_ON(!CAPABLE(ptee->tee));

	mutex_lock(&ptee->mutex);
	param.a0 = TEESMC32_ST_FASTCALL_LOGSYS_READY;
	printk( "configure_shm param.a0:%x\n", (uint)param.a0);
	tee_smc_call(&param);

	mutex_unlock(&ptee->mutex);

	if (param.a0 != TEESMC_RETURN_OK) {
		pr_err("shm service not available: %X", (uint)param.a0);
		ret = -EINVAL;
		goto out;
	}

out:
//	dev_dbg(DEV, "< ret=%d pa=0x%lX va=0x%p size=%zu, %scached",
//		ret, ptee->shm_paddr, ptee->shm_vaddr, shm_size,
//		(ptee->shm_cached == 1) ? "" : "un");
	return ret;
}


/*
 * Driver functions
 */
int tee_log_init(struct tee_tz *ptee)
{

	INIT_LIST_HEAD(&logbuf_list);

	if (tzdbgfs_init())
		goto err;

	log_sys_ready_notify(ptee);

	tee_log_add("logtest", 8);
	return 0;
	
err:
	return -ENXIO;
}


int tee_log_remove(void)
{
	tzdbgfs_exit();

	return 0;
}


//module_init(tee_log_init);
//module_exit(tee_log_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TZ Log driver");
MODULE_VERSION("1.1");
MODULE_ALIAS("platform:tee_log");

