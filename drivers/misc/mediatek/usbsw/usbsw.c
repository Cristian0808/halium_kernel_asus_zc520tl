//yutao add usb swtich file point begin 20160601
//#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/wakelock.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
//#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
//#include <linux/irq.h>




int usbsw_flag=0;
#define   USBSW_SWITCH_CLASS_NAME    "usbsw"
#define   USBSW_DEVICE_NODE_NAME   "usbsw"
#define   USBSW_DEVICE_CLASS_NAME  "usbsw" 
#define    USBSW_DEVICE_FILE_NAME   "usbsw"
 //dev_t usb_sw_dev=0;;
static struct class *usb_sw_class = NULL;
//static  struct device *usb_dev = NULL;

static struct platform_device * pltfm_dev ;
struct device* temp = NULL;  

struct pinctrl *usbswctrl = NULL;
struct pinctrl_state *usbsw_default= NULL;
struct pinctrl_state *usbsw_high= NULL;
struct pinctrl_state *usbsw_low= NULL;
static void usbsw_handle(void);
static DECLARE_TASKLET(usbsw_tasklet,usbsw_handle,0);

//static struct work_struct usbsw_init_work;
//static struct workqueue_struct *usbsw_init_workqueue;

static int usbsw_major=0;
static int usbsw_minor=0;

struct usb_switch_device
{
   
   int usbsw_val;
   struct semaphore sem;
   struct cdev dev;

};

struct usb_switch_device* usb_sw_dev =NULL;


static struct file_operations usbsw_fops={
	.owner=THIS_MODULE,
	
};

//extern init_MUTEX(struct semaphore sem);
static ssize_t _usbsw_show(struct usb_switch_device*dev,char*buf)
{
    int val=0;
	if(down_interruptible(&(dev->sem)))
		{
			return -ERESTARTSYS;
		}
	val=dev->usbsw_val;


	up(&(dev->sem));

	return snprintf(buf,PAGE_SIZE,"%d\n",val);  //from kernel space to user space

}

void usbsw_handle(void)
{
    int err = -1;
	if(usbsw_flag==1)
	{
	//printk("yutao now pull the otg sw pin high\n");
	pinctrl_select_state(usbswctrl,usbsw_high) ;
	err =0;
	}
	else 
	{
	//printk("yutao now pull the otg sw pin low\n");
	pinctrl_select_state(usbswctrl,usbsw_low) ;
	err=1;
	}

}

static void _usbsw_store(struct usb_switch_device *dev,const char * buf,size_t count)
{
	      int val=0;
	      val=simple_strtoul(buf, NULL, 10);
		//if(down_interruptible(&(dev->sem)))
		//  {
		//	return -ERESTARTSYS;
		// }
		
		   
	      dev->usbsw_val=val;
		  if(val==1)
		  {
		     usbsw_flag=1;
		     tasklet_schedule(&usbsw_tasklet);
			 }
		  else 
		  {
			 usbsw_flag=0;
		     tasklet_schedule(&usbsw_tasklet);
		  }
		 
		
	    //  up(&(dev->sem));
		
             // return count;

}

static ssize_t usbsw_show(struct device *dev, struct device_attribute *attr,char *buf)
{


	struct usb_switch_device *gdev=(struct usb_switch_device*)dev_get_drvdata(dev);
	return _usbsw_show(gdev,buf);
}

static void usbsw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	struct usb_switch_device *gdev=(struct usb_switch_device*)dev_get_drvdata(dev);
	
	 _usbsw_store(gdev,buf,size);
	
}

 static int _usbsw_setup_dev(struct usb_switch_device* dev)
 	{
 	
 	
 		int err;
		dev_t devno=MKDEV(usbsw_major,usbsw_minor);
		memset(dev,0,sizeof(struct usb_switch_device));
		cdev_init(&(dev->dev),&usbsw_fops);
		dev->dev.owner=THIS_MODULE;
		dev->dev.ops=&usbsw_fops;
		err=cdev_add(&(dev->dev),devno,1);
		if(err)
			{
				return err;

		       }
		dev->usbsw_val=0;
		sema_init(&(dev->sem),1);
		return 0;
 	 }
 static DEVICE_ATTR(usbsw_val, 0664 , usbsw_show, usbsw_store);
static int __init usbsw_driver_init(void)
{
    int err = -1;  
    dev_t dev = 0;  
   

   //printk(KERN_ALERT"Initializing usbsw device.\n");
   
   err = alloc_chrdev_region(&dev, 0, 1, USBSW_DEVICE_NODE_NAME); 
   
       if(err < 0) {  
       printk(KERN_ALERT"Failed to alloc char dev region.\n");  
        goto fail;  
		}
		
	usbsw_major=MAJOR(dev);
	usbsw_minor=MINOR(dev);
	
	usb_sw_dev=kmalloc(sizeof(struct usb_switch_device),GFP_KERNEL);
	    if(!usb_sw_dev) {  
       err = -ENOMEM;  
      // printk(KERN_ALERT"Failed to alloc usb_switch_dev.\n");  
        goto unregister;  
    }

	err=_usbsw_setup_dev(usb_sw_dev);
	   if(err) {  
      // printk(KERN_ALERT"Failed to setup dev: %d.\n", err);  
        goto cleanup;  
		} 
		
	usb_sw_class = class_create(THIS_MODULE, USBSW_DEVICE_CLASS_NAME);  
       if(IS_ERR(usb_sw_class)) {  
        err = PTR_ERR(usb_sw_class);  
      // printk(KERN_ALERT"Failed to create usbsw class.\n");  
       goto destroy_cdev;  
    } 

	   
	    temp = device_create(usb_sw_class, NULL, dev, "%s", USBSW_DEVICE_FILE_NAME);  
		
  if(IS_ERR(temp)) {  
       err = PTR_ERR(temp);  
       //printk(KERN_ALERT"Failed to create usbsw device.");  
        goto destroy_class;  
   }

	    err = device_create_file(temp, &dev_attr_usbsw_val);  
    if(err < 0) {  
      // printk(KERN_ALERT"Failed to create attribute val.");                  
       goto destroy_device;  
    }  
  
    dev_set_drvdata(temp, usb_sw_dev);  
	
  //  printk(KERN_ALERT"Initializing usbsw device  successfully.\n");
	
    return 0;
	
destroy_device:
     device_destroy(usb_sw_class,dev);	
destroy_class:
	class_destroy(usb_sw_class);
	

destroy_cdev:
	cdev_del(&(usb_sw_dev->dev));

cleanup:
	
	kfree(usb_sw_dev);
unregister:
     unregister_chrdev_region(MKDEV(usbsw_major, usbsw_minor), 1);    
		
fail:

     return err;


   
   
   

}

static void __exit usbsw_driver_exit(void)
{
	dev_t devno = MKDEV(usbsw_major, usbsw_minor);  
	//printk(KERN_ALERT"Destroy usbsw device.\n");
	
	  if(usb_sw_class) {  
        device_destroy(usb_sw_class, MKDEV(usbsw_major, usbsw_minor));  
        class_destroy(usb_sw_class);  
    } 

    if(usb_sw_dev) {  
        cdev_del(&(usb_sw_dev->dev));  
       kfree(usb_sw_dev);  
   }
unregister_chrdev_region(devno, 1); 
}



static int usbsw_probe(struct platform_device *dev)
{
	//printk("[usbsw]usbsw_probe begin!\n");
	pltfm_dev = dev;
	
	usbswctrl = devm_pinctrl_get(&pltfm_dev->dev);
	if (IS_ERR(usbswctrl)) {
		//printk(&pltfm_dev->dev, "Cannot find  usbsw pinctrl!");
	}
    usbsw_default = pinctrl_lookup_state(usbswctrl, "otgsw_pins_default");
	usbsw_low = pinctrl_lookup_state(usbswctrl, "otg_switch_enp0_gpio");
	if (IS_ERR(usbsw_low)) {
		//printk("%s : pinctrl err, usbsw_enable\n", __func__);
	}

	usbsw_high = pinctrl_lookup_state(usbswctrl, "otg_switch_enp1_gpio");
	if (IS_ERR(usbsw_low)) {
		//printk("%s : pinctrl err, usbsw_disable\n", __func__);
	}
	//printk("[usbsw]usbsw_probe done!\n");
	//pinctrl_select_state(usbswctrl,usbsw_high) ;
	return 0;
}
static int usbsw_remove(struct platform_device *dev)
{

	return 0;
}

struct of_device_id usbsw_of_match[]={
{.compatible = "mediatek,otgsw",},
{},
};
static struct platform_driver usbsw_driver= {
.probe=usbsw_probe,
.remove=usbsw_remove,
.driver={
.name="usbsw_drv",
.owner=THIS_MODULE,
.of_match_table=usbsw_of_match,
},
};
#if 0
static void usbsw_init_work_callback(void)
{
	printk("[usbsw]pull the pin high for test\n");
	pinctrl_select_state(usbswctrl,usbsw_high) ;
}
#endif
static int usbsw_pin_init(void)
{
	int ret = 0;

	//printk("[usbsw]usbsw_pin_init begin!!!!!!!!!!!!\n");
	ret = platform_driver_register(&usbsw_driver);
	if (ret!=0)
		printk("[usbsw]platform_driver_register error:%d\n", ret);
	else
		printk("[usbsw]platform_driver_register done!\n");

		//printk("[usbsw]usbsw_pin_init done!\n");
	//add test codes here to check the pin func
	//usbsw_init_workqueue = create_singlethread_workqueue("usbsw");
	//INIT_WORK(&usbsw_init_work, usbsw_init_work_callback);

	//ret = queue_work(usbsw_init_workqueue, &usbsw_init_work);
	if(!ret)
	printk("usbsw init work fail\n");
	return ret;

}
module_init(usbsw_pin_init);

late_initcall(usbsw_driver_init);
module_exit(usbsw_driver_exit);
//yutao add usb swtich file point end 20160601
