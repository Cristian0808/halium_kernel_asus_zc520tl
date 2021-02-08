// zhaozhensen@wind-mobi.com 20160725 begin
/* MicroArray Fingerprint
 * plat-mtk.h
 * date: 2015-08-20
 * version: v2.0
 * Author: czl
 */

#ifndef PLAT_MTK_H
#define PLAT_MTK_H

#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/of_irq.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <mt_gpio.h>

static unsigned int irq = 0;

int plat_request_irq(irq_handler_t handler, unsigned long flags) {	
	struct device_node *node = NULL;
	unsigned int ret;
	u32 ints[2] = {0};
	const char*tname = "mediatek,finger_print-eint";
	node = of_find_compatible_node(NULL, NULL, "mediatek,finger_print-eint");
	of_property_read_u32_array(node,"debounce",ints,ARRAY_SIZE(ints));
	gpio_set_debounce(ints[0], ints[1]);
	irq = irq_of_parse_and_map(node, 0);	
	ret = request_irq(irq, handler, flags, tname, NULL);
	disable_irq_nosync(irq);
	return ret;
}

void plat_enable_irq(u8 flag) {
	static int state = -1;

	//printd("%s: start\n", __func__);

		if (flag) {
			printd("%s: enable_irq.\n", __func__);
			enable_irq(irq);
		} else {
			printd("%s: disable_irq.\n", __func__);
			disable_irq_nosync(irq);
		}

	//printd("%s: end.\n", __func__);
}

#endif
// zhaozhensen@wind-mobi.com 20160725 end