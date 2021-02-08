// add by zhaozhensen@wind-mobi.com 20170331 begin
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "kd_camera_hw.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "s5k3m2_otp.h"
#include "kd_camera_typedef.h"
#include <linux/dma-mapping.h>
#include "s5k3m2mipi_Sensor.h"
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define PFX "S5K3M2_OTP_FMT"

//#define Hynix_OTP_File_Load // eeprom_slim.txt file load. For SW Eng. 

#ifdef Hynix_OTP_File_Load
  #include <linux/kernel.h>
  #include <linux/init.h> 
  #include <linux/module.h> 
  #include <linux/syscalls.h>
  #include <linux/fcntl.h> 
  #include <asm/uaccess.h> 
  #include <linux/types.h> 
  #include <linux/slab.h> 
#endif 

/*******************************************************************************
*
********************************************************************************/

//#define CAM_CALGETDLT_DEBUG
#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
  #define CAM_CALINF(fmt, arg...)    pr_err("[%s] " fmt, __FUNCTION__, ##arg)
  #define CAM_CALDB(fmt, arg...)     pr_err("[%s] " fmt, __FUNCTION__, ##arg)
  #define CAM_CALERR(fmt, arg...)    pr_err("[%s] " fmt, __FUNCTION__, ##arg)
#else
  #define CAM_CALINF(x,...)
  #define CAM_CALDB(x,...)
  #define CAM_CALERR(fmt, arg...)    pr_err("[%s] " fmt, __FUNCTION__, ##arg)
#endif

#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)
#define CAM_CAL_ICS_REVISION 1 //seanlin111208
#define CAM_CAL_DRVNAME "S5K3M2_CAM_CAL_DRV"
#define CAM_CAL_I2C_GROUP_ID 0

#define OTP_READ_SKIP 0 
#define OTP_READ 1 

#define MAX_LSC_SIZE 1024
#define MAX_OTP_SIZE 2200

/*******************************************************************************
*
********************************************************************************/
static DEFINE_SPINLOCK(g_CAM_CALLock); // for SMP

static dev_t g_CAM_CALdevno = MKDEV(CAM_CAL_DEV_MAJOR_NUMBER,0);
static struct cdev * g_pCAM_CAL_CharDrv = NULL;

static struct class *CAM_CAL_class = NULL;
static atomic_t g_CAM_CALatomic;

static int have_read_S5k3m2_otp = 0;
extern void s5k3m2_OTP_write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
extern kal_uint16 s5k3m2_otp_read_8(kal_uint16 addr);
extern void s5k3m2_otp_write_8(kal_uint16 addr, kal_uint8 para);
//read_HynixOtp_sensor_8 -> read_Hi1332otp_8


typedef struct {
	u8     ModuleID;
	u8     Version;//0x01
	u8     AwbAfInfo;//0xF
	u8     wb_unit_rg_h;
	u8     wb_unit_rg_l;
	u8     wb_unit_bg_h;
	u8     wb_unit_bg_l;
	u8     wb_unit_gg_h;
	u8     wb_unit_gg_l;
	u8     wb_golden_rg_h;
	u8     wb_golden_rg_l;
	u8     wb_golden_bg_h;
	u8     wb_golden_bg_l;
	u8     wb_golden_gg_h;
	u8     wb_golden_gg_l;
	u8     GoldenAwbGbH;    //unused
	u8     GoldenAwbGbL;    //unused
	u8     GoldenAwbBH;     //unused
	u8     GoldenAwbBL;     //unused
	u16    AfInfinite;
	u16    AfMacro;
	u16    LscSize; // 21 22
	u8     Lsc[MAX_LSC_SIZE];
}OTP_MTK_TYPE;

OTP_MTK_TYPE   s5k3m2_otp_data;

/*******************************************************************************
*
********************************************************************************/

static void Enable_S5k3m2_OTP_Read( void)
{

    msleep(1);
    s5k3m2_OTP_write_cmos_sensor(0x0136, 0x1800);
    s5k3m2_OTP_write_cmos_sensor(0x0304, 0x0006);
    s5k3m2_OTP_write_cmos_sensor(0x0306, 0x0073);
    s5k3m2_OTP_write_cmos_sensor(0x030C, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030E, 0x0064);
    s5k3m2_OTP_write_cmos_sensor(0x0302, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0300, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030A, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0308, 0x0008);
    s5k3m2_OTP_write_cmos_sensor(0x0100, 0x0100);
	msleep(10);

}

static void Dsiable_S5k3m2_OTP_Read(void) 
{
    s5k3m2_otp_write_8(0x0A00, 0x00); // read disable
}

int read_S5k3m2_WB_Cal_Data(u8 enable) // seg 3 . WB only read wequence 
{
    
    msleep(1);
    s5k3m2_OTP_write_cmos_sensor(0x0136, 0x1800);
    s5k3m2_OTP_write_cmos_sensor(0x0304, 0x0006);
    s5k3m2_OTP_write_cmos_sensor(0x0306, 0x0073);
    s5k3m2_OTP_write_cmos_sensor(0x030C, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030E, 0x0064);
    s5k3m2_OTP_write_cmos_sensor(0x0302, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0300, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030A, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0308, 0x0008);
    s5k3m2_OTP_write_cmos_sensor(0x0100, 0x0100);
	msleep(10);
	s5k3m2_otp_write_8(0x0A02, 0x1F); //select page
    s5k3m2_otp_write_8(0x0A00, 0x01); // read enable
	if(s5k3m2_otp_read_8(0x0A08) == 0x01){
          s5k3m2_otp_data.wb_unit_rg_h   = s5k3m2_otp_read_8(0x0A10);
          s5k3m2_otp_data.wb_unit_rg_l   = s5k3m2_otp_read_8(0x0A11);
          s5k3m2_otp_data.wb_unit_bg_h   = s5k3m2_otp_read_8(0x0A12);
          s5k3m2_otp_data.wb_unit_bg_l   = s5k3m2_otp_read_8(0x0A13);
          s5k3m2_otp_data.wb_unit_gg_h   = s5k3m2_otp_read_8(0x0A14);
          s5k3m2_otp_data.wb_unit_gg_l   = s5k3m2_otp_read_8(0x0A15);
          s5k3m2_otp_data.wb_golden_rg_h = s5k3m2_otp_read_8(0x0A16);
          s5k3m2_otp_data.wb_golden_rg_l = s5k3m2_otp_read_8(0x0A17);
          s5k3m2_otp_data.wb_golden_bg_h = s5k3m2_otp_read_8(0x0A18);
          s5k3m2_otp_data.wb_golden_bg_l = s5k3m2_otp_read_8(0x0A19);
          s5k3m2_otp_data.wb_golden_gg_h = s5k3m2_otp_read_8(0x0A1A);
          s5k3m2_otp_data.wb_golden_gg_l = s5k3m2_otp_read_8(0x0A1B);
     } else if (s5k3m2_otp_read_8(0x0A1D) == 0x01) {
          s5k3m2_otp_data.wb_unit_rg_h   = s5k3m2_otp_read_8(0x0A25);
          s5k3m2_otp_data.wb_unit_rg_l   = s5k3m2_otp_read_8(0x0A26);
          s5k3m2_otp_data.wb_unit_bg_h   = s5k3m2_otp_read_8(0x0A27);
          s5k3m2_otp_data.wb_unit_bg_l   = s5k3m2_otp_read_8(0x0A28);
          s5k3m2_otp_data.wb_unit_gg_h   = s5k3m2_otp_read_8(0x0A29);
          s5k3m2_otp_data.wb_unit_gg_l   = s5k3m2_otp_read_8(0x0A2A);
          s5k3m2_otp_data.wb_golden_rg_h = s5k3m2_otp_read_8(0x0A2B);
          s5k3m2_otp_data.wb_golden_rg_l = s5k3m2_otp_read_8(0x0A2C);
          s5k3m2_otp_data.wb_golden_bg_h = s5k3m2_otp_read_8(0x0A2D);
          s5k3m2_otp_data.wb_golden_bg_l = s5k3m2_otp_read_8(0x0A2E);
          s5k3m2_otp_data.wb_golden_gg_h = s5k3m2_otp_read_8(0x0A2F);
          s5k3m2_otp_data.wb_golden_gg_l = s5k3m2_otp_read_8(0x0A30);
    }
    s5k3m2_otp_write_8(0x0A00, 0x00); // read disable

    return 0;
}



int read_S5k3m2_AF_Cal_Data(u8 enable)
{
	u16    AfInfinite;
	u16    AfMacro;

    msleep(1);
    s5k3m2_OTP_write_cmos_sensor(0x0136, 0x1800);
    s5k3m2_OTP_write_cmos_sensor(0x0304, 0x0006);
    s5k3m2_OTP_write_cmos_sensor(0x0306, 0x0073);
    s5k3m2_OTP_write_cmos_sensor(0x030C, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030E, 0x0064);
    s5k3m2_OTP_write_cmos_sensor(0x0302, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0300, 0x0004);
    s5k3m2_OTP_write_cmos_sensor(0x030A, 0x0001);
    s5k3m2_OTP_write_cmos_sensor(0x0308, 0x0008);
    s5k3m2_OTP_write_cmos_sensor(0x0100, 0x0100);
	msleep(10);
	s5k3m2_otp_write_8(0x0A02, 0x1F); //select page
    s5k3m2_otp_write_8(0x0A00, 0x01); // read enable
	if(s5k3m2_otp_read_8(0x0A32) == 0x01){
		  printk("zzs AF group1_data\n");
		  printk("zzs read test group1 bg_l 0x%x\n", s5k3m2_otp_read_8(0x0A19));
		  printk("zzs read test group1 afinfh 0x%x\n", s5k3m2_otp_read_8(0x0a33));
		  printk("zzs read test group1 afinfl 0x%x\n", s5k3m2_otp_read_8(0x0a34));
		  printk("zzs read test group1 afmach 0x%x\n", s5k3m2_otp_read_8(0x0a35));
		  printk("zzs read test group1 afmanl 0x%x\n", s5k3m2_otp_read_8(0x0a36));
          s5k3m2_otp_data.AfInfinite = ((s5k3m2_otp_read_8(0x0a33) << 8)&0xFF00)
						 |(s5k3m2_otp_read_8(0x0a34)&0x00FF);
          s5k3m2_otp_data.AfMacro    = ((s5k3m2_otp_read_8(0x0a35) << 8)&0xFF00)
						 |(s5k3m2_otp_read_8(0x0a36)&0x00FF);

     } else if (s5k3m2_otp_read_8(0x0A38) == 0x01) {
   		  printk("zzs AF group2_data\n");
		  printk("zzs read test group2 bg_l 0x%x\n", s5k3m2_otp_read_8(0x0A19));
		  printk("zzs read test group1 afinfh 0x%x\n", s5k3m2_otp_read_8(0x0A39));
		  printk("zzs read test group1 afinfl 0x%x\n", s5k3m2_otp_read_8(0x0A3A));
		  printk("zzs read test group1 afmach 0x%x\n", s5k3m2_otp_read_8(0x0A3B));
		  printk("zzs read test group1 afmanl 0x%x\n", s5k3m2_otp_read_8(0x0a3C));
          s5k3m2_otp_data.AfInfinite = ((s5k3m2_otp_read_8(0x0A39) << 8)&0xFF00)
						 |(s5k3m2_otp_read_8(0x0A3A)&0x00FF);
          s5k3m2_otp_data.AfMacro    = ((s5k3m2_otp_read_8(0x0A3B) << 8)&0xFF00)
						 |(s5k3m2_otp_read_8(0x0a3C)&0x00FF);
    } else {
			printk("neither group1 nor group2 has any data\n");
	}
    s5k3m2_otp_write_8(0x0A00, 0x00); // read disable
	printk("zzs 1 AfInfinite = %d, AfMacro = %d\n", s5k3m2_otp_data.AfInfinite, s5k3m2_otp_data.AfMacro);
	pr_err("zzs 1 AfInfinite = %d, AfMacro = %d\n", s5k3m2_otp_data.AfInfinite, s5k3m2_otp_data.AfMacro);
	CAM_CALERR("zzs 1 AfInfinite = %d, AfMacro = %d\n", s5k3m2_otp_data.AfInfinite, s5k3m2_otp_data.AfMacro);
	CAM_CALINF("zzs 1 AfInfinite = %d, AfMacro = %d\n", s5k3m2_otp_data.AfInfinite, s5k3m2_otp_data.AfMacro);
    return 0;
}

int read_S5k3m2_otp_mtk_fmt(void)
{ 
	int i = 0;
	int offset = 0;
    int rec = 0 ; 
    
	CAM_CALINF("[S5k3m2_otp] OTP (1) readed =%d \n",have_read_S5k3m2_otp);
	CAM_CALERR("[S5k3m2_otp] OTP (2) readed =%d \n",have_read_S5k3m2_otp);
	printk("Call read_S5k3m2_otp_mtk_fmt \n" );

	if(1 == have_read_S5k3m2_otp ) { // if "have_read_S5k3m2_otp"  value is 1, sKip read func
		CAM_CALDB("[zzs s5k3m2_otp] OTP readed ! skip\n");
        return 0;  // temp delete 
    }
   
    s5k3m2_otp_data.ModuleID = 0x32;
    s5k3m2_otp_data.Version = 0x01;
    s5k3m2_otp_data.AwbAfInfo = 0x03;

	spin_lock(&g_CAM_CALLock);
	have_read_S5k3m2_otp = 1; // Set To "1"
	spin_unlock(&g_CAM_CALLock);

    //Enable_Hi1332_OTP_Read(); // Write : OTP Enable Setting.

	//rec |= read_S5k3m2_WB_Cal_Data(OTP_READ);// seg 3 already apply in sensor
    rec |= read_S5k3m2_AF_Cal_Data(OTP_READ);// seg 4 

    if(rec < 0) // If Read Flag Fail, Disable MTK AF Cal. 
    {
    }

    //Dsiable_Hi1332_OTP_Read();
    
    return 0 ; 
}

#ifdef CONFIG_COMPAT
static int compat_put_cal_info_struct(
            COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
            stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;
	printk("zzs CONFIG_COMPAT\n");
	pr_err("zzs CONFIG_COMPAT\n");
    err = get_user(i, &data->u4Offset);
    err |= put_user(i, &data32->u4Offset);
    err |= get_user(i, &data->u4Length);
    err |= put_user(i, &data32->u4Length);
    /* Assume pointer is not change */
#if 1
    err |= get_user(p, &data->pu1Params);
    err |= put_user(p, &data32->pu1Params);
#endif
    return err;
}
static int compat_get_cal_info_struct(
            COMPAT_stCAM_CAL_INFO_STRUCT __user *data32,
            stCAM_CAL_INFO_STRUCT __user *data)
{
    compat_uptr_t p;
    compat_uint_t i;
    int err;

    err = get_user(i, &data32->u4Offset);
    err |= put_user(i, &data->u4Offset);
    err |= get_user(i, &data32->u4Length);
    err |= put_user(i, &data->u4Length);
    err |= get_user(p, &data32->pu1Params);
    err |= put_user(compat_ptr(p), &data->pu1Params);

    return err;
}

static long S5k3m2otp_Ioctl_Compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;
    COMPAT_stCAM_CAL_INFO_STRUCT __user *data32;
    stCAM_CAL_INFO_STRUCT __user *data;
    int err;

	CAM_CALDB("[S5k3m2_otp] S5k3m2otp_Ioctl_Compat\n" );

    if (!filp->f_op || !filp->f_op->unlocked_ioctl)
        return -ENOTTY;

    switch (cmd) {

    case COMPAT_CAM_CALIOC_G_READ:
    {
        data32 = compat_ptr(arg);
        data = compat_alloc_user_space(sizeof(*data));
        if (data == NULL)
            return -EFAULT;

        err = compat_get_cal_info_struct(data32, data);
        if (err)
            return err;

        ret = filp->f_op->unlocked_ioctl(filp, CAM_CALIOC_G_READ,(unsigned long)data);
        err = compat_put_cal_info_struct(data32, data);


        if(err != 0)
            CAM_CALERR("[s5k3m2_otp] compat_put_acdk_sensor_getinfo_struct failed\n");
        return ret;
    }
    default:
        return -ENOIOCTLCMD;
    }
}
#endif

static int selective_read_region(u32 offset, BYTE* data,u16 i2c_id,u32 size)
{    
    CAM_CALDB("[s5k3m2_otp] selective_read_region offset =%x size %d data read = %d\n", offset,size, *data);

    memcpy((void *)data,(void *)&s5k3m2_otp_data+offset,size);
    return size;
}

/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int CAM_CAL_Ioctl(struct inode * a_pstInode,
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
#else
static long CAM_CAL_Ioctl(
    struct file *file,
    unsigned int a_u4Command,
    unsigned long a_u4Param
)
#endif
{
    int i4RetValue = 0;
    u8 * pBuff = NULL;
    u8 * pu1Params = NULL;
    stCAM_CAL_INFO_STRUCT *ptempbuf;
#ifdef CAM_CALGETDLT_DEBUG
    struct timeval ktv1, ktv2;
    unsigned long TimeIntervalUS;
#endif

    CAM_CALDB("[s5k3m2_otp] CAM_CAL_Ioctl\n" );

    if(_IOC_NONE == _IOC_DIR(a_u4Command))
    {
    }
    else
    {
        pBuff = (u8 *)kmalloc(sizeof(stCAM_CAL_INFO_STRUCT),GFP_KERNEL);

        if(NULL == pBuff)
        {
            CAM_CALERR("[s5k3m2_otp] ioctl allocate mem failed\n");
            return -ENOMEM;
        }

        if(_IOC_WRITE & _IOC_DIR(a_u4Command))
        {
            if(copy_from_user((u8 *) pBuff , (u8 *) a_u4Param, sizeof(stCAM_CAL_INFO_STRUCT)))
            {    //get input structure address
                kfree(pBuff);
                CAM_CALERR("[s5k3m2_otp] ioctl copy from user failed\n");
                return -EFAULT;
            }
        }
    }

    ptempbuf = (stCAM_CAL_INFO_STRUCT *)pBuff;
    pu1Params = (u8*)kmalloc(ptempbuf->u4Length,GFP_KERNEL);
    if(NULL == pu1Params)
    {
        kfree(pBuff);
        CAM_CALERR("[s5k3m2_otp] ioctl allocate mem failed\n");
        return -ENOMEM;
    }


    if(copy_from_user((u8*)pu1Params ,  (u8*)ptempbuf->pu1Params, ptempbuf->u4Length))
    {
        kfree(pBuff);
        kfree(pu1Params);
        CAM_CALERR(" [s5k3m2_otp] ioctl copy from user failed\n");
        return -EFAULT;
    }

    switch(a_u4Command)
    {
        case CAM_CALIOC_S_WRITE:
            
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv1);
#endif
            i4RetValue = 0;//iWriteData((u16)ptempbuf->u4Offset, ptempbuf->u4Length, pu1Params);
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
#endif
            break;
        case CAM_CALIOC_G_READ:
            CAM_CALDB("[s5k3m2_otp] Read CMD \n");
#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv1);
#endif
            i4RetValue = selective_read_region(ptempbuf->u4Offset, pu1Params, S5K3M2_OTP_DEVICE_ID, ptempbuf->u4Length);

#ifdef CAM_CALGETDLT_DEBUG
            do_gettimeofday(&ktv2);
            if(ktv2.tv_sec > ktv1.tv_sec)
            {
                TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
            }
            else
            {
                TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;
            }
            CAM_CALDB("[s5k3m2_otp] Read data %d bytes take %lu us\n",ptempbuf->u4Length, TimeIntervalUS);
#endif

            break;
        default :
      	     CAM_CALINF("[s5k3m2_otp] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    if(_IOC_READ & _IOC_DIR(a_u4Command))
    {
        //copy data to user space buffer, keep other input paremeter unchange.
        if(copy_to_user((u8 __user *) ptempbuf->pu1Params , (u8 *)pu1Params , ptempbuf->u4Length))
        {
            kfree(pBuff);
            kfree(pu1Params);
            CAM_CALERR("[s5k3m2_otp] ioctl copy to user failed\n");
            return -EFAULT;
        }
    }

    kfree(pBuff);
    kfree(pu1Params);
    return i4RetValue;
}


static u32 g_u4Opened = 0;
//#define
//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
static int CAM_CAL_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    CAM_CALDB("[s5k3m2_otp] CAM_CAL_Open\n");
    spin_lock(&g_CAM_CALLock);
    if(g_u4Opened)
    {
        spin_unlock(&g_CAM_CALLock);
		CAM_CALERR("[s5k3m2_otp] Opened, return -EBUSY\n");
        return -EBUSY;
    }
    else
    {
        g_u4Opened = 1;
        atomic_set(&g_CAM_CALatomic,0);
    }
    spin_unlock(&g_CAM_CALLock);
    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int CAM_CAL_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    spin_lock(&g_CAM_CALLock);

    g_u4Opened = 0;

    atomic_set(&g_CAM_CALatomic,0);

    spin_unlock(&g_CAM_CALLock);

    return 0;
}

static const struct file_operations g_stCAM_CAL_fops =
{
    .owner = THIS_MODULE,
    .open = CAM_CAL_Open,
    .release = CAM_CAL_Release,
    //.ioctl = CAM_CAL_Ioctl
#ifdef CONFIG_COMPAT
    .compat_ioctl = S5k3m2otp_Ioctl_Compat,
#endif
    .unlocked_ioctl = CAM_CAL_Ioctl
};

#define CAM_CAL_DYNAMIC_ALLOCATE_DEVNO 1

inline static int RegisterCAM_CALCharDrv(void)
{
    struct device* CAM_CAL_device = NULL;
#if CAM_CAL_DYNAMIC_ALLOCATE_DEVNO
    if( alloc_chrdev_region(&g_CAM_CALdevno, 0, 1,CAM_CAL_DRVNAME) )
    {
        CAM_CALERR(" [s5k3m2_otp] Allocate device no failed\n");

        return -EAGAIN;
    }
#else
    if( register_chrdev_region(  g_CAM_CALdevno , 1 , CAM_CAL_DRVNAME) )
    {
        CAM_CALERR(" [s5k3m2_otp] Register device no failed\n");

        return -EAGAIN;
    }
#endif

    //Allocate driver
    g_pCAM_CAL_CharDrv = cdev_alloc();

    if(NULL == g_pCAM_CAL_CharDrv)
    {
        unregister_chrdev_region(g_CAM_CALdevno, 1);

        CAM_CALERR(" [s5k3m2_otp] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pCAM_CAL_CharDrv, &g_stCAM_CAL_fops);

    g_pCAM_CAL_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pCAM_CAL_CharDrv, g_CAM_CALdevno, 1))
    {
        CAM_CALERR(" [s5k3m2_otp] Attatch file operation failed\n");

        unregister_chrdev_region(g_CAM_CALdevno, 1);

        return -EAGAIN;
    }

    CAM_CAL_class = class_create(THIS_MODULE, "CAM_CALdrv_S5K3M2");
    if (IS_ERR(CAM_CAL_class)) {
        int ret = PTR_ERR(CAM_CAL_class);
        CAM_CALERR("[s5k3m2_otp] Unable to create class, err = %d\n", ret);
		printk("[s5k3m2_otp] Unable to create class, err = %d\n", ret);
		pr_err("[s5k3m2_otp] Unable to create class, err = %d\n", ret);
        return ret;
    }
    CAM_CAL_device = device_create(CAM_CAL_class, NULL, g_CAM_CALdevno, NULL, CAM_CAL_DRVNAME);
    CAM_CALDB("[s5k3m2_otp] RegisterCAM_CALCharDrv PASSS\n");

    return 0;
}

inline static void UnregisterCAM_CALCharDrv(void)
{
    //Release char driver
    cdev_del(g_pCAM_CAL_CharDrv);

    unregister_chrdev_region(g_CAM_CALdevno, 1);

    device_destroy(CAM_CAL_class, g_CAM_CALdevno);
    class_destroy(CAM_CAL_class);
}

static int CAM_CAL_probe(struct platform_device *pdev)
{

    return 0;//i2c_add_driver(&CAM_CAL_i2c_driver);
}

static int CAM_CAL_remove(struct platform_device *pdev)
{
    //i2c_del_driver(&CAM_CAL_i2c_driver);
    return 0;
}

// platform structure
static struct platform_driver g_stCAM_CAL_Driver = {
    .probe		= CAM_CAL_probe,
    .remove	= CAM_CAL_remove,
    .driver		= {
        .name	= CAM_CAL_DRVNAME,
        .owner	= THIS_MODULE,
    }
};


static struct platform_device g_stCAM_CAL_Device = {
    .name = CAM_CAL_DRVNAME,
    .id = 0,
    .dev = {
    }
};

static int __init CAM_CAL_init(void)
{
    int i4RetValue = 0;
    CAM_CALDB("[s5k3m2_otp]\n");
   //Register char driver
	i4RetValue = RegisterCAM_CALCharDrv();
    if(i4RetValue){
 	   CAM_CALDB("[s5k3m2_otp] register char device failed!\n");
	   return i4RetValue;
	}
	CAM_CALDB(" [s5k3m2_otp] Attached!! \n");

  //  i2c_register_board_info(CAM_CAL_I2C_BUSNUM, &kd_cam_cal_dev, 1);
    if(platform_driver_register(&g_stCAM_CAL_Driver)){
        CAM_CALERR("[s5k3m2_otp] failed to register s5k3m2 driver\n");
        return -ENODEV;
    }

    if (platform_device_register(&g_stCAM_CAL_Device))
    {
        CAM_CALERR("[s5k3m2_otp] failed to register s5k3m2 driver, 2nd time\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit CAM_CAL_exit(void)
{
	platform_driver_unregister(&g_stCAM_CAL_Driver);
}

module_init(CAM_CAL_init);
module_exit(CAM_CAL_exit);

MODULE_DESCRIPTION("CAM_CAL driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");
// add by zhaozhensen@wind-mobi.com 20170331 end

