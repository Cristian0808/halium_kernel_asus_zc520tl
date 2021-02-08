// zhaozhensen@wind-mobi.com 20160725 begin
/* MicroArray Fprint
 * madev.c
 * date: 2015-11-02
 * version: v2.1
 * Author: czl
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include "print.h"
#include "madev.h"
#include "plat-mtk.h"

#if 0
#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
#endif				

#if defined(CONFIG_MTK_CLKMGR)
 /* mt_clkmgr.h will be removed after CCF porting is finished. */
#include <mach/mt_clkmgr.h>
#endif				/* defined(CONFIG_MTK_CLKMGR) */
#endif

//ranyanhao@wind-mobi.com 20160704 begin
#ifdef CONFIG_WIND_DEVICE_INFO
extern char FP_id_str[20];
#endif
//ranyanhao@wind-mobi.com 20160704 end

//ranyanhao@wind-mobi.com 20160705 begin
#define Wind_FP_ATA
#ifdef Wind_FP_ATA
#include <linux/proc_fs.h>
static struct proc_dir_entry *fp_afs121_file = NULL;
static ssize_t fp_afs121_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	return 0;
}

static struct file_operations fp_afs121_ops =
{
	.owner = THIS_MODULE,
	.read  = fp_afs121_read,
};
#endif

#if 0
//ranyanhao@wind-mobi.com 20160705 end
struct mt_spi_t {
	struct platform_device *pdev;
	void __iomem *regs;
	int irq;
	int running;
	struct wake_lock wk_lock;
	struct mt_chip_conf *config;
	struct spi_master *master;

	struct spi_transfer *cur_transfer;
	struct spi_transfer *next_transfer;

	spinlock_t lock;
	struct list_head queue;
#if !defined(CONFIG_MTK_CLKMGR)
	struct clk *clk_main;	/* main clock for spi bus */
#endif				/* !defined(CONFIG_MTK_CLKMGR) */
};


static void enable_clk(struct mt_spi_t *ms)
{
#if (!defined(CONFIG_MT_SPI_FPGA_ENABLE))
#if defined(CONFIG_MTK_CLKMGR)
	enable_clock(MT_CG_PERI_SPI0, "spi");
#else
	int ret;
	ret = clk_enable(ms->clk_main);
#endif
#endif
}
#endif

struct fprint_dev {
	dev_t idd;
	struct cdev *chd;
	struct class *cls;
	struct device *dev;
};

struct fprint {
	struct fprint_dev sdev;
	uint8_t f_irq;		//中断标志
	struct input_dev *input;
	struct work_struct work;
	struct workqueue_struct *workq;
	struct wake_lock wl;
	struct wake_lock proccesslock;
    struct mt_spi_t *ms;
};
static struct fprint *smas = NULL;

static DECLARE_WAIT_QUEUE_HEAD(drv_waitq);

int polled = 0;
int report = 0;
int irq_flag = 0;
static void mas_work(struct work_struct *pws) {
	printd("%s: start.\n", __func__);
	//if(report) {
	//	report = 0;
		polled = 1;
		irq_flag = 1;
		wake_up_interruptible(&drv_waitq);
//	}

}

static irqreturn_t mas_interrupt(int irq, void *dev_id) {
	wake_lock_timeout(&smas->wl,5*HZ);
	printd("%s: start. f_irq=%d \n", __func__, smas->f_irq);
	queue_work(smas->workq, &smas->work);
	printd("%s: end. f_irq=%d\n", __func__, smas->f_irq);
	return IRQ_HANDLED;
}
/*
static int __init spi_clk_probe(struct spi_device *spi)
{
	if((spi != NULL) && (spi->master) != NULL){
		smas->ms = spi_master_get_devdata(spi->master);
	}
	enable_clk(smas->ms);
	return 0;
}

static struct spi_device_id spi_id_table = { "spi-clk", 0 };

static struct spi_driver spi_clk_driver= {
	.driver = {
		.name = "spi_clk",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		},
	.probe = spi_clk_probe,
	.id_table = &spi_id_table,
};
 */

static struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
	.modalias = "spi-clk",
	.bus_num = 0,
	.chip_select = 1,
	.mode = SPI_MODE_3,
	},
};

/*---------------------------------- fops ------------------------------------*/
int mas_ioctl(int cmd, int arg) {
	int ret = 0;
	static int x;
	printd("%s: start cmd=0x%.3x arg=%d\n", __func__, cmd, arg);

	switch (cmd) {
		case IOCTL_SCREEN_ON:
			input_report_key(smas->input, KEY_WAKEUP, 0);
			input_sync(smas->input);
			break;
		case IOCTL_SET_X:
			x = arg;
			break;
		case IOCTL_SET_Y:
			if( (x>=0? x : -x) >= (arg>=0? arg : -arg)){	// |x| > |y|
				if(x>=0){//up
					input_report_key(smas->input, FINGERPRINT_SWIPE_DOWN, 1);
					input_sync(smas->input);
					input_report_key(smas->input, FINGERPRINT_SWIPE_DOWN, 0);
					input_sync(smas->input);			
				}else{//down
					input_report_key(smas->input, FINGERPRINT_SWIPE_UP, 1);
					input_sync(smas->input);
					input_report_key(smas->input, FINGERPRINT_SWIPE_UP, 0);
					input_sync(smas->input);
				}
			}else{
				if(arg>=0){//left
					input_report_key(smas->input, FINGERPRINT_SWIPE_RIGHT ,1);
					input_sync(smas->input);
					input_report_key(smas->input, FINGERPRINT_SWIPE_RIGHT, 0);
					input_sync(smas->input);				
				}else{//right
					input_report_key(smas->input, FINGERPRINT_SWIPE_LEFT, 1);
					input_sync(smas->input);
					input_report_key(smas->input, FINGERPRINT_SWIPE_LEFT, 0);
					input_sync(smas->input);
				}
			}
			break;
		case IOCTL_KEY_TAP: //单击
			input_report_key(smas->input, FINGERPRINT_TAP, 1);
			input_sync(smas->input);
			input_report_key(smas->input, FINGERPRINT_TAP, 0);
			input_sync(smas->input);
			break;
		case IOCTL_KEY_DTAP: //双击
			input_report_key(smas->input, FINGERPRINT_DTAP, 1);
			input_sync(smas->input);
			input_report_key(smas->input, FINGERPRINT_DTAP, 0);
			input_sync(smas->input);
			break;
		case IOCTL_KEY_LTAP: //长按
			input_report_key(smas->input, FINGERPRINT_LONGPRESS, 1);
			input_sync(smas->input);
			input_report_key(smas->input, FINGERPRINT_LONGPRESS, 0);
			input_sync(smas->input);
			break;
		case IOCTL_ENABLE_CLK:
			/*printd("%s: enable clk\n",__func__);
#if !defined(CONFIG_MTK_CLKMGR)
			clk_prepare(smas->ms->clk_main);
#endif
			enable_clk(smas->ms); */
			break;
		case 0x129:
			printd("%s: ============+++++++++++++++++=================in\n",__func__);
			irq_flag = 0;
			wait_event_interruptible(drv_waitq, irq_flag != 0);
			printd("%s: ============+++++++++++++++++=================in after\n",__func__);
			break;
		case 0x130:
			printd("%s: ============+++++++++++++++++=================out\n",__func__);
			irq_flag = 1;	
			wake_up_interruptible(&drv_waitq);
			printd("%s: ============+++++++++++++++++=================out after\n",__func__);
			break;
		case 0x131:
/*			printd("%s: clk enable \n",__func__);
			enable_clk(smas->ms);
			udelay(5*1000);
			printd("%s: clk enable end\n",__func__); */
			break;
		case 0x132:
		
			plat_enable_irq(1);
			break;
		case 0x133:
		   plat_enable_irq(0);
		   
		case 0x134:
			wake_lock_timeout(&smas->proccesslock,1*HZ);
			break;
	}
	printd("%s: end. ret=%d\n", __func__, ret);
	return ret;
}

/* 写数据
 * @return 成功:count, -1count太大，-2拷贝失败
 */
static ssize_t mas_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
	int val=0, ret=0;

	printd("%s: start.\n", __func__);

	if(count==6) { 
		int cmd, arg;
		uint8_t tmp[6];
		ret = copy_from_user(tmp, buf, count);
		cmd = tmp[0];
		cmd <<= 8;
		cmd += tmp[1];
		arg = tmp[2];
		arg <<= 8;
		arg += tmp[3];
		arg <<= 8;
		arg += tmp[4];
		arg <<= 8;
		arg += tmp[5];
		printd("%s: cmd=0x%.3x arg=%d\n", __func__, cmd, arg);
		val = mas_ioctl(cmd, arg);
	}
	printd("%s: end. ret =%d\n", __func__, ret);
	return val;
}
/*
static unsigned int mas_poll(struct file *filp, struct poll_table_struct *wait) {
	unsigned int mask = 0;
	printd("%s\n start",__func__) ;
	poll_wait(filp, &drv_waitq, wait);
	mask = 0;
	if(polled) {
		polled = 0;
		mask |= POLLIN;
	}
	printd("%s\n end",__func__) ;
	return mask; 
}*/

static const struct file_operations sfops = {
	.owner = THIS_MODULE,
	.write = mas_write,
//	.poll = mas_poll,
};

static void mas_set_input(void) {
	struct input_dev *input = NULL;
	int ret = 0;

	printd("%s: start.\n", __func__);

	input = input_allocate_device();
	if (!input) {
		printw("%s: input_allocate_device failed.\n", __func__);
		return ;
	}
	set_bit(EV_KEY, input->evbit);
	set_bit(EV_ABS, input->evbit);
	set_bit(EV_SYN, input->evbit);
	set_bit(KEY_WAKEUP, input->keybit);
	set_bit(KEY_POWER, input->keybit);
	set_bit(FINGERPRINT_SWIPE_UP, input->keybit); 
	set_bit(FINGERPRINT_SWIPE_DOWN, input->keybit); 
	set_bit(FINGERPRINT_SWIPE_LEFT, input->keybit); 
	set_bit(FINGERPRINT_SWIPE_RIGHT, input->keybit); 
	set_bit(FINGERPRINT_TAP, input->keybit); 
	set_bit(FINGERPRINT_DTAP, input->keybit); 
	set_bit(FINGERPRINT_LONGPRESS, input->keybit); 

	input->name = "madev";
    input->id.bustype = BUS_SPI;
	ret = input_register_device(input);
    if (ret) {
        input_free_device(input);
        printw("%s: failed to register input device.\n",__func__);
        return;
    }
	smas->input  = input;

	printd("%s: end.\n", __func__);
}

/*---------------------------------- module ------------------------------------*/
static int __init mas_init(void) {
	int ret;
	
	char *temp_strptr;
	extern char *saved_command_line;

	printd("%s: start\n", __func__);
	smas = kmalloc(sizeof(struct fprint), GFP_KERNEL);
	if (smas==NULL) {
		printw("%s: smas kmalloc failed.\n", __func__);
		if(smas!=NULL) kfree(smas);
		return -ENOMEM;
	}
	smas->f_irq = FALSE;
	
	wake_lock_init(&smas->wl,WAKE_LOCK_SUSPEND,"ma_wakelock");
	wake_lock_init(&smas->proccesslock,WAKE_LOCK_SUSPEND,"proccess_wakelock");
	
	smas->sdev.chd = cdev_alloc();
	cdev_init(smas->sdev.chd, &sfops);
	alloc_chrdev_region(&(smas->sdev.idd), 0, 1, "madev");
	ret = cdev_add(smas->sdev.chd, smas->sdev.idd, 1);
	if (ret) {
		printw("%s: cdev_add failed. ret=%d\n", ret);
		return -1;
	}
	smas->sdev.cls = class_create(THIS_MODULE, "madev");
	if (IS_ERR(smas->sdev.cls)) {
		printw("%s: class_create failed.\n", __func__);
		return -1;
	}
	smas->sdev.dev = device_create(smas->sdev.cls, NULL, smas->sdev.idd, NULL, "madev");
	ret = IS_ERR(smas->sdev.dev) ? PTR_ERR(smas->sdev.dev) : 0;
	if (ret) {
		printw("%s: device_create failed. ret=%d\n", __func__, ret);
		goto fail;
	}
	mas_set_input();
	INIT_WORK(&smas->work, mas_work);
	ret = plat_request_irq(mas_interrupt, IRQF_TRIGGER_RISING);
	printd("%s: request_irq ret=%d\n", __func__, ret);
	if (ret) {
		printw("%s: request_irq failed.\n", __func__);
		goto fail;
	}
	smas->workq = create_singlethread_workqueue("mas_workqueue");
	if (!smas->workq) {
		printw("%s: create_single_workqueue failed\n", __func__);
		return -ENOMEM;
	}

	//liukangping@wind-mobi.com 20160416 end
	temp_strptr =  kzalloc(strlen(saved_command_line) + strlen(" androidboot.fptype=afs121") + 1,
			GFP_KERNEL);
	strcpy(temp_strptr, saved_command_line);
	strcat(temp_strptr, " androidboot.fptype=afs121");
	//kfree(saved_command_line);
	saved_command_line = temp_strptr;

#ifdef CONFIG_WIND_DEVICE_INFO
	sprintf(FP_id_str, "afs121");
#endif
	//ranyanhao@wind-mobi.com 20160704 end

	//ranyanhao@wind-mobi.com 20160705 begin
#ifdef Wind_FP_ATA
	fp_afs121_file = proc_create("fp_afs121", (S_IWUSR|S_IRUGO|S_IWUGO),
			NULL, &fp_afs121_ops);

	if(fp_afs121_file == NULL){
		printk(" %s: proc fp_afs121_file file create failed!\n",__func__);
	}
#endif
	printd("%s: end\n", __func__);

	//spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	//spi_register_driver(&spi_clk_driver);
	return ret;
fail:
	return -1;
}

static void __exit mas_exit(void) {
	printd("%s: start\n", __func__);
	wake_lock_destroy(&smas->wl);
	unregister_chrdev_region(smas->sdev.idd, 1);
	cdev_del(smas->sdev.chd);
	device_destroy(smas->sdev.cls, smas->sdev.idd);
	class_destroy(smas->sdev.cls);
	if(smas->workq) destroy_workqueue(smas->workq);
	if(smas!=NULL) kfree(smas);
	printd("%s: end\n", __func__);
	//spi_unregister_driver(&spi_clk_driver);
}

module_init(mas_init);
module_exit(mas_exit);

MODULE_AUTHOR("microarray");
MODULE_DESCRIPTION("for microarray fprint driver");
MODULE_LICENSE("GPL");
// zhaozhensen@wind-mobi.com 20160725 end
