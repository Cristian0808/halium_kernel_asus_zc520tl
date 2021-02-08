//liukangping 20160421 begin
/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <mt_spi.h>
#include <asm/atomic.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
//#include <mach/mt_spi.h>
//#include <mach/mt_gpio.h>
//#include <eint.h>
//#include <gpio_const.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/rtpm_prio.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <gpio_const.h>
#include "upmu_sw.h"
#include "upmu_common.h"
#include <mt_gpio.h>
//#include "cust_gpio_usage.h"  
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irqchip/mt-eic.h>

#include <linux/cdev.h>
#include "slspi.h"
#include "sl_proc.h"

#define VERBOSE  0
#define SL_MAX_FRAME_NUM 2

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
//#define SPIDEV_MAJOR			153	/* assigned */
#define N_SPI_MINORS			32	
#define SILEAD_PROC

#ifdef GSL6313_INTERRUPT_CTRL
#define PINCTRL_STATE_INTERRUPT "fp_interrupt"  /* [PM99] BUG#xxx Jonny_Chan IRQ wake-up control */
#endif
static DECLARE_BITMAP(minors, N_SPI_MINORS);
/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY)

#if 0
/// @ 2015 add by joker
static void dump_g_addr(char *ddr)
{
    int i=0;
    memset(tmp, 0, sizeof(tmp));
    for(i=0; i <32; ++i) {
        if (i %16==0) sprintf(tmp+strlen(tmp),"\n");
        sprintf(tmp+strlen(tmp), " %02x", ddr[i]); }
    sprintf(tmp+strlen(tmp),"\n");
    //printk(KERN_INFO "%s", tmp);
}
#endif
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct spidev_data	*fp_spidev = NULL;
static unsigned int spidev_major = 0;
static struct cdev spicdev;

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");


#define SL_READ  0x00 
#define SL_WRITE 0xFF 

static void spidev_work(struct work_struct *work);
static int spidev_setup_eint(struct spi_device *spi);
/* Note:this configuration is just for Mode 1 chip,such as 6163b,6172 */
struct mt_chip_conf chip_config = {
		.setuptime =10,//6, //10,//15,//10 ,//3,
		.holdtime =10, //6,//10,//15,//10,//3,
		.high_time =15,//4,//6,//8,//12, //25,//8,      //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
		.low_time = 15,//4,//6,//8,//12,//25,//8,
		.cs_idletime = 60,//30,// 60,//100,//12,
		.ulthgh_thrsh = 0,

		.rx_mlsb = SPI_MSB, 
		.tx_mlsb = SPI_MSB,		 
		.tx_endian = 0,
		.rx_endian = 0,

		.cpol = SPI_CPOL_0,
		.cpha = SPI_CPHA_0,
		
		.com_mod = DMA_TRANSFER,
		.pause = 0,
		.finish_intr = 1,
};
//set spi mode
static inline void spidev_schedule_work(struct spidev_data *spidev)
{
    if (work_pending(&spidev->work)) {
        return;
    }
    if (spidev->wqueue) {
        queue_work(spidev->wqueue, &spidev->work);
    } else {
        schedule_work(&spidev->work);
    }
}

static int  put_buffer(struct spidev_data *spidev)
{
    spidev->k_mmap_buf += sizeof(struct sl_frame);
    if (spidev->k_mmap_buf - spidev->mmap_buf == sizeof(struct sl_frame)*spidev->max_frame_num) {
        spidev->k_mmap_buf = spidev->mmap_buf;
    }

    atomic_inc(&spidev->frame_num);
    if (atomic_read(&spidev->frame_num) == spidev->max_frame_num-1) {//spidev->k_mmap_buf == spidev->u_mmap_buf) {
        //buffer is full
        dev_dbg(&spidev->spi->dev, "Receive buffer is full\n");
        return 0;
    }
    return 0;
}
static int get_buffer(struct spidev_data *spidev)
{
    int offset = -EAGAIN;
    if (atomic_read(&spidev->frame_num) == 0) {//buffer is empty
        dev_dbg(&spidev->spi->dev, "Receive buffer is empyt\n");
        spidev_schedule_work(spidev);
        return offset;
    }

    atomic_dec(&spidev->frame_num);
    offset = spidev->u_mmap_buf- spidev->mmap_buf;
    spidev->u_mmap_buf += sizeof(struct sl_frame);
    if ((spidev->u_mmap_buf - spidev->mmap_buf) == sizeof(struct sl_frame)*spidev->max_frame_num) {
        spidev->u_mmap_buf = spidev->mmap_buf;
    }
    spidev_schedule_work(spidev);
    return offset;
}


static int gsl_fp_rdinit(struct spidev_data *spidev, unsigned char reg)
{
    uint8_t tx[] = {
        reg, SL_READ,
    };
    unsigned rx[ARRAY_SIZE(tx)] = {0};
    struct spi_message	m;
    struct spi_transfer	t = {
        .rx_buf		= rx,
        .tx_buf		= tx,
        .len		= ARRAY_SIZE(tx),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}

void init_frame(struct spidev_data *spidev)
{
    unsigned int ret=0;
    unsigned long timeout;
// init page point
    spidev_write_reg(spidev, 0x00, 0xBF);
// start scanning
    spidev_write_reg(spidev, (0xFF080024>>7), 0xF0);
    spidev_write_reg(spidev, 0x2007FFFF, (0xFF080024%0x80));
    /* Wait  2 seconds for scanning done */
    timeout = jiffies + 2*HZ;
    while (time_before(jiffies, timeout)) {
        ret = spidev_read_reg(spidev, 0xBF);
        dev_dbg(&spidev->spi->dev, "0xBF=0x%02x\n", ret);
        if (ret != 0) {
            break;
        }
        udelay(100);
    }
    dev_dbg(&spidev->spi->dev, "last ret 0xBF=0x%02x\n", ret);
    spidev_write_reg(spidev, 0x00, 0xF0);
    gsl_fp_rdinit(spidev, 0);
}

static struct spi_transfer	t_silead[SL_ONE_FRAME_PAGES];

static void spidev_work(struct work_struct *work)
{
    struct spidev_data *spidev = container_of(work, struct spidev_data, work);
    struct spi_message	m;
    //struct spi_transfer	t[SL_ONE_FRAME_PAGES];
    int ret = 0, i;
    
	struct sched_param param = {.sched_priority = 1};
	sched_setscheduler(current, SCHED_RR, &param);

    if (atomic_read(&spidev->is_cal_mode)){
        return ;
    }

    t_silead[0].rx_buf	= spidev->k_mmap_buf;
    t_silead[0].tx_buf	= spidev->tx_mmap_buf;
    t_silead[0].len	= SL_HEAD_SIZE +SL_PAGE_SIZE;
    t_silead[0].bits_per_word = SPI_BITS;
    t_silead[0].delay_usecs = SPI_DELAY;
    t_silead[0].speed_hz = SPI_SPEED;
    spi_message_init(&m);
    spi_message_add_tail(&t_silead[0], &m);
    for (i=1; i < SL_ONE_FRAME_PAGES; ++i)
    {
        t_silead[i].rx_buf	= spidev->k_mmap_buf+i*SL_PAGE_SIZE + SL_HEAD_SIZE;
        t_silead[i].tx_buf	= spidev->tx_mmap_buf+i*SL_PAGE_SIZE + SL_HEAD_SIZE;
        t_silead[i].len    = SL_PAGE_SIZE;
        t_silead[i].bits_per_word = SPI_BITS;
        t_silead[i].delay_usecs = SPI_DELAY;
        t_silead[i].speed_hz = SPI_SPEED;
        spi_message_add_tail(&t_silead[i], &m);
    }

    if (!atomic_read(&spidev->frame_num)) {
        init_frame(spidev);
        ret = spidev_sync(spidev, &m);
        if (ret >0) {
            put_buffer(spidev);
        } else {
            dev_notice(&spidev->spi->dev, "sync fialed %d\n", ret);
        }
    } else {
        dev_dbg(&spidev->spi->dev, "Receive buffer is full=%d\n", atomic_read(&spidev->frame_num));
    }

    //if interrupt config, enable it
    //
}

static void spidev_irq_work(struct work_struct *work)
{
	struct spidev_data*		spidev = container_of(work,struct spidev_data,irq_work);
    char*					env_ext[2] = {"SILEAD_FP_EVENT=IRQ", NULL};
	printk("irq bottom half spidev_irq_work enter \n");
	kobject_uevent_env(&spidev->spi->dev.kobj, KOBJ_CHANGE, env_ext);

	return;
}

static irqreturn_t spidev_irq_routing(int irq, void* dev)
{
    struct spidev_data *spidev = fp_spidev;
	printk("irq top half spidev_irq_routing enter \n");
	disable_irq_nosync(spidev->irq);

	if(spidev->wqueue)
	{
#ifdef thomasDEBUG
		printk("now spidev->wqueue is not NULL!\n");
#endif
		queue_work(spidev->wqueue,&spidev->irq_work);
	}
	else
	{
#ifdef thomasDEBUG
		printk("now spidev->wqueue is NULL!\n");
#endif
		schedule_work(&spidev->irq_work);
	}

    //enable_irq(spidev->irq);
    return IRQ_HANDLED;

} 

/*-------------------------------------------------------------------------*/

#ifdef LSB_TO_MSB
static const unsigned char reverse_table256[] =   
{  
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,   
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,   
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,   
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,   
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,   
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,  
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,   
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,  
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,  
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,   
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,  
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,  
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,   
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,  
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,   
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF  
};  

static void Modifybuf(u8 *buf, size_t len)
{
    int i;

    for (i = 0; i < len; i++) {
        buf[i] = reverse_table256[buf[i]];
    }
}
#endif


/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void spidev_complete(void *arg)
{
    complete(arg);
}

ssize_t spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int status;

#ifdef LSB_TO_MSB
    struct list_head *p;
    struct spi_transfer *t;

    list_for_each(p, &message->transfers) {
        //pr_err("%s:%d\n", __func__, __LINE__);
        t = list_entry(p, struct spi_transfer, transfer_list);
        if (t->tx_buf) {
            Modifybuf((u8*)t->tx_buf, t->len);
        }
    }
#endif

    message->complete = spidev_complete;
    message->context = &done;

    if (spidev->spi == NULL) {
        status = -ESHUTDOWN;
    } else {
        spin_lock_irq(&spidev->spi_lock);
        status = spi_async(spidev->spi, message);
        spin_unlock_irq(&spidev->spi_lock);
		
    }
  
    if (status == 0) {
        //wait_for_completion(&done);------> deadlock
        /*unsigned long ret = */wait_for_completion_timeout(&done, msecs_to_jiffies(3000)); 
        status = message->status;
		
        if (status == 0)
            status = message->actual_length;

#ifdef LSB_TO_MSB
        list_for_each(p, &message->transfers) {
            //pr_err("%s:%d\n", __func__, __LINE__);
            t = list_entry(p, struct spi_transfer, transfer_list);
            if (t->rx_buf) {
                Modifybuf((u8*)t->rx_buf, t->len);
            }
        }
#endif
    }
   
    return status;
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
    struct spi_transfer	t = {
        .tx_buf		= spidev->buffer,
        .len		= len,
    };
    struct spi_message	m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
    struct spi_transfer	t = {
        .rx_buf		= spidev->buffer,
        .len		= len,
    };
    struct spi_message	m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}

static inline ssize_t
__spidev_sync_read(struct spidev_data *spidev, size_t offset, size_t len)
{
    struct spi_transfer	t = {
        .rx_buf		= spidev->mmap_buf + offset,
        .tx_buf		= spidev->mmap_buf + offset,
        .len		= len,
    };
    struct spi_message	m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spidev_sync(spidev, &m);
}


static inline ssize_t
__spidev_async_read(struct spidev_data *spidev, size_t offset, size_t len)
{

    struct spi_transfer	t = {
        .rx_buf		= spidev->mmap_buf + offset,
        .tx_buf		= spidev->mmap_buf + offset,
        .len		= len,
    };
    struct spi_message	m;

    //unsigned		is_dma_mapped:1;
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spidev_sync(spidev, &m);
}

static inline void sl_fp_read_init(struct spidev_data *spidev, u8 addr)
{
    struct spi_transfer		t;
    struct spi_message	m;
    u8 tx[6] = {0};
    u8 rx[6] = {0};

    tx[0] = addr;

    t.tx_buf = tx;
    t.rx_buf = rx;
    t.len = 6;
    t.bits_per_word = SPI_BITS;
    t.delay_usecs = SPI_DELAY;
    t.speed_hz = SPI_SPEED;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    spidev_sync(spidev, &m);
}

static inline ssize_t sl_fp_read(struct spidev_data *spidev, u8 addr, u8 *pdata, size_t len)
{
    ssize_t	status;
    struct spi_transfer		t;
    struct spi_message	m;
    u8 tx[SPI_BUF_SIZE + 3] = {0};
    u8 rx[SPI_BUF_SIZE + 3] = {0};
    u8 offset = 2;
    int i;

    if(len > SPI_BUF_SIZE) {
#ifdef thomasDEBUG
        printk("%s  too long len = %d!\n", __func__, len);
#endif
        return -1;
    }

    if(addr < 0x80) {
        offset = 3;
        sl_fp_read_init(spidev, addr);
    }

    tx[0] = addr;

    t.tx_buf = tx;
    t.rx_buf = rx;
    t.len = len + offset;
    t.bits_per_word = SPI_BITS;
    t.delay_usecs = SPI_DELAY;
    t.speed_hz = SPI_SPEED;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    status = spidev_sync(spidev, &m);
    if(status > 0) {
        for(i = 0; i < len; i ++) {
            *(pdata + i) = rx[i + offset];
        }
    } else {
#ifdef thomasDEBUG
        printk("%s  error status = %d!\n", __func__, status);
#endif
    }
    return status;
}

static inline ssize_t
sl_fp_write(struct spidev_data *spidev, uint8_t reg, uint32_t w_data)
{
    struct spi_transfer		t;
    struct spi_message	m;
    uint8_t tx[] = {
        reg, 0xFF,
        (w_data >>0) &0xFF,
        (w_data >>8) &0xFF,
        (w_data >>16) &0xFF,
        (w_data >>24) &0xFF,
    };
    u8 rx[sizeof(tx)] = {0};
    t.tx_buf = tx;
    t.rx_buf = rx;
    t.len = sizeof(tx);
    t.bits_per_word = SPI_BITS;
    t.delay_usecs = SPI_DELAY;
    t.speed_hz = SPI_SPEED;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spidev_sync(spidev, &m);
}

static union {
    unsigned char temp_char[4];
    unsigned int get_reg_data;
} temp_data;

unsigned int spidev_read_reg(struct spidev_data *spidev, unsigned char reg)
{
    struct spi_message	m;
    unsigned char rx[6];
    unsigned char tx[] = {
        reg, SL_READ, 0x00, 0x00, 0x00, 0x00
    };
    struct spi_transfer	t = {
        .rx_buf		= rx,
        .tx_buf		= tx,
        .len		= ARRAY_SIZE(tx),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };
    if (!(reg>0x80 && reg <0x100)) {
        gsl_fp_rdinit(spidev, reg);
    }
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    spidev_sync(spidev, &m);
    memcpy(temp_data.temp_char, (rx+2), 4);
    return temp_data.get_reg_data;
}

int spidev_write_reg(struct spidev_data *spidev, unsigned int data, unsigned char reg)
{
    struct spi_message	m;
    uint8_t rx[6] = {0};
    uint8_t tx[] = {
        reg, SL_WRITE,
        (data >>0) &0xFF,
        (data >>8) &0xFF,
        (data >>16) &0xFF,
        (data >>24) &0xFF,
    };
    struct spi_transfer	t = {
        .rx_buf		= rx,
        .tx_buf		= tx,
        .len		= ARRAY_SIZE(tx),
        .bits_per_word = SPI_BITS,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_SPEED,
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    return spidev_sync(spidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct spidev_data	*spidev;
    ssize_t			status = 0;
   
    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    spidev = filp->private_data;

    mutex_lock(&spidev->buf_lock);
    status = spidev_sync_read(spidev, count);
    if (status > 0) {
        unsigned long	missing;

        missing = copy_to_user(buf, spidev->buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }
    mutex_unlock(&spidev->buf_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
             size_t count, loff_t *f_pos)
{
    struct spidev_data	*spidev;
    ssize_t			status = 0;
    unsigned long		missing;
  
    /* chipselect only toggles at start or end of operation */
    if (count > bufsiz)
        return -EMSGSIZE;

    spidev = filp->private_data;

    mutex_lock(&spidev->buf_lock);
    missing = copy_from_user(spidev->buffer, buf, count);
    if (missing == 0) {
        status = spidev_sync_write(spidev, count);
    } else
        status = -EFAULT;
    mutex_unlock(&spidev->buf_lock);

    return status;
}

static int spidev_message(struct spidev_data *spidev,
                          struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message	msg;
    struct spi_transfer	*k_xfers;
    struct spi_transfer	*k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned		n, total;//,pf_tmp,pftmp;
    u8			*buf;
    int			status = -EFAULT;
   printk("%s +++ in --- 1 \n",__func__);
    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if (k_xfers == NULL)
        return -ENOMEM;

    /* Construct spi_message, copying any tx data to bounce buffer.
     * We walk the array of user-provided transfers, using each one
     * to initialize a kernel version of the same transfer.
     */
	
    buf = spidev->buffer;
	
    total = 0;
    for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
            n;
            n--, k_tmp++, u_tmp++) {
	
        k_tmp->len = u_tmp->len;
    
        total += k_tmp->len;
        if (total > bufsiz) {
            status = -EMSGSIZE;
            goto done;
        }

        if (u_tmp->rx_buf) {
            k_tmp->rx_buf = buf;
		
            if (!access_ok(VERIFY_WRITE, (u8 __user *)
                           (uintptr_t) u_tmp->rx_buf,
                           u_tmp->len)){
				
						   goto done;}
        }
        if (u_tmp->tx_buf) {
            k_tmp->tx_buf = buf;
			
            if (copy_from_user(buf, (const u8 __user *)
                               (uintptr_t) u_tmp->tx_buf,
                               u_tmp->len)){
								  
							   goto done;}
        }
	
#ifdef thomasDEBUG
		for(pftmp=0;pftmp<u_tmp->len;pftmp++){
		printk(KERN_WARNING "%u   function: %s\n",buf[pftmp],__FUNCTION__);
		}
#endif
        buf += k_tmp->len;
       
        k_tmp->cs_change = !!u_tmp->cs_change;
	
        k_tmp->bits_per_word = u_tmp->bits_per_word;
		
        k_tmp->delay_usecs = u_tmp->delay_usecs;
	
        k_tmp->speed_hz = u_tmp->speed_hz;
		
#ifdef thomasDEBUG
        dev_dbg(&spidev->spi->dev,
                "  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
                u_tmp->len,
                u_tmp->rx_buf ? "rx " : "",
                u_tmp->tx_buf ? "tx " : "",
                u_tmp->cs_change ? "cs " : "",
                u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
                u_tmp->delay_usecs,
                u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }

    //retval =  __spidev_sync_read(spidev, 0, tmp);
    status = spidev_sync(spidev, &msg);

    if (status < 0) {
        dev_err(&spidev->spi->dev, "spidev sync failed %d\n", status);
        goto done;
    }
	printk("%s +++ in --- 2 \n",__func__);
    /* copy any rx data out of bounce buffer */
    buf = spidev->buffer;
	
    for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
        if (u_tmp->rx_buf) {
            if (__copy_to_user((u8 __user *)
                               (uintptr_t) u_tmp->rx_buf, buf,
                               u_tmp->len)) {
                status = -EFAULT;
				
                goto done;
            }
		
#ifdef thomasDEBUG
			for(pf_tmp=0;pf_tmp<(u_tmp->len);pf_tmp++){			
			printk(KERN_WARNING "%u ",*((uint8_t *)(u_tmp->rx_buf)+pf_tmp));
			printk(KERN_WARNING "\n");
			}
#endif
        }
        buf += u_tmp->len;
    }
    status = total;

done:
    kfree(k_xfers);
	printk("%s +++ in --- over \n",__func__);
    return status;
}

static int spidev_mmap(struct file* filep, struct vm_area_struct *vma)
{
    struct spidev_data	*spidev = filep->private_data;

    vma->vm_flags |= VM_RESERVED;
    vma->vm_flags |= VM_LOCKED;
    if (NULL == spidev->mmap_buf) {
        dev_err(&spidev->spi->dev,"frame buffer is not alloc\n");
        return -ENOMEM;
    }
    return remap_pfn_range( vma, vma->vm_start,
                            virt_to_phys((void*)((unsigned long)spidev->mmap_buf))>>PAGE_SHIFT,
                            vma->vm_end - vma->vm_start, PAGE_SHARED);
}
/*oujiacheng 20160323 begin*/
static struct pinctrl *fp_pctrl; /* static pinctrl instance */

/* DTS state mapping name */
static const char *fp_state_name[2] = {
	"finger_rst_low",         
	"finger_rst_high"           
};

/* pinctrl implementation */
static long fp_set_state(const char *name)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;

	BUG_ON(!fp_pctrl);
	pState = pinctrl_lookup_state(fp_pctrl, name);
	if (IS_ERR(pState)) {
		pr_err("set state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	/* select state! */
	pinctrl_select_state(fp_pctrl, pState);

exit:
	return ret; /* Good! */
}

long fp_dts_gpio_init(struct platform_device *pdev)
{
	long ret = 0;
	struct pinctrl *pctrl;
//	printk("[D260_R]--%s----fp dts=-%s----\n",__func__,pdev->name);
	/* retrieve */
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		dev_err(&pdev->dev, "Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	fp_pctrl = pctrl;

exit:
	return ret;
}

static int fp_dts_probe(struct platform_device *dev)
{
	/* gpio setting */
	fp_dts_gpio_init(dev);
	
	return 0;
}

static int fp_dts_remove(struct platform_device *dev)
{

	return 0;
}

#ifdef CONFIG_OF
struct of_device_id fp_dts_of_match[] = {
	{ .compatible = "mediatek,fpreset", },
	{},
};
#endif

static struct platform_driver fp_dts_driver = {
	.probe = fp_dts_probe,
	.remove = fp_dts_remove,
	.driver = {
			.name = "fp_dts_platform_drv",
			#ifdef CONFIG_OF
			.of_match_table = fp_dts_of_match,
			#endif
		   },
};



static int spidev_reset_hw(struct spidev_data *spidev)
{
	int ret = 0;
	ret = fp_set_state(fp_state_name[0]);
	
//	printk("%s: reset gpio val = %d ---ret=%d- 1 \n",__func__, gpio_get_value(80),ret);
	mdelay(5);
	ret = 0;
	//mt_set_gpio_out(GPIO_FP_RST_PIN ,GPIO_OUT_ONE);
	ret = fp_set_state(fp_state_name[1]);
//	printk("%s: reset gpio val = %d  ---ret =%d- 2 \n",__func__, gpio_get_value(80),ret);
	return 0;
 
}

static int spidev_shutdown_hw(struct spidev_data *spidev)
{
	int ret = 0;
	ret = fp_set_state(fp_state_name[0]);
	mdelay(5);
//	printk("%s: reset gpio val = %d ---ret=%d- 1 \n",__func__, gpio_get_value(80),ret);
	return 0;
}
/*oujiacheng 20160323 end*/

static long spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int						err = 0;
    int						retval = 0;
    struct spidev_data		*spidev;
    struct spi_device		*spi;
    u32						tmp;
    unsigned				n_ioc;
    struct spi_ioc_transfer	*ioc;
 	printk("%s: +++ int !\n",__func__);
    /* Check type and command number */
    if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC){
	
	return -ENOTTY; }

    /* Check access direction once here; don't repeat below.
     * IOC_DIR is from the user perspective, while access_ok is
     * from the kernel perspective; so they look reversed.
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE,
                         (void __user *)arg, _IOC_SIZE(cmd));
    if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ,
                         (void __user *)arg, _IOC_SIZE(cmd));
    if (err){
		
	return -EFAULT; }

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
    spidev = filp->private_data;
	
    spin_lock_irq(&spidev->spi_lock);
    spi = spi_dev_get(spidev->spi);
    spin_unlock_irq(&spidev->spi_lock);
   
    if (atomic_read(&spidev->is_cal_mode)){
        dev_dbg(&spidev->spi->dev, "Current stat is cal mode\n");
		
        return -EBUSY;
    }
     
    if (atomic_read(&spidev->is_suspend)){
        dev_dbg(&spidev->spi->dev, "device is suspend\n");
	
        return -EBUSY;
    }

    if (spi == NULL){
		
        return -ESHUTDOWN;
    }

    /* use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
    mutex_lock(&spidev->buf_lock);
    switch (cmd) {
    case SPI_HW_RESET :
		spidev_reset_hw(spidev);
		break;
    case SPI_HW_SHUTDOWN:
		spidev_shutdown_hw(spidev);
		break;
    case SPI_SYNC_READ:
        retval =  __get_user(tmp,  (u32 __user *)arg);
        dev_dbg(&spi->dev, "SPI_SYNC_READ: pagesize=%d\n", tmp);
        if (retval == 0) {
            retval = __spidev_sync_read(spidev, 0, tmp);
        } else {
            dev_err(&spi->dev, "SPI_SYNC_READ:failed get_user\n");
        }
        break;
    case SPI_ASYNC_READ_PRE:
        dev_dbg(&spi->dev, "SPI_ASYNC_READ_PRE\n");
        spidev->k_mmap_buf = spidev->u_mmap_buf =spidev->mmap_buf;
        atomic_set(&spidev->frame_num, 0);
        cancel_work_sync(&spidev->work);
        spidev_schedule_work(spidev);
        retval = 0;
        break;
    case SPI_ASYNC_READ:
        dev_dbg(&spi->dev, "SPI_ASYNC_READ\n");
        retval = get_buffer(spidev);
        break;
    case SPI_GET_BUFFER_SIZE:
        dev_dbg(&spi->dev, "SPI_GET_BUFFER_SIZE\n");
        retval = __put_user(spidev->max_buf_size,
                            (__u32 __user *)arg);
        /* read requests */
    case SPI_IOC_RD_MODE:
        retval = __put_user(spi->mode & SPI_MODE_MASK,
                            (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_LSB_FIRST:
        retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
                            (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_BITS_PER_WORD:
        retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_MAX_SPEED_HZ:
        retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
        break;

        /* write requests */
    case SPI_IOC_WR_MODE:
        retval = __get_user(tmp, (u8 __user *)arg);
        if (retval == 0) {
            u8	save = spi->mode;

            if (tmp & ~SPI_MODE_MASK) {
                retval = -EINVAL;
                break;
            }

            tmp |= spi->mode & ~SPI_MODE_MASK;
            spi->mode = (u8)tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->mode = save;
            else
                dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
        }
        break;
    case SPI_IOC_WR_LSB_FIRST:
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0) {
            u8	save = spi->mode;

            if (tmp)
                spi->mode |= SPI_LSB_FIRST;
            else
                spi->mode &= ~SPI_LSB_FIRST;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->mode = save;
            else
                dev_dbg(&spi->dev, "%csb first\n",
                        tmp ? 'l' : 'm');
        }
        break;
    case SPI_IOC_WR_BITS_PER_WORD:
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0) {
            u8	save = spi->bits_per_word;

            spi->bits_per_word = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->bits_per_word = save;
            else
                dev_dbg(&spi->dev, "%d bits per word\n", tmp);
        }
        break;
    case SPI_IOC_WR_MAX_SPEED_HZ:
        retval = __get_user(tmp, (__u32 __user *)arg);
        if (retval == 0) {
            u32	save = spi->max_speed_hz;

            spi->max_speed_hz = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->max_speed_hz = save;
            else
                dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
        }
        break;
	case SPI_HW_IRQ_ENBALE:
#if  1
		if(arg)
		{
			//int mode
			printk("int mode");
			sl_fp_write(fp_spidev,0xbf,0);	
    			enable_irq(fp_spidev->irq);
			//mt_eint_unmask(spidev->irq);
		}
		else
		{
			//polling mode
			printk("polling mode");
			//mt_eint_mask(spidev->irq);
			//disable_irq_nosync(spidev->irq);
		}
#endif
        break;
    default:
        /* segmented and/or full-duplex I/O request */
        if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
                || _IOC_DIR(cmd) != _IOC_WRITE) {
            retval = -ENOTTY;
			
            break;
        }

        tmp = _IOC_SIZE(cmd);
		
        if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) {
            retval = -EINVAL;
			
            break;
        }
        n_ioc = tmp / sizeof(struct spi_ioc_transfer);
		
        if (n_ioc == 0){
			
		break; }

        /* copy into scratch area */
        ioc = kmalloc(tmp, GFP_KERNEL);
		
        if (!ioc) {
            retval = -ENOMEM;
			
            break;
        }
		
        if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
            kfree(ioc);
            retval = -EFAULT;
			
            break;
        }
        
        /* translate to spi_message, execute */
        retval = spidev_message(spidev, ioc, n_ioc);
        kfree(ioc);
		
        break;
    }

    mutex_unlock(&spidev->buf_lock);
    spi_dev_put(spi);
	
    return retval;
}

#if 1 //def CONFIG_COMPAT
static long
spidev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk("111111111111111111111111111111\n");
    return spidev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int spidev_open(struct inode *inode, struct file *filp)
{
    struct spidev_data	*spidev;
    int			status = -ENXIO;
	spidev = fp_spidev;
    if (atomic_read(&fp_spidev->is_cal_mode)){
        dev_dbg(&spidev->spi->dev, "Current stat is cal mode\n");
		
        return -EACCES;
    }

    mutex_lock(&device_list_lock);

    list_for_each_entry(spidev, &device_list, device_entry) {
        if (spidev->devt == inode->i_rdev) {
            status = 0;
            break;
        }
    }
	
    if (status == 0) {
        if (!spidev->buffer) {
            spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
            if (!spidev->buffer) {
                dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
                status = -ENOMEM;
            }
        }
        if (status == 0) {
            spidev->users++;
            filp->private_data = spidev;
            nonseekable_open(inode, filp);
        }
		
    } else{
		
	pr_debug("spidev: nothing for minor %d\n", iminor(inode));}

    mutex_unlock(&device_list_lock);
    return status;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
    struct spidev_data	*spidev;
    int			status = 0;

    mutex_lock(&device_list_lock);
    spidev = filp->private_data;
    filp->private_data = NULL;

    /* last close? */
    spidev->users--;
    if (!spidev->users) {
        int		dofree;
        if (spidev->buffer) {
            kfree(spidev->buffer);
            spidev->buffer = NULL;
        }
        /* ... after we unbound from the underlying device? */
        spin_lock_irq(&spidev->spi_lock);
        dofree = (spidev->spi == NULL);
        spin_unlock_irq(&spidev->spi_lock);

        if (dofree)
            kfree(spidev);
    }
    cancel_work_sync(&spidev->work);
    mutex_unlock(&device_list_lock);
    return status;
}

static const struct file_operations spidev_fops = {
    .owner =	THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
    .write =	spidev_write,
    .read =		spidev_read,
    .unlocked_ioctl = spidev_ioctl,
    .compat_ioctl = spidev_compat_ioctl,
    .open =		spidev_open,
    .release =	spidev_release,
    .llseek =	no_llseek,
    .mmap = spidev_mmap,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */



static struct class *spidev_class;
#ifdef thomasDEBUG
static void intToStr(unsigned int chipID,char * buf)
{
	int i;
	for(i=0;i<8;i++){
		if(((chipID >> 4*(7-i)) & 0xf)>=0 && ((chipID >> 4*(7-i)) & 0xf) <=9)
			buf[i]=((chipID >> 4*(7-i)) & 0xf) + '0';
		else
			buf[i]=(((chipID >> 4*(7-i)) & 0xf) -10)+ 'a';
	}
	buf[8]='\0';
	return ;
}
#endif		
		
/*-------------------------------------------------------------------------*/
static int  spidev_probe(struct spi_device *spi)
{
    struct spidev_data	*spidev;
    int			status;
    unsigned long		minor, page;
    int			error; //bacon 20160121
	int val=0;
	printk("wind-log_001");
    spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
    if (!spidev)
        return -ENOMEM;
    /* Initialize the driver data */
    spidev->spi = spi;
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	spidev_reset_hw(spidev);
    spin_lock_init(&spidev->spi_lock);
    mutex_init(&spidev->buf_lock);
    INIT_LIST_HEAD(&spidev->device_entry);
    INIT_WORK(&spidev->work, spidev_work);
	INIT_WORK(&spidev->irq_work,spidev_irq_work);
    wake_lock_init(&spidev->wake_lock, WAKE_LOCK_SUSPEND, "silead_wake_lock");
    spidev->wqueue = create_singlethread_workqueue("silead_wq");
    spidev->max_frame_num = SL_MAX_FRAME_NUM;
    spidev->max_buf_size =((sizeof(struct sl_frame)*spidev->max_frame_num+PAGE_SIZE)/PAGE_SIZE)*PAGE_SIZE;
    spidev->mmap_buf = kmalloc(spidev->max_buf_size, GFP_KERNEL);
    spidev->tx_mmap_buf = kmalloc(spidev->max_buf_size, GFP_KERNEL);
    memset(spidev->tx_mmap_buf, 0, spidev->max_buf_size);
    memset(spidev->mmap_buf, 0, spidev->max_buf_size);
    if (!spidev->mmap_buf) {
        dev_err(&spi->dev, "alloc kebuffer failedn\n");
		
        return -ENOMEM;
    }
    for(page = (unsigned long)spidev->mmap_buf;
            page < (unsigned long)spidev->mmap_buf+spidev->max_buf_size; page+= PAGE_SIZE) {
        SetPageReserved(virt_to_page(page));
    }
    /* If we can allocate a minor number, hook up this device.
     * Reusing minors is fine so long as udev or mdev is working.
     */
    mutex_lock(&device_list_lock);
    minor = find_first_zero_bit(minors, N_SPI_MINORS);
    if (minor < N_SPI_MINORS) {
        struct device *dev;

        spidev->devt = MKDEV(spidev_major, minor);
        dev = device_create(spidev_class, &spi->dev, spidev->devt,
                            spidev, "silead_fp_dev");
		
        status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
    } else {
        dev_dbg(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
		
    }
    if (status == 0) {
        set_bit(minor, minors);
        list_add(&spidev->device_entry, &device_list);
		
    }
    mutex_unlock(&device_list_lock);

    if (status == 0){
		printk("\n");
        spi_set_drvdata(spi, spidev);
		fp_spidev = spidev;
//#ifdef thomasDEBUG
		spidev_reset_hw(spidev);
		 val=spidev_read_reg(spidev, 0xfc);
		 
		 //liukangping 20160426 begin
		 printk(" spidev_read_reg val = %08x\n ",val);	
		 if((val & 0xffffff00) != 0x6163b000)
		 	{
		 	 printk("6163b compare fail");
		     return -ENODEV ;
		 	}
		 //liukangping 20160426 end
		 
		 //intToStr(val,tmp);
		 //tmp[8]='\0';
	//	printk(" [%s] the chip id = %s \n",__func__,tmp);	
//#endif
	#ifdef SILEAD_PROC
        sl_proc_init(spidev);
    #endif
        atomic_set(&spidev->is_cal_mode, 0);///default is enroll mode
        atomic_set(&spidev->is_suspend, 0);///default is enroll mode
		printk("wind-log_002");
		spidev_setup_eint(fp_spidev->spi);
		printk("wind-log_003");
	}
    else{
        kfree(spidev);
	
	}
   
    return status;
}


/*add for eint start*/
static int spidev_setup_eint(struct spi_device *spi)
{	
	struct spidev_data *spidev = fp_spidev;	
	int ret;
	u32 ints[2] = {0, 0};
	dev_err(&spi->dev, "silead: spidev_setup_eint\n");
	/* eint request */
	printk("wind-log_004");
	spidev->irq_node = of_find_compatible_node(NULL, NULL, "mediatek,finger_print-eint");
	printk("wind-log_005");
	dev_err(&spi->dev, "silead: spidev_setup_eint222\n");
	if (spidev->irq_node) {
	    printk("wind-log_006");
		of_property_read_u32_array(spidev->irq_node, "debounce", ints, ARRAY_SIZE(ints));
		gpio_request(ints[0], "finger_print");
		gpio_set_debounce(ints[0], ints[1]);
		spidev->wake_up_gpio = ints[0];
		dev_err(&spi->dev, "ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
		spidev->irq = irq_of_parse_and_map(spidev->irq_node, 0);
		dev_err(&spi->dev, "spidev->irq = %d\n", spidev->irq);
		if (!spidev->irq) {
		    printk("wind-log_007");
			dev_err(&spi->dev, "irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}

		if (request_irq(spidev->irq, spidev_irq_routing, IRQF_TRIGGER_NONE, "Silead-eint", NULL))	//IRQF_TRIGGER_RISING
		{
			dev_err(&spi->dev, "IRQ LINE NOT AVAILABLE!!\n");
			printk("wind-log_008");
			return -EINVAL;
		}
		fp_spidev->irq=spidev->irq;
		enable_irq(fp_spidev->irq);
		printk("wind-log_009");
	} else {
			dev_err(&spi->dev, "null irq node!!\n");
			printk("wind-log_010");
		   }
	return 0;
}
/*add for eint end*/


static int /*__devexit*/ spidev_remove(struct spi_device *spi)
{
    struct spidev_data	*spidev = spi_get_drvdata(spi);
    unsigned long page;

    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&spidev->spi_lock);
    spidev->spi = NULL;
    spi_set_drvdata(spi, NULL);
    spin_unlock_irq(&spidev->spi_lock);

    /* prevent new opens */
    mutex_lock(&device_list_lock);
    list_del(&spidev->device_entry);
    device_destroy(spidev_class, spidev->devt);
    clear_bit(MINOR(spidev->devt), minors);
    if (spidev->users == 0) {
        if (spidev->mmap_buf) {
            for(page = (unsigned long)spidev->mmap_buf;
                    page < (unsigned long)spidev->mmap_buf+spidev->max_buf_size; page+= PAGE_SIZE) {
                ClearPageReserved(virt_to_page(page));
            }

            kfree(spidev->mmap_buf);
        }
        if (spidev->tx_mmap_buf){
            kfree(spidev->tx_mmap_buf);
        }
        wake_lock_destroy(&spidev->wake_lock);
        kfree(spidev);
       
    }
    mutex_unlock(&device_list_lock);

    return 0;
}

static int spidev_suspend(struct spi_device *spi)
{
    return 0;
}
static int spidev_resume(struct spi_device *spi)
{
    return 0;
}

struct spi_device_id spi_silead_id = {"silead_fp",0};
#if 0
static const struct dev_pm_ops silead_pm = {
	.suspend = spidev_suspend,
	.resume  = spidev_resume
};
#endif
//"mediatek,finger_print-eint"
struct of_device_id silead_of_match[] = {
	{ .compatible = "mediatek,silead_fp", },
	{},
};



static struct spi_driver spidev_spi_driver = {
    .driver = {
        .name =		"silead_fp",   //spidev
		.bus = &spi_bus_type,
        .owner =	THIS_MODULE,
        .of_match_table = silead_of_match,//add for power on
//	.pm    =        &silead_pm
    },
    .probe =	spidev_probe,
    .remove =	__exit_p(spidev_remove),

//    .suspend  = spidev_suspend,
//    .resume   = spidev_resume,
	.id_table = &spi_silead_id,
    /* NOTE:  suspend/resume methods are not necessary here.
     * We don't do anything except pass the requests to/from
     * the underlying controller.  The refrigerator handles
     * most issues; the controller driver handles the rest.
     */
};

/*-------------------------------------------------------------------------*/

static struct spi_board_info spi_silead_board_info[] __initdata = {
  [0] = {
	    .modalias = "silead_fp",
		.max_speed_hz = 9000000,
		.bus_num = 0,
		.chip_select = 1,
		.mode = SPI_MODE_0, 
		.controller_data= &chip_config 
   },
};

static int __init spidev_init(void)
{
    int status;
    dev_t devno;
   
	spi_register_board_info(spi_silead_board_info,ARRAY_SIZE(spi_silead_board_info));

    /* Claim our 256 reserved device numbers.  Then register a class
     * that will key udev/mdev to add/remove /dev nodes.  Last, register
     * the driver which manages those device numbers.
     */
    BUILD_BUG_ON(N_SPI_MINORS > 256);
    printk("wind-log_011");
    status = alloc_chrdev_region(&devno, 0,255, "sileadfp");
    if(status <0 ){
		
         return status;}

    spidev_major = MAJOR(devno);
    cdev_init(&spicdev, &spidev_fops);
    spicdev.owner = THIS_MODULE;
    status = cdev_add(&spicdev,MKDEV(spidev_major, 0),N_SPI_MINORS);
	
    if(status != 0){
		printk("wind-log_012");
        return status;}

    spidev_class = class_create(THIS_MODULE, "spidev1");
	printk("wind-log_013");
    if (IS_ERR(spidev_class)) {
        unregister_chrdev(spidev_major, spidev_spi_driver.driver.name);
		printk("wind-log_014");
        return PTR_ERR(spidev_class);
    }

    status = spi_register_driver(&spidev_spi_driver);
	printk("wind-log_014");
    if (status < 0) {
		printk("%s:%d ++++++ init failed\n", __func__, __LINE__);
        class_destroy(spidev_class);
        unregister_chrdev(spidev_major, spidev_spi_driver.driver.name);
		
    }
	/*oujiacheng 20160323 begin*/
	int ret = 0;
	ret = platform_driver_register(&fp_dts_driver);
	if (ret)
		printk("[D260_R]fp_platform_driver_register error:(%d)\n", ret);
	else
		printk("[D260_R]fp_platform_driver_register done!\n");	
	/*oujiacheng 20160323 end*/
	
	printk("%s:%d ++++++ init ok\n", __func__, __LINE__);
    return status;
}

static void __exit spidev_exit(void)
{
    free_irq(fp_spidev->irq,fp_spidev);
    cdev_del(&spicdev);
    spi_unregister_driver(&spidev_spi_driver);
    class_destroy(spidev_class);
    unregister_chrdev(spidev_major, spidev_spi_driver.driver.name);
	platform_driver_unregister(&fp_dts_driver); //oujiacheng 20160323 add

}

module_init(spidev_init);
module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");
//liukangping 20160421 end
