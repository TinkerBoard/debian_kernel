/*
 * Analog Devices ADV7511 HDMI transmitter driver
 *
 * Copyright 2012 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef __DRM_I2C_ADV7511_H__
#define __DRM_I2C_ADV7511_H__

#include <linux/hdmi.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

struct sn65dsi84_data {
	struct backlight_device *backlight;
	struct i2c_client *client;
	struct device *dev;

	unsigned int lvds_clk_rate;
	unsigned int dsi_lanes;
	unsigned int lvds_format;;
	unsigned int lvds_bpp;
	unsigned int width_mm;
	unsigned int height_mm;
	unsigned int sync_delay;
	unsigned int refclk_multiplier;
	unsigned int lvds_voltage;
	bool test_pattern_en;
	bool dual_link;
	bool clk_from_refclk;
	bool enabled;
	bool debug;

	bool powered;

	struct gpio_desc *lvds_vdd_en_gpio;
	struct gpio_desc *sn65dsi84_en_gpio;
	struct gpio_desc *lvds_hdmi_sel_gpio;
	struct gpio_desc *pwr_source_gpio;

	struct workqueue_struct *wq;
	struct work_struct work;
	//unsigned int	dsi84_irq_gpio;
	struct gpio_desc *dsi84_irq_gpio;
	unsigned int	dsi84_irq;

	enum drm_connector_status status;
	struct drm_connector connector;
	struct device_node *host_node;
	struct drm_bridge bridge;

	struct mipi_dsi_device *dsi;
	struct videomode  vm;

	struct drm_display_mode curr_mode;
	unsigned int bus_format;
	unsigned int bpc;
	unsigned int format;
	unsigned int mode_flags;
	unsigned int t1;
	unsigned int t2;
	unsigned int t3;
	unsigned int t4;
	unsigned int t5;
	unsigned int t6;
	unsigned int t7;
};
#endif /* __DRM_I2C_ADV7511_H__ */
