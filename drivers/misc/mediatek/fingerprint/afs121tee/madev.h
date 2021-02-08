// zhaozhensen@wind-mobi.com 20160725 begin
/* MicroArray Fprint
 * madev.h
 * date: 2015-11-02
 * version: v2.3
 * Author: czl
 */

#ifndef MADEV_H
#define MADEV_H

//接口命令
#define IOCTL_DEBUG			0x100	//调试信息
#define IOCTL_IRQ_ENABLE	0x101	//中断使能
#define IOCTL_SPI_SPEED   	0x102	//SPI速度
#define IOCTL_READ_FLEN		0x103	//读帧长度(保留)
#define IOCTL_LINK_DEV		0x104	//连接设备(保留)
#define IOCTL_COVER_NUM		0x105	//材料编号
#define IOCTL_GET_VDATE		0x106	//版本日期

#define IOCTL_CLR_INTF		0x110	//清除中断标志
#define IOCTL_GET_INTF		0x111	//获取中断标志
#define IOCTL_REPORT_FLAG	0x112 	//上报标志
#define IOCTL_REPORT_KEY	0x113	//上报键值
#define IOCTL_SET_WORK		0x114	//设置工作
#define IOCTL_GET_WORK		0x115	//获取工作
#define IOCTL_SET_VALUE		0x116	//设值
#define IOCTL_GET_VALUE		0x117	//取值
#define IOCTL_TRIGGER		0x118	//自触发
#define IOCTL_WAKE_LOCK		0x119	//唤醒上锁
#define IOCTL_WAKE_UNLOCK	0x120	//唤醒解锁

#define IOCTL_SCREEN_ON		0x121

#define IOCTL_KEY_DOWN		0x121	//按下
#define IOCTL_KEY_UP		0x122	//抬起
#define IOCTL_SET_X			0x123	//偏移X
#define IOCTL_SET_Y			0x124	//偏移Y
#define IOCTL_KEY_TAP		0x125	//单击
#define IOCTL_KEY_DTAP		0x126	//双击
#define IOCTL_KEY_LTAP		0x127	//长按

#define IOCTL_ENABLE_CLK    0x128
#define TRUE 	1
#define FALSE 	0

#define FINGERPRINT_SWIPE_UP 	KEY_FN_F1//827
#define FINGERPRINT_SWIPE_DOWN 	KEY_FN_F2//828
#define FINGERPRINT_SWIPE_LEFT 	KEY_FN_F3//829
#define FINGERPRINT_SWIPE_RIGHT KEY_FN_F4//830
#define FINGERPRINT_TAP 		KEY_FN_F5//	831
#define FINGERPRINT_DTAP		KEY_FN_F6// 	832
#define FINGERPRINT_LONGPRESS 	KEY_FN_F7//833

#endif /* MADEV_H */


// zhaozhensen@wind-mobi.com 20160725 end
