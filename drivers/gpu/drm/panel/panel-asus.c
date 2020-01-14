// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2018, Bootlin
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>

#include <drm/drm_panel.h>
#include <video/mipi_display.h>

#include <video/of_videomode.h>
#include <video/videomode.h>


extern int tinker_mcu_ili9881c_is_connected(void);
extern int tinker_mcu_is_connected(void);

extern int tc358762_dsi_probe(struct mipi_dsi_device *dsi);
extern int tc358762_dsi_remove(struct mipi_dsi_device *dsi);
extern int tc358762_dsi_shutdown(struct mipi_dsi_device *dsi);

extern int ili9881c_dsi_probe(struct mipi_dsi_device *dsi);
extern int ili9881c_dsi_remove(struct mipi_dsi_device *dsi);
	
static int asus_dsi_probe(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	printk("asus_dsi_probe+\n");
	if (tinker_mcu_is_connected())
		ret = tc358762_dsi_probe(dsi);
	else
		ret = ili9881c_dsi_probe(dsi);
	printk("asus_dsi_probe -\n");
	
	return ret;
}

static int asus_dsi_remove(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	printk("asus_dsi_remove+\n");
	if (tinker_mcu_is_connected())
		ret = tc358762_dsi_remove(dsi);
	else
		ret = ili9881c_dsi_remove(dsi);
	printk("asus_dsi_remove-\n");

	return ret;
}

void asus_dsi_shutdown(struct mipi_dsi_device *dsi)
{
	printk("asus_dsi_shutdown+\n");
	if (tinker_mcu_is_connected())
		tc358762_dsi_shutdown(dsi);
	printk("asus_dsi_shutdown-\n");

	return;
}

static const struct of_device_id asus_of_match[] = {
	{ .compatible = "asus-dsi-panel"},
	{ }
};
MODULE_DEVICE_TABLE(of, asus_of_match);

static struct mipi_dsi_driver asus_dsi_driver = {
	.probe		= asus_dsi_probe,
	.remove		= asus_dsi_remove,
	.shutdown 	= asus_dsi_shutdown,
	.driver = {
		.name		= "asus-dsi",
		.of_match_table	= asus_of_match,
	},
};
module_mipi_dsi_driver(asus_dsi_driver);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Ilitek ILI9881C Controller Driver");
MODULE_LICENSE("GPL v2");

