//modified ranyanhao@wind-mobi.com 20160215 begin

//#ifdef BUILD_LK
//	#include <platform/mt_gpio.h>
//#elif defined(BUILD_UBOOT)
//	#include <asm/arch/mt_gpio.h>
//#else
//	#include <mach/mt_gpio.h>
//#endif

#ifdef BUILD_LK
#include <platform/gpio_const.h>
#include <platform/mt_gpio.h>
#include <platform/upmu_common.h>
#else
    #include <linux/string.h>
    #if defined(BUILD_UBOOT)
        #include <asm/arch/mt_gpio.h>
    #else
        #include <mt_gpio.h>
    #endif
#endif


#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1280)
#define LCM_ID_HX8394                                                              (0x0D)
#define REGFLAG_DELAY             								0xFC
#define REGFLAG_END_OF_TABLE      							0xFD   // END OF REGISTERS MARKER

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util;

#define __SAME_IC_COMPATIBLE__

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

//static struct platform_device * pltfm_dev ;

extern struct pinctrl *lcmbiasctrl ;
extern struct pinctrl_state *lcmbias_enable;
extern struct pinctrl_state *lcmbias_disable;

 struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting1[] = 
{
   
    //HX8394F+BOE5.2_20160301
	{0xB9, 3,{0xFF,0x83,0x94}},
	
	//0xSet0xPower,0x                              //0x54
	{0xB1, 15,{0x6C,0x12,0x12,0x23,0x04,0x11,0xF1,0x80,0xDA,0xDF,0x23,0x80,0xC0,0xD2,0x58}},

	//0xSet0xMIPI,0x
	{0xBA, 2,{0x73,0x83}},

	//0xSet0xDisplay,0x
	{0xB2, 11,{0x00,0x64,0x0E,0x0D,0x22,0x23,0x08,0x08,0x1C,0x4D,0x00}},

	//0xSet0xCYC,0x
	{0xB4, 12,{0x00,0xFF,0x5C,0x5A,0x5C,0x5A,0x5C,0x5A,0x01,0x70,0x01,0x70}},

	//0xSet0xD3,0x
	{0xD3, 32,{0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x32,0x10,0x02,0x00,0x02,0x32,0x13,0xC0,0x00,0x00,0x32,0x10,0x08,0x00,0x00,0x47,0x04,0x06,
			0x06,0x47,0x08,0x08,0x47,0x0C,0x08}},

	//0xSet0xGIP,0x
	{0xD5, 44,{0x01,0x00,0x03,0x02,0x05,0x04,0x07,0x06,0x21,0x20,0x23,0x22,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
			0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19}},

	//0xSet0xD6,0x
	{0xD6, 44,{0x06,0x07,0x04,0x05,0x02,0x03,0x00,0x01,0x22,0x23,0x20,0x21,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
			0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18}},

	//0xSet0xVCOM,0x
	//SSD2828_Gen_1A_2P(0xB6,0x4E,0x4E);//3c 3c

	// Set Gamma
	{0xE0, 42,{0x00,0x08,0x0B,0x33,0x3A,0x3F,0x19,0x3D,0x06,0x09,0x0D,0x17,0x0E,0x12,0x14,0x13,0x14,0x07,0x12,0x19,0x1D,0x00,0x07,0x0A,0x33,
			0x3A,0x3F,0x19,0x3D,0x07,0x09,0x0D,0x17,0x0F,0x12,0x15,0x12,0x13,0x06,0x13,0x1A,0x1E}},

			
	{0xC7, 4,{0x00,0xC0,0x40,0xC0}},		
			
			
	//0xSet0xPanel,0x
	{0xCC, 1,{0x0B}},  //ranyanhao@wind-mobi.com 20160302 modify the display direction

	//0xSet0xC0,0x
	{0xC0, 2,{0x30,0x14}},
     {0xB6, 2,{0x4E,0x4E}},
	

	{0x11, 1,{0x00}},
	
	{REGFLAG_DELAY, 120, {0}},	
			  
	{0x29, 1,{0x00}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}



};
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    //Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    // Display off sequence
    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
	//zhangaifeng@wind-mobi.com begin
   // {0xFF,	3,		{0x98,0x81,0x01}},
   // {0x58, 1, {0x01}},
//{REGFLAG_DELAY, 20, {}},
		//zhangaifeng@wind-mobi.com end
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{

		memset(params, 0, sizeof(LCM_PARAMS));
		params->type   = LCM_TYPE_DSI;


		//dingyisheng@wind-mobi.com 20160413 beign
		/*The following two values added are always uesd for CTS check */
		/*in order to match density and screen size*/
		params->physical_width = 65;
	       params->physical_height = 115 ;
             //dingyisheng@wind-mobi.com 20160413 end

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		params->dbi.te_mode				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		//LCM_DBI_TE_MODE_DISABLED;
		//LCM_DBI_TE_MODE_VSYNC_ONLY;  
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING; 
		/////////////////////   
		//if(params->dsi.lcm_int_te_monitor)  
		//params->dsi.vertical_frontporch *=2;  
		//params->dsi.lcm_ext_te_monitor= 0;//TRUE; 
	//	params->dsi.noncont_clock= TRUE;//FALSE;   
	//	params->dsi.noncont_clock_period=2;
//		params->dsi.cont_clock=1;
		////////////////////          
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;  
		// DSI    /* Command mode setting */  
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;      
		//The following defined the fomat for data coming from LCD engine.  
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;   
		params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST; 
		params->dsi.data_format.padding 	= LCM_DSI_PADDING_ON_LSB;    
		params->dsi.data_format.format	  = LCM_DSI_FORMAT_RGB888;       
		// Video mode setting		   
		params->dsi.intermediat_buffer_num = 2;  
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;  
		params->dsi.packet_size=256;    
		// params->dsi.word_count=480*3;	
		//DSI CMD mode need set these two bellow params, different to 6577   
		// params->dsi.vertical_active_line=800;   
		params->dsi.vertical_sync_active				= 4; //4   
		params->dsi.vertical_backporch				       = 12;  //14  
		params->dsi.vertical_frontporch				       = 10;  //16  
		params->dsi.vertical_active_line				       = FRAME_HEIGHT;     
		params->dsi.horizontal_sync_active				= 58;   //4
		params->dsi.horizontal_backporch				= 128;  //60  
		params->dsi.horizontal_frontporch				= 128;    //60
//		params->dsi.horizontal_blanking_pixel				= 60;   
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;  

	//	params->dsi.HS_TRAIL=14;
	//	params->dsi.pll_div1=1;		   
	//	params->dsi.pll_div2=1;		   
	//	params->dsi.fbk_div =28;//28	
//zhounengwen@wind-mobi.com 20150327 beign
// To fix lcm rf
        params->dsi.PLL_CLOCK = 208;

//		params->dsi.CLK_TRAIL = 17;
	
	    params->dsi.cont_clock=0;   
		
	    params->dsi.esd_check_enable = 1;
	    params->dsi.customization_esd_check_enable      = 1;

	    params->dsi.lcm_esd_check_table[0].cmd          = 0xd9;
	    params->dsi.lcm_esd_check_table[0].count        = 1;
	    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x80; //80
//hebiao@wind-mobi.com 20160324 begin
	    params->dsi.lcm_esd_check_table[1].cmd          = 0x09;
	    params->dsi.lcm_esd_check_table[1].count        = 3;
	    params->dsi.lcm_esd_check_table[1].para_list[0] = 0x80; 
	    params->dsi.lcm_esd_check_table[1].para_list[1] = 0x73; 
	    params->dsi.lcm_esd_check_table[1].para_list[2] = 0x04;
		 
		params->dsi.lcm_esd_check_table[2].cmd          = 0x45;
       		params->dsi.lcm_esd_check_table[2].count        = 2;
		params->dsi.lcm_esd_check_table[2].para_list[0] = 0x05;
		params->dsi.lcm_esd_check_table[2].para_list[1] = 0x17;
//hebiao@wind-mobi.com 20160324 end
}

static void init_lcm_registers(void)
	{
		unsigned int data_array[16];
	 
		data_array[0] = 0x00043902;
		data_array[1] = 0x9483FFB9;
		dsi_set_cmdq(&data_array, 2, 1);
		//MDELAY(1);
		
	  data_array[0] = 0x00033902;				
		data_array[1] = 0x008373BA;
		dsi_set_cmdq(&data_array,2, 1);
		//MDELAY(1);
	   
		data_array[0] = 0x00103902; 					
		data_array[1] = 0x0A0A6CB1;
		data_array[2] = 0xf1110423;
		data_array[3] = 0x23DFDA80;//0x23543A81
		data_array[4] = 0x58D2C080;
		dsi_set_cmdq(&data_array, 5, 1);
		//MDELAY(1);
		
		
		data_array[0] = 0x000c3902; 					
		data_array[1] = 0x0e6400B2;
		data_array[2] = 0x0823120d;
		data_array[3] = 0x004d1c08;
	  dsi_set_cmdq(&data_array, 4, 1);
		//MDELAY(1);
	   
		data_array[0] = 0x000d3902; 					
		data_array[1] = 0x5Cff00B4;
		data_array[2] = 0x5C5a5C5a;
		data_array[3] = 0x0170015a;
		data_array[4] = 0x00000070;
		dsi_set_cmdq(&data_array, 5, 1);
		//MDELAY(1);
		
	
		data_array[0] = 0x00213902; 					
		data_array[1] = 0x000000D3;
		data_array[2] = 0x08080000; 					
		data_array[3] = 0x00021032;
		data_array[4] = 0xC0133202; 					
		data_array[5] = 0x10320000;
		data_array[6] = 0x47000008; 					
		data_array[7] = 0x47060604;
		data_array[8] = 0x0C470808; 	
		data_array[9] = 0x00000008; 
		dsi_set_cmdq(&data_array, 10, 1);
		//MDELAY(1);
	   
	data_array[0] = 0x002d3902; 						
	data_array[1] = 0x030001D5;
	data_array[2] = 0x07040502;
	data_array[3] = 0x23202106;
	data_array[4] = 0x18181822;
	data_array[5] = 0x18181818;
	data_array[6] = 0x18181818;
	data_array[7] = 0x18181818;
	data_array[8] = 0x18181818;
	data_array[9] = 0x18181818;
	data_array[10] = 0x18181818;
	data_array[11] = 0x19181818;
	data_array[12] = 0x00000019;
	dsi_set_cmdq(&data_array, 13, 1);
	//MDELAY(1);
	 
	data_array[0] = 0x002d3902; 						
	data_array[1] = 0x040706D6;
	data_array[2] = 0x00030205;
	data_array[3] = 0x20232201;
	data_array[4] = 0x18181821;
	data_array[5] = 0x18181818;
	data_array[6] = 0x18181818;
	data_array[7] = 0x18181818;
	data_array[8] = 0x18181818;
	data_array[9] = 0x18181818;
	data_array[10] = 0x18181818;
	data_array[11] = 0x18191918;
	data_array[12] = 0x00000018;
	dsi_set_cmdq(&data_array, 13, 1);
	//MDELAY(1);
	
#if 1	
	data_array[0] = 0x002b3902; 						
	data_array[1] = 0x1E1902E0;
	data_array[2] = 0x273F322E;
	data_array[3] = 0x0D0A0743;
	data_array[4] = 0x14120E18;
	data_array[5] = 0x12071413;
	data_array[6] = 0x19021713;
	data_array[7] = 0x3F322E1E;
	data_array[8] = 0x0A074327;
	data_array[9] = 0x120E180D;
	data_array[10] = 0x07141314;
	data_array[11] = 0x00171312;
	dsi_set_cmdq(&data_array, 12, 1);
	// MDELAY(1);
#endif	

#if 0		 
	data_array[0] = 0x002b3902; 						
	data_array[1] = 0x383428E0;
	data_array[2] = 0x3C3F3A33;
	data_array[3] = 0x0D090950;
	data_array[4] = 0x13110E17;
	data_array[5] = 0x11091312;
	data_array[6] = 0x34280F19;
	data_array[7] = 0x3F3A3338;
	data_array[8] = 0xE929503C;
	data_array[9] = 0x310E370D;
	data_array[10] = 0x0913F313;
	data_array[11] = 0x000F1911;
	dsi_set_cmdq(&data_array, 12, 1);
	// MDELAY(1); 
#endif	
 
	data_array[0] = 0x09CC1500;// 			
	dsi_set_cmdq(&data_array, 1, 1);
  //MDELAY(1);
	
	data_array[0] = 0x00053902; 						
	data_array[1] = 0x40c000c7;
	data_array[2] = 0x000000c0;
	dsi_set_cmdq(&data_array, 3, 1);
	//MDELAY(1);
	
	
	data_array[0] = 0x00033902; 						
	data_array[1] = 0x001430C0;
	dsi_set_cmdq(&data_array, 2, 1);
	//MDELAY(1);
	
		
#if 0
	 data_array[0] = 0x07BC1500;//			  
	 dsi_set_cmdq(&data_array, 1, 1);
		
	data_array[0] = 0x11D21500;// 			
	dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(1);	
#endif	

	data_array[0] = 0x00033902; 						
    data_array[1] = 0x004E4EB6;
	dsi_set_cmdq(&data_array, 2, 1);
	//MDELAY(1);
		
	
	data_array[0] = 0x00110500;//			
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);
	
	data_array[0] = 0x00290500;   
	dsi_set_cmdq(&data_array, 1, 1);
     //dingyisheng@wind-mobi.com 20160812 begin
     //MDELAY(50);
    //dingyisheng@wind-mobi.com 20160812 end
	}


static void lcm_init(void)
{
#ifdef GPIO_LCD_BIAS_ENP_PIN
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
#endif
	pinctrl_select_state(lcmbiasctrl, lcmbias_enable); 
//dingyisheng@wind-mobi.com 20160812 begin
	MDELAY(10);
//dingyisheng@wind-mobi.com 20160812 end
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);
//	printk(" gemingming hx8394f init  \n");
//	push_table(lcm_initialization_setting1, sizeof(lcm_initialization_setting1) / sizeof(struct LCM_setting_table), 1); 

  init_lcm_registers();
		  
}

static void lcm_suspend(void) 
{
  unsigned int data_array[16];
	// when phone sleep , config output low, disable backlight drv chip  
	//push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	
	data_array[0] = 0x00280500;   
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(50);
	
	data_array[0] = 0x00100500;//			
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);
		
	
#ifdef GPIO_LCD_BIAS_ENP_PIN
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
// zhaozhensen@wind-mobi.com 20160322 begin
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
// zhaozhensen@wind-mobi.com 20160322 end
#endif
pinctrl_select_state(lcmbiasctrl, lcmbias_disable); 
	MDELAY(10);

}

static void lcm_resume(void)
{
	lcm_init();

}

static unsigned int lcm_compare_id(void)
{
	unsigned int id=0,id1=0,id2=0;
	unsigned char buffer[3];
	unsigned int data_array[16];  
	pinctrl_select_state(lcmbiasctrl, lcmbias_enable); 
#ifdef GPIO_LCD_BIAS_ENP_PIN
	mt_set_gpio_mode(GPIO_LCD_BIAS_ENP_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_ENP_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
#endif
  MDELAY(50);
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120); 

	data_array[0]=0x00043902;
	data_array[1]=0x9483FFB9;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0]=0x00033902;
	data_array[1]=0x004373ba;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0] = 0x00023700;// return byte number
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10);

	//read_reg_v2(0xDA, buffer, 1);
	//id1= buffer[0]; //should be 0x83
	read_reg_v2(0xDC, buffer, 1); 
	id2= buffer[0]; //should be 0x0D

	id= id2; //830D

#if defined(BUILD_LK)||defined(BUILD_UBOOT)
	printf(" hzs %s id=%x id1=%x id2=%x \n",__func__,id, id1, id2);
#else
	printk("dzl------------- %s id=%x  \n",__func__,id);
#endif	

	if(LCM_ID_HX8394==id)
		return 1;
	else
		return 0;
}
//static int err_count = 0;


static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK
    unsigned char buffer[8] = {0};

    unsigned int array[4];

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

    array[0] = 0x00013700;    

    dsi_set_cmdq(array, 1,1);

    read_reg_v2(0x0A, buffer,8);

	printk( "ili9881 lcm_esd_check: buffer[0] = %d,buffer[1] = %d,buffer[2] = %d,buffer[3] = %d,buffer[4] = %d,buffer[5] = %d,buffer[6] = %d,buffer[7] = %d\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5],buffer[6],buffer[7]);

    if((buffer[0] != 0x9C))/*LCD work status error,need re-initalize*/

    {

        printk( "ili9881 lcm_esd_check buffer[0] = %d\n",buffer[0]);

        return TRUE;

    }

    else

    {

#if 0
        if(buffer[3] != 0x02) //error data type is 0x02

        {
		//  is not 02, 
             err_count = 0;

        }

        else

        {
		// is 02, so ,
             if((buffer[4] == 0x40) || (buffer[5] == 0x80))
             {
			 // buffer[4] is not 0, || (huo),buffer[5] is 0x80.
			   err_count = 0;
             }
             else

             {
			// is  0,0x80,  
			   err_count++;
             }             

             if(err_count >=2 )
             {
			
                 err_count = 0;

                 printk( "ili9881 lcm_esd_check buffer[4] = %d , buffer[5] = %d\n",buffer[4],buffer[5]);

                 return TRUE;

             }

        }
#endif
        return FALSE;

    }
#endif
	
}


static unsigned int lcm_esd_recover(void)
{
    #ifndef BUILD_LK
    printk( "ili9881 lcm_esd_recover\n");
    #endif
	lcm_init();
//	lcm_resume();

	return TRUE;
}

//dingyisheng@wind-mobi.com 20160704 begin 	
extern atomic_t ESDCheck_byCPU;
static unsigned int lcm_ata_check(unsigned char *buf)		
{
	#ifndef BUILD_LK
	unsigned int id=0,id1=0,id2=0;	
	unsigned char buffer[3];
//dingyisheng@wind-mobi.com 20160810 begin
/*
	unsigned int data_array[16];  		

	data_array[0]=0x00043902;
	data_array[1]=0x9483FFB9;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);

	data_array[0]=0x00023902;
	data_array[1]=0x000013ba;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(10);
	
	data_array[0] = 0x00023700;
	dsi_set_cmdq(data_array, 1, 1);	
	MDELAY(10);

	//atomic_set(&ESDCheck_byCPU,1);	
	//read_reg_v2(0xDA, buffer, 1); 	
	//atomic_set(&ESDCheck_byCPU,0);	
	//id1 = buffer[0];
*/
//dingyisheng@wind-mobi.com 20160810 begin
	atomic_set(&ESDCheck_byCPU,1);	
	read_reg_v2(0xDC, buffer, 1);  //0xDB	
	atomic_set(&ESDCheck_byCPU,0);	
	id2 = buffer[0]; 

	id = id2;	

	if(LCM_ID_HX8394==id)	
		return 1;	
	else	
		return 0;	
#else 
		return 0;
#endif
}	
//dingyisheng@wind-mobi.com 20160704 end 

#ifdef WIND_LCD_POWER_SUPPLY_SUPPORT
extern void lcm_init_power(void);
extern void lcm_resume_power(void);
extern void lcm_suspend_power(void);
#endif

LCM_DRIVER hx8394d_hd720_dsi_vdo_hlt_lcm_drv =
{
	.name           	= "hx8394d_hd720_dsi_vdo_hlt",
	.set_util_funcs 	= lcm_set_util_funcs,
	.get_params     	= lcm_get_params,
	.init           	= lcm_init,
	.suspend        	= lcm_suspend,
	.resume         	= lcm_resume,
	.compare_id     	= lcm_compare_id,
	.esd_check = lcm_esd_check,
	//dingyisheng@wind-mobi.com 20160704 begin 
	.ata_check    =   lcm_ata_check,
	//dingyisheng@wind-mobi.com 20160704 end
	.esd_recover = lcm_esd_recover,
#ifdef WIND_LCD_POWER_SUPPLY_SUPPORT
	.init_power		= lcm_init_power,
	.resume_power   = lcm_resume_power,
	.suspend_power  = lcm_suspend_power,
#endif
};
//late_initcall(lcm_init);

//modified ranyanhao@wind-mobi.com 20160215 end
