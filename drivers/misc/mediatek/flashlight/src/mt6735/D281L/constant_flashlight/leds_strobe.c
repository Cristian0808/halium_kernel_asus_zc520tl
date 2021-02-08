//#liqiang@wind-mobi.com 20160102 begin
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_typedef.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>

#include <linux/proc_fs.h>



/******************************************************************************
 * Debug configuration
******************************************************************************/
/* availible parameter */
/* ANDROID_LOG_ASSERT */
/* ANDROID_LOG_ERROR */
/* ANDROID_LOG_WARNING */
/* ANDROID_LOG_INFO */
/* ANDROID_LOG_DEBUG */
/* ANDROID_LOG_VERBOSE */

#define TAG_NAME "[leds_strobe.c]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(TAG_NAME "%s: " fmt, __func__ , ##arg)

/*#define DEBUG_LEDS_STROBE*/
#ifdef DEBUG_LEDS_STROBE
#define PK_DBG PK_DBG_FUNC
#else
#define PK_DBG(a, ...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock);	/* cotta-- SMP proection */


static u32 strobe_Res;
static u32 strobe_Timeus;
static BOOL g_strobe_On;

static int g_duty = -1;
static int g_timeOutTimeMs;

static DEFINE_MUTEX(g_strobeSem);
static struct platform_device * pltfm_dev ;

struct pinctrl *flashlightctrl = NULL;
struct pinctrl_state *flashlight_enable = NULL;
struct pinctrl_state *flashlight_diable = NULL;
struct pinctrl_state *mode_enable = NULL;
struct pinctrl_state *mode_diable = NULL;
struct pinctrl_state *pwm_out= NULL;
//hebiao@wind-mobi.com 20160530 begin
#ifdef CONFIG_MTK_CHARGER_RED_DETECTC_OUT_HIGH
struct pinctrl_state *charger_red_high = NULL;
#endif
//hebiao@wind-mobi.com 20160530 end

static int hduration=-1;
static int lduration=-1;

extern int flashlight_set_pwm_old(u32 hduration, u32 lduration, u32 level);//hebiao@wind-mobi.com 20160219 begin

//ranyanhao@wind-mobi.com 20160331 begin
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT
static struct proc_dir_entry *asus_proc_flash_file = NULL;
static unsigned char asus_flash_brightness_flag = 0;
int FL_Enable(void);
int FL_Disable(void);
int FL_dim_duty(kal_uint32 duty);
char * asus_string_data = "NULL";
unsigned char torch_level = 0;
unsigned char asus_flash_lock = 0;

//modified by hebiao 20160606 begin
static ssize_t asus_flash_brightness_read(struct file *file, char *buf,
	size_t len, loff_t *pos)
{
	size_t ret = 0;
	char *ptr =buf;

	if (*pos)
		return 0;
	
	ptr += sprintf(ptr,"%s", asus_string_data);
	//ptr += sprintf(ptr, "\n");


	*pos += ptr - buf;

	return (ptr -buf);
}
//modified by hebiao 20160606 end

static ssize_t asus_flash_brightness_write(struct file *file, const char *buff,
	size_t len, loff_t *pos)
{
	char buf[5] = {0};
	unsigned char i = 0;

	if (len >= 5)
	{
		return -EFAULT;
	}
	if (copy_from_user(buf, buff, len))
	{
		return -EFAULT;
	}
	printk("ranyanhao app_write:%s\n",buf);

	torch_level = simple_strtol(buf, NULL, 0); //modified by hebiao 2016527

	
	printk("ranyanhao torch_level = %d\n",torch_level); 

	if(torch_level) {
		asus_flash_lock = 1;
		printk("ranyanhao FL_Enable\n");
		FL_dim_duty(0);
		FL_Enable();
	} else {
		asus_flash_lock = 0;
		printk("ranyanhao FL_Disable\n");
		FL_dim_duty(0);
		FL_Disable();
	}

	strcpy(asus_string_data, buf);
	
	return len;
}

static struct file_operations Asus_proc_asus_flash_brightness_ops =
{
	.owner = THIS_MODULE,
	.read  = asus_flash_brightness_read,
	.write = asus_flash_brightness_write,
};
#endif
//ranyanhao@wind-mobi.com 20160331 end

static int flashlight_probe(struct platform_device *dev)
{
	printk("[flashlight]flashlight_probe begin!\n");
	pltfm_dev = dev;
	
		flashlightctrl = devm_pinctrl_get(&pltfm_dev->dev);
	if (IS_ERR(flashlightctrl)) {
		dev_err(&pltfm_dev->dev, "Cannot find  flashlight pinctrl!");
	}
    /*Cam0 Power/Rst Ping initialization*/
	flashlight_enable = pinctrl_lookup_state(flashlightctrl, "state_enable_output1");
	if (IS_ERR(flashlight_enable)) {
		pr_debug("%s : pinctrl err, flashlight_enable\n", __func__);
	}

	flashlight_diable = pinctrl_lookup_state(flashlightctrl, "state_enable_output0");
	if (IS_ERR(flashlight_diable)) {
		pr_debug("%s : pinctrl err, flashlight_diable\n", __func__);
	}

	mode_enable = pinctrl_lookup_state(flashlightctrl, "state_mode_output1");
	if (IS_ERR(mode_enable)) {
		pr_debug("%s : pinctrl err, mode_enable\n", __func__);
	}

	mode_diable = pinctrl_lookup_state(flashlightctrl, "state_mode_output0");
	if (IS_ERR(mode_diable)) {
		pr_debug("%s : pinctrl err, mode_diable\n", __func__);
	}

	pwm_out = pinctrl_lookup_state(flashlightctrl, "state_pwm_output");
	if (IS_ERR(mode_diable)) {
		pr_debug("%s : pinctrl err, pwmoutput\n", __func__);
	}
	
//hebiao@wind-mobi.com 20160530 begin
#ifdef CONFIG_MTK_CHARGER_RED_DETECTC_OUT_HIGH
	charger_red_high = pinctrl_lookup_state(flashlightctrl, "charger_red_GPIO_output");
	if (IS_ERR(charger_red_high)) {
		pr_debug("%s : pinctrl err, charger_red_high\n", __func__);
	}
#endif
//hebiao@wind-mobi.com 20160530 end
	
//ranyanhao@wind-mobi.com 20160331 begin  
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT 
	asus_proc_flash_file = proc_create("driver/asus_flash_brightness", (S_IWUSR|S_IRUGO|S_IWUGO),
		NULL, &Asus_proc_asus_flash_brightness_ops);
	
	if(asus_proc_flash_file == NULL)
	{
		printk(" %s: proc asus_proc_flash file create failed!\n");
	}
#endif
//ranyanhao@wind-mobi.com 20160331 begin
	
	printk("[flashlight]flashlight_probe done!\n");
	return 0;
}

//hebiao@wind-mobi.com 20160530 begin
#ifdef CONFIG_MTK_CHARGER_RED_DETECTC_OUT_HIGH
int charger_red_output_high(void)
{
printk("func==%s line=%d charger_red_output_high begin\n", __func__, __LINE__); 
	pinctrl_select_state(flashlightctrl, charger_red_high);
printk("func==%s line=%d charger_red_output_high end\n", __func__, __LINE__); 
	return 0;

}
#endif
//hebiao@wind-mobi.com 20160530 begin


static int flashlight_remove(struct platform_device *dev)
{
//ranyanhao@wind-mobi.com 20160331 begin
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT
    remove_proc_entry( "driver/asus_flash_brightness", NULL);
#endif
//ranyanhao@wind-mobi.com 20160331 end
	return 0;
}


struct of_device_id flashlight_of_match[] = {
	{ .compatible = "mediatek,flashlight", },
	{},
};

static struct platform_driver flashlight_driver = {
	.probe = flashlight_probe,
	.remove = flashlight_remove,
	.driver = {
			.name = "flashlight_drv",
			.of_match_table = flashlight_of_match,
		   },
};
static int flashlight_mod_init(void)
{
	int ret = 0;

	printk("[flashlight]flashlight_mod_init begin!\n");
	ret = platform_driver_register(&flashlight_driver);
	if (ret)
		printk("[flashlight]platform_driver_register error:(%d)\n", ret);
	else
		printk("[Accdet]platform_driver_register done!\n");

		printk("[flashlight]flashlight_mod_init done!\n");
	return ret;

}

static void flashlight_mod_exit(void)
{
	printk("[Accdet]flashlight_mod_exit\n");
	platform_driver_unregister(&flashlight_driver);
	printk("[Accdet]flashlight_mod_exit Done!\n");
}

module_init(flashlight_mod_init);
module_exit(flashlight_mod_exit);

static struct work_struct workTimeOut;


/*****************************************************************************
Functions
*****************************************************************************/

int FL_Enable(void)
{
		
	int level =0;
/*
//ranyanhao@wind-mobi.com 20160406 begin
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT
    if (asus_flash_brightness_flag == 0) {
#endif
//ranyanhao@wind-mobi.com 20160406 end 
*/
	
	//torch mode
	if(g_duty==0){
	//PK_DBG("torch mode\n");	
	hduration = 49;
	lduration = 49;

	//hebiao@wind-mobi.com 20160527 begin
		if (asus_flash_lock) {
			pinctrl_select_state(flashlightctrl, flashlight_diable);
			msleep(1);
			pinctrl_select_state(flashlightctrl, mode_enable);
		} else {
			pinctrl_select_state(flashlightctrl, flashlight_diable);
			pinctrl_select_state(flashlightctrl, mode_diable);
			msleep(1);
			pinctrl_select_state(flashlightctrl, mode_enable);
			msleep(6);
			pinctrl_select_state(flashlightctrl, mode_enable); //zhaozhensen@wind-mobi.com 20170428 PWM->GPIO
		}
	//hebiao@wind-mobi.com 20160527 end
	}
	//flash mode
	else{
		//PK_DBG("flash mode\n");
		switch(g_duty){
			case 0:
				hduration = 9;
				lduration = 89;
				level = 10;
				break;
			case 1:
				hduration = 9;
				lduration = 89;
				level = 10;
				break;
			case 2:
				hduration = 19;
				lduration = 79;
				level = 20;
				break;
			case 3:
				hduration = 29;
				lduration = 69;
				level = 30;
				break;
			case 4:
				hduration = 39;
				lduration = 59;
				level = 40;
				break;
			case 5:
				hduration = 49;
				lduration = 49;
				level = 50;
				break;
			case 6:
				hduration = 59;
				lduration = 39;
				level = 60;
				break;
			case 7:
				hduration = 69;
				lduration = 29;
				level = 70;
				break;
			case 8:
				hduration = 79;
				lduration = 19;
				level = 80;
				break;
			case 9:
				hduration = 89;
				lduration = 9;
				level = 90;
				break;
			case 10:
				hduration = 97;
				lduration = 1;
				level = 98;
				break;
			default:
				hduration = 97;
				lduration = 1;
				level = 98;
				break;
		}

		pinctrl_select_state(flashlightctrl, flashlight_diable);
		msleep(6);
		pinctrl_select_state(flashlightctrl, mode_enable);
		pinctrl_select_state(flashlightctrl, pwm_out);
		flashlight_set_pwm_old(hduration,lduration,level);

		//into flash mode
		pinctrl_select_state(flashlightctrl, flashlight_enable);
	}
	
	printk("FL_enable end func==%s, line=%d\n", __func__, __LINE__);

	PK_DBG("FL_enable end line=%d\n", __LINE__);	
/*
//ranyanhao@wind-mobi.com 20160406 begin
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT
		asus_flash_brightness_flag ++; 
    } else {
	  printk("error,flash is opend%d\n");
    }
#endif
//ranyanhao@wind-mobi.com 20160406 end
*/

    return 0;
}



int FL_Disable(void)
{
/*
//ranyanhao@wind-mobi.com 20160406 begin
#ifdef CONFIG_WIND_ASUS_DEMAND_SUPPORT
	asus_flash_brightness_flag = 0; 
#endif
//ranyanhao@wind-mobi.com 20160406 end
*/
	pinctrl_select_state(flashlightctrl, flashlight_diable);
	pinctrl_select_state(flashlightctrl, mode_diable);

	PK_DBG(" FL_Disable line=%d\n",__LINE__);
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
	g_duty = duty;
    return 0;
}




int FL_Init(void)
{

    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	PK_DBG("FL_uninit");
	FL_Disable();
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
	FL_Disable();
	PK_DBG("ledTimeOut_callback\n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
	schedule_work(&workTimeOut);
	return HRTIMER_NORESTART;
}

static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
	INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs = 1000;
	hrtimer_init(&g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_timeOutTimer.function = ledTimeOutCallback;
}



static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("GPIO flash constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift,(int)arg);
    switch(cmd)
    {

	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n", (int)arg);
		g_timeOutTimeMs = arg;
		break;


	case FLASH_IOC_SET_DUTY:
		PK_DBG("FLASHLIGHT_DUTY: %d\n", (int)arg);
		FL_dim_duty(arg);
		break;


	case FLASH_IOC_SET_STEP:
		PK_DBG("FLASH_IOC_SET_STEP: %d\n", (int)arg);

		break;

	case FLASH_IOC_SET_ONOFF:
		PK_DBG("FLASHLIGHT_ONOFF: %d\n", (int)arg);
		if (arg == 1) {

			int s;
			int ms;

			if (g_timeOutTimeMs > 1000) {
				s = g_timeOutTimeMs / 1000;
				ms = g_timeOutTimeMs - s * 1000;
			} else {
				s = 0;
				ms = g_timeOutTimeMs;
			}

			if (g_timeOutTimeMs != 0) {
				ktime_t ktime;

				ktime = ktime_set(s, ms * 1000000);
				hrtimer_start(&g_timeOutTimer, ktime, HRTIMER_MODE_REL);
			}
			FL_Enable();
		} else {
			FL_Disable();
			hrtimer_cancel(&g_timeOutTimer);
		}
		break;
	default:
		PK_DBG(" No such command\n");
		i4RetValue = -EPERM;
		break;
	}
	return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
	int i4RetValue = 0;

	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res) {
		FL_Init();
		timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


	if (strobe_Res) {
		PK_DBG(" busy!\n");
		i4RetValue = -EBUSY;
	} else {
		strobe_Res += 1;
	}


	spin_unlock_irq(&g_strobeSMPLock);
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
	PK_DBG(" constant_flashlight_release\n");

	if (strobe_Res) {
		spin_lock_irq(&g_strobeSMPLock);

		strobe_Res = 0;
		strobe_Timeus = 0;

		/* LED On Status */
		g_strobe_On = FALSE;

		spin_unlock_irq(&g_strobeSMPLock);

		FL_Uninit();
	}

	PK_DBG(" Done\n");

	return 0;

}


FLASHLIGHT_FUNCTION_STRUCT constantFlashlightFunc = {
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &constantFlashlightFunc;
	return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

	return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);
//#liqiang@wind-mobi.com 20160102 end