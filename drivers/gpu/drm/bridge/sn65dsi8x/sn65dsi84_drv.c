/*
 * Analog Devices sn65dsi84 HDMI transmitter driver
 *
 * Copyright 2012 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_edid.h>

#include "sn65dsi84.h"

#include <linux/i2c.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME "sn65dsi84"

#define SN_SOFT_RESET			0x09
#define SN_CLK_SRC				0x0a
#define SN_CLK_DIV				0x0b
#define SN_PLL_EN				0x0d
#define SN_DSI_LANES			0x10
#define SN_DSI_EQ				0x11
#define SN_DSI_CLK				0x12
#define SN_FORMAT				0x18
#define SN_LVDS_VOLTAGE		0x19
#define SN_LVDS_TERM			0x1a
#define SN_LVDS_CM_VOLTAGE	0x1b
#define SN_HACTIVE_LOW			0x20
#define SN_HACTIVE_HIGH		0x21
#define SN_VACTIVE_LOW			0x24
#define SN_VACTIVE_HIGH		0x25
#define SN_SYNC_DELAY_LOW		0x28
#define SN_SYNC_DELAY_HIGH	0x29
#define SN_HSYNC_LOW			0x2c
#define SN_HSYNC_HIGH			0x2d
#define SN_VSYNC_LOW			0x30
#define SN_VSYNC_HIGH			0x31
#define SN_HBP					0x34
#define SN_VBP					0x36
#define SN_HFP					0x38
#define SN_VFP					0x3a
#define SN_TEST_PATTERN		0x3c
#define SN_IRQ_EN				0xe0
#define SN_IRQ_MASK				0xe1
#define SN_IRQ_STAT				0xe5

#define CHA_DSI_LANES (3)

/*SN_CLK_DIV*/
#define DSI_CLK_DIVIDER 			(3)

/*SN_CLK_SRC*/
#define LVDS_CLK_RANGE_OFFSET		(1)
#define HS_CLK_SRC_OFFSET 			(0)

/*SN_FORMAT	*/
#define DE_NEG_POLARITY_OFFSET 	(7)
#define HS_NEG_POLARITY_OFFSET 	(6)
#define VS_NEG_POLARITY_OFFSET 	(5)
#define LVDS_LINK_CFG_OFFSET 		(4)
#define CHA_24BPP_MODE_OFFSET 	(3)
#define CHB_24BPP_MODE_OFFSET 	(2)
#define CHA_24BPP_FMT1_OFFSET 	(1)
#define CHB_24BPP_FMT1_OFFSET 	(0)

#define MULTIPLY_BY_1	(1)
#define MULTIPLY_BY_2	(2)
#define MULTIPLY_BY_3	(3)
#define MULTIPLY_BY_4	(4)

static bool switch_to_lvds = true;

bool hdmi_lvds_switch(void)
{
	return switch_to_lvds ;
}
EXPORT_SYMBOL_GPL(hdmi_lvds_switch);

bool sn65dsi84_is_connected(void)
{
	return switch_to_lvds ;
}
EXPORT_SYMBOL_GPL(sn65dsi84_is_connected);

static void ConvertBoard_power_on(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->pwr_source_gpio) {
		gpiod_set_value_cansleep(sn65dsi84->pwr_source_gpio, 1);
		msleep(20);
	}
}

static void ConvertBoard_power_off(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->pwr_source_gpio) {
		gpiod_set_value_cansleep(sn65dsi84->pwr_source_gpio, 0);
	}
}

static void lvds_power_on(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->lvds_vdd_en_gpio) {
		gpiod_set_value_cansleep(sn65dsi84->lvds_vdd_en_gpio, 1);
		//msleep(20);//T2: 0.01ms ~50ms
	}
}

static void lvds_power_off(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->lvds_vdd_en_gpio) {
		//msleep(10);//T5: 0.01ms ~50ms
		gpiod_set_value_cansleep(sn65dsi84->lvds_vdd_en_gpio, 0);
		//msleep(1000);//T7: 1000ms
	}
}

#if 1
int sn65dsi84_read(struct i2c_client *client, int reg, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at reg=0x%02x\n", reg);
		*val = 0;
		return ret;
	}

	*val = ret;
	return 0;
}

int sn65dsi84_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = i2c_smbus_write_byte_data(client, reg, val);

	if (ret)
		dev_err(&client->dev, "failed to write at reg=0x%02x, val=0x%02x\n", reg, val);

	return ret;
}
#else
int sn65dsi84_read(struct i2c_client *client, int reg, uint8_t *val)
{
	*val = 0;
	printk("sn65dsi84_read reg=0x%x\n", reg);
	return 0;
}

int sn65dsi84_write(struct i2c_client *client, u8 reg, u8 val)
{
	printk("sn65dsi84_write reg=0x%x val=0x%x\n", reg, val);
	return 0;
}
#endif
int sn65dsi84_update_bit(struct i2c_client *client, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return sn65dsi84_write(client, reg, tmp);
}

static void sn65dsi84_chip_enable(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->sn65dsi84_en_gpio) {
		gpiod_set_value_cansleep(sn65dsi84->sn65dsi84_en_gpio, 1);
		msleep(10);
	}

	sn65dsi84->powered = true;
}

static void sn65dsi84_chip_shutdown(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s \n", __func__);
	if (sn65dsi84->sn65dsi84_en_gpio) {
		gpiod_set_value_cansleep(sn65dsi84->sn65dsi84_en_gpio, 0);
		msleep(10);
	}

	sn65dsi84->powered = false;
}

//static void sn65dsi84_csr_registers_readback(struct sn65dsi84_data *sn65dsi84)
//{
//}

static void sn65dsi84_enable_irq(struct sn65dsi84_data *sn65dsi84, bool enable)
{
	sn65dsi84_write(sn65dsi84->client, SN_IRQ_MASK, enable ? 0xFF : 0);
	sn65dsi84_write(sn65dsi84->client, SN_IRQ_EN, enable ? 0x1 : 0);
}

static unsigned int sn65dsi84_clk_src(struct sn65dsi84_data *sn65dsi84)
{
	#define LVDS_CDLK (sn65dsi84->lvds_clk_rate)
	unsigned int val = 0;

	if ((25000000 <= LVDS_CDLK) && (LVDS_CDLK < 37500000))
		val = 0;
	else if ((37500000 <= LVDS_CDLK) && (LVDS_CDLK < 62500000))
		val = 1;
	else if ((62500000 <= LVDS_CDLK) && (LVDS_CDLK < 87500000))
		val = 2;
	else if ((87500000 <= LVDS_CDLK) && (LVDS_CDLK < 112500000))
		val = 3;
	else if ((112500000 <= LVDS_CDLK) && (LVDS_CDLK < 137500000))
		val = 4;
	else if ((137500000 <= LVDS_CDLK) && (LVDS_CDLK < 154000000))
		val = 5;

	val = (val << LVDS_CLK_RANGE_OFFSET);
	if (!sn65dsi84->clk_from_refclk)
		val |= (1 << HS_CLK_SRC_OFFSET);
	printk(KERN_INFO "%s val=%x\n", __func__, val);

	return val;
}

static unsigned int sn65dsi84_clk_div(struct sn65dsi84_data *sn65dsi84)
{
	//unsigned long long rate = sn65dsi84->lvds_clk_rate;
	//unsigned long long dsi_ch_clk;
	unsigned long rate = sn65dsi84->lvds_clk_rate;
	unsigned long dsi_ch_clk;
	unsigned int val;

	if (sn65dsi84->dual_link)
		dsi_ch_clk = (rate * sn65dsi84->lvds_bpp * 2) / (2 * sn65dsi84->dsi_lanes);
	else
		dsi_ch_clk = (rate * sn65dsi84->lvds_bpp) / (2 * sn65dsi84->dsi_lanes);

	val = dsi_ch_clk/rate;
	val -= 1;
	val = (val << DSI_CLK_DIVIDER);

	printk(KERN_INFO "%s val=%x\n", __func__, val);

	return val;
}

static unsigned int sn65dsi84_refclk_multiplier(struct sn65dsi84_data *sn65dsi84)
{
	unsigned int val = 0;

	if (sn65dsi84->refclk_multiplier == MULTIPLY_BY_1)
		val = 0;
	else if (sn65dsi84->refclk_multiplier == MULTIPLY_BY_2)
		val = BIT(0);
	else if (sn65dsi84->refclk_multiplier == MULTIPLY_BY_3)
		val = BIT(1);
	else if (sn65dsi84->refclk_multiplier == MULTIPLY_BY_4)
		val = BIT(0) | BIT(1);
	else
		val = BIT(1);

	printk(KERN_INFO "%s val=%x\n", __func__, val);

	return val;
}


static unsigned int sn65dsi84_dsi_clk(struct sn65dsi84_data *sn65dsi84)
{
	//unsigned long long rate = sn65dsi84->lvds_clk_rate;
	//unsigned long long dsi_ch_clk;
	unsigned long rate = sn65dsi84->lvds_clk_rate;
	unsigned long dsi_ch_clk;
	unsigned int val;

	if (sn65dsi84->dual_link)
		dsi_ch_clk = (rate * sn65dsi84->lvds_bpp * 2) / (2 * sn65dsi84->dsi_lanes);
	else
		dsi_ch_clk = (rate * sn65dsi84->lvds_bpp) / (2 * sn65dsi84->dsi_lanes);

	if (dsi_ch_clk >= 40000000)
		val = dsi_ch_clk/5000000;
	else
		val = 0;

	printk(KERN_INFO "%s val=%x\n", __func__, val);

	return val;

}

static unsigned int sn65dsi84_format(struct sn65dsi84_data *sn65dsi84)
{
	unsigned int val = 0;

	if (sn65dsi84->vm.flags & DISPLAY_FLAGS_DE_LOW)
		val |= (1 << DE_NEG_POLARITY_OFFSET);

	if (sn65dsi84->vm.flags & DISPLAY_FLAGS_HSYNC_LOW)
		val |= (1 << HS_NEG_POLARITY_OFFSET);

	if (sn65dsi84->vm.flags & DISPLAY_FLAGS_VSYNC_LOW)
		val |= (1 << VS_NEG_POLARITY_OFFSET);

	if (!sn65dsi84->dual_link)
		val |= (1 << LVDS_LINK_CFG_OFFSET);

	if (sn65dsi84->lvds_bpp == 24)
		val |= (1 << CHA_24BPP_MODE_OFFSET);

	if (sn65dsi84->lvds_format == 1)
		val |= (1 << CHA_24BPP_FMT1_OFFSET);

	if (sn65dsi84->dual_link) {
		if (sn65dsi84->lvds_bpp == 24)
			val |= (1 << CHB_24BPP_MODE_OFFSET);

		if (sn65dsi84->lvds_format == 1)
			val |= (1 << CHB_24BPP_FMT1_OFFSET);
	}

	printk(KERN_INFO "%s val=%x\n", __func__, val);

	return val;
}

static void sn65dsi84_init_csr_registers(struct sn65dsi84_data *sn65dsi84)
{
	bool test_pattern = sn65dsi84->test_pattern_en;
	int dual_link_div = sn65dsi84->dual_link ? 2 : 1;
	uint8_t val;
	#define HIGH_BYTE(X) ((X & 0xFF00 ) >> 8)
	#define LOW_BYTE(X)  (X & 0x00FF)

	printk(KERN_INFO "%s +\n", __func__);
#if 0
	sn65dsi84_write(sn65dsi84->client, SN_SOFT_RESET, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_CLK_SRC, sn65dsi84_clk_src(sn65dsi84));//0x05
	sn65dsi84_write(sn65dsi84->client, SN_CLK_DIV, sn65dsi84_clk_div(sn65dsi84));//0x28
	sn65dsi84_write(sn65dsi84->client, SN_PLL_EN, 0x00);
#else
	sn65dsi84_write(sn65dsi84->client, SN_SOFT_RESET, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_PLL_EN, 0x00);
	msleep(10);
	sn65dsi84_write(sn65dsi84->client, SN_CLK_SRC, sn65dsi84_clk_src(sn65dsi84));//0x05
	if (sn65dsi84->clk_from_refclk)
		sn65dsi84_write(sn65dsi84->client, SN_CLK_DIV, sn65dsi84_refclk_multiplier(sn65dsi84));
	else
		sn65dsi84_write(sn65dsi84->client, SN_CLK_DIV, sn65dsi84_clk_div(sn65dsi84));//0x28
#endif
	/* Configure DSI_LANES  */
	sn65dsi84_read(sn65dsi84->client, SN_DSI_LANES, &val);
	val &= ~(3 << CHA_DSI_LANES);
	val |= ((4 - sn65dsi84->dsi_lanes) << CHA_DSI_LANES);
	sn65dsi84_write(sn65dsi84->client, SN_DSI_LANES, val);

	sn65dsi84_write(sn65dsi84->client, SN_DSI_EQ, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_DSI_CLK, sn65dsi84_dsi_clk(sn65dsi84));//0x56
	sn65dsi84_write(sn65dsi84->client, 0x13, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_FORMAT, sn65dsi84_format(sn65dsi84));//0x06
	sn65dsi84_write(sn65dsi84->client, SN_LVDS_VOLTAGE, sn65dsi84->lvds_voltage);//Dis tuner tool suggest 0x0.
	sn65dsi84_write(sn65dsi84->client, SN_LVDS_TERM, 0x03);
	sn65dsi84_write(sn65dsi84->client, SN_LVDS_CM_VOLTAGE, 0x00);

	if (!test_pattern) {
		sn65dsi84_write(sn65dsi84->client, SN_HACTIVE_LOW, LOW_BYTE(sn65dsi84->vm.hactive));
		sn65dsi84_write(sn65dsi84->client, SN_HACTIVE_HIGH, HIGH_BYTE(sn65dsi84->vm.hactive));
	} else {
		sn65dsi84_write(sn65dsi84->client, SN_HACTIVE_LOW, LOW_BYTE(sn65dsi84->vm.hactive/dual_link_div));//HACTIVE 0xc0
		sn65dsi84_write(sn65dsi84->client, SN_HACTIVE_HIGH, HIGH_BYTE(sn65dsi84->vm.hactive/dual_link_div));//0x03
	}

	sn65dsi84_write(sn65dsi84->client, 0x22, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x23, 0x00);

	if (!test_pattern) {
		sn65dsi84_write(sn65dsi84->client, SN_VACTIVE_LOW, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_VACTIVE_HIGH, 0x00);
	} else {
		sn65dsi84_write(sn65dsi84->client, SN_VACTIVE_LOW, LOW_BYTE(sn65dsi84->vm.vactive));//VACTIVE 0x38
		sn65dsi84_write(sn65dsi84->client, SN_VACTIVE_HIGH, HIGH_BYTE(sn65dsi84->vm.vactive));//0x04
	}

	sn65dsi84_write(sn65dsi84->client, 0x26, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x27, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_SYNC_DELAY_LOW, LOW_BYTE(sn65dsi84->sync_delay));//0x21
	sn65dsi84_write(sn65dsi84->client, SN_SYNC_DELAY_HIGH, HIGH_BYTE(sn65dsi84->sync_delay));
	sn65dsi84_write(sn65dsi84->client, 0x2A, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x2B, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_HSYNC_LOW, LOW_BYTE(sn65dsi84->vm.hsync_len/dual_link_div));//HSYNC//0x2c
	sn65dsi84_write(sn65dsi84->client, SN_HSYNC_HIGH, HIGH_BYTE(sn65dsi84->vm.hsync_len/dual_link_div));//0x0
	sn65dsi84_write(sn65dsi84->client, 0x2E, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x2F, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_VSYNC_LOW, LOW_BYTE(sn65dsi84->vm.vsync_len));//VSYNC //0x08
	sn65dsi84_write(sn65dsi84->client, SN_VSYNC_HIGH, HIGH_BYTE(sn65dsi84->vm.vsync_len));//0x00
	sn65dsi84_write(sn65dsi84->client, 0x32, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x33, 0x00);
	sn65dsi84_write(sn65dsi84->client, SN_HBP, sn65dsi84->vm.hback_porch/dual_link_div);//HBP //0x2c
	sn65dsi84_write(sn65dsi84->client, 0x35, 0x00);

	if (!test_pattern) {
		sn65dsi84_write(sn65dsi84->client, SN_VBP, 0x00);
		sn65dsi84_write(sn65dsi84->client, 0x37, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_HFP, 0x00);
		sn65dsi84_write(sn65dsi84->client, 0x39, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_VFP,  0x00);
		sn65dsi84_write(sn65dsi84->client, 0x3B, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_TEST_PATTERN, 0x00);
	} else {
		sn65dsi84_write(sn65dsi84->client, SN_VBP, sn65dsi84->vm.vback_porch);//VBP //0x08
		sn65dsi84_write(sn65dsi84->client, 0x37, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_HFP, sn65dsi84->vm.hfront_porch/dual_link_div);//HFP //0x28
		sn65dsi84_write(sn65dsi84->client, 0x39, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_VFP,  sn65dsi84->vm.vfront_porch);//VFP 0x04
		sn65dsi84_write(sn65dsi84->client, 0x3B, 0x00);
		sn65dsi84_write(sn65dsi84->client, SN_TEST_PATTERN, 0x10);
	}

	sn65dsi84_write(sn65dsi84->client, 0x3D, 0x00);
	sn65dsi84_write(sn65dsi84->client, 0x3E, 0x00);

	printk(KERN_INFO "%s -\n", __func__);
}

static void sn65dsi84_init_sequence(struct sn65dsi84_data *sn65dsi84)
{
	uint8_t val = 0xFF;
	int retry = 0;

	printk(KERN_INFO "%s +\n", __func__);

	do {
		sn65dsi84_chip_shutdown(sn65dsi84);
		sn65dsi84_chip_enable(sn65dsi84);
		sn65dsi84_init_csr_registers(sn65dsi84);
		sn65dsi84_write(sn65dsi84->client, SN_PLL_EN, 0x1);
		msleep(10);
		sn65dsi84_write(sn65dsi84->client, SN_SOFT_RESET, 1);
		msleep(10);

		sn65dsi84_read(sn65dsi84->client, SN_IRQ_STAT, &val);
		if (val !=0x00)
			printk(KERN_ERR "sn65dsi84_init_sequence SN_IRQ_STAT = %x\n", val);

		sn65dsi84_write(sn65dsi84->client, SN_IRQ_STAT, 0xFF);
		msleep(2);
		sn65dsi84_read(sn65dsi84->client, SN_IRQ_STAT, &val);
		if (val !=0x00)
			printk(KERN_ERR "sn65dsi84_init_sequence init fail, val = %x\n", val);
	} while((val !=0x00) && (++retry < 3));

	sn65dsi84_enable_irq(sn65dsi84, true);

	printk(KERN_INFO "%s - \n", __func__);
}

static int sn65dsi84_get_modes(struct sn65dsi84_data *sn65dsi84,
			     struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	u32 bus_format = sn65dsi84->bus_format;//MEDIA_BUS_FMT_RGB888_1X24;
	//u32 *bus_flags = &connector->display_info.bus_flags;//nxp
	int ret;

	printk(KERN_INFO "%s +\n", __func__);
	mode = drm_mode_create(connector->dev);
	if (!mode) {
		printk(KERN_INFO "%s :Failed to create display mode!\n", __func__);
		return 0;
	}

	drm_display_mode_from_videomode(&sn65dsi84->vm, mode);
	mode->width_mm = sn65dsi84->width_mm;
	mode->height_mm = sn65dsi84->height_mm;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_probed_add(connector, mode);
	//drm_mode_connector_list_update(connector);//nxp
   	drm_mode_connector_list_update(connector, true);
	connector->display_info.width_mm = sn65dsi84->width_mm;
	connector->display_info.height_mm = sn65dsi84->height_mm;
	connector->display_info.bpc = sn65dsi84->bpc;

	#if 0//nxp
	*bus_flags |= DRM_BUS_FLAG_DE_HIGH | DRM_BUS_FLAG_PIXDATA_NEGEDGE;
	#else//nxp
	//if (sn65dsi84->vm.flags & DISPLAY_FLAGS_DE_HIGH)
	//	*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
	//if (sn65dsi84->vm.flags & DISPLAY_FLAGS_DE_LOW)
	//	*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	//if (sn65dsi84->vm.flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
	//	*bus_flags |= DRM_BUS_FLAG_PIXDATA_NEGEDGE;
	//if (sn65dsi84->vm.flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
	//	*bus_flags |= DRM_BUS_FLAG_PIXDATA_POSEDGE;
	#endif

	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	if (ret) {
		printk(KERN_ERR "%s return ret=%d\n",  __func__, ret);
		return ret;
	}

	//drm_mode_probed_add(connector, mode);
 	printk(KERN_INFO "%s -\n", __func__);
	return 1;
}

static enum drm_connector_status
sn65dsi84_detect(struct sn65dsi84_data *sn65dsi84)
{
	#define ID_REGISTERS_SZIE (9)
	enum drm_connector_status status = sn65dsi84->status;
	uint8_t id[ID_REGISTERS_SZIE] = {0x35, 0x38, 0x49, 0x53, 0x44, 0x20, 0x20, 0x20, 0x01};
	uint8_t return_id[ID_REGISTERS_SZIE] = {0};
	int i;

	printk(KERN_INFO "%s \n", __func__);

	if (status == connector_status_connected)
		return status;

	for (i = 0; i < sizeof(id) /sizeof(uint8_t); i++) {
		sn65dsi84_read(sn65dsi84->client, i, &return_id[i]);
	}

	if (!memcmp(id, return_id, sizeof(id) /sizeof(uint8_t))) {
		printk(KERN_INFO "sn65dsi84_detect successful\n");
		status = connector_status_connected;
	} else {
		printk(KERN_ERR "sn65dsi84_detect fail, 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			return_id[0], return_id[1], return_id[2], return_id[3], return_id[4], return_id[5], return_id[6], return_id[7], return_id[8]);
	}

	sn65dsi84->status = status;

	printk(KERN_INFO "%s sn65dsi84->status=%d\n", __func__, sn65dsi84->status);

	return status;
}

static int sn65dsi84_mode_valid(struct sn65dsi84_data *sn65dsi84,
			      struct drm_display_mode *mode)
{
	  printk(KERN_INFO "%s: hdisplay =%d, vdisplay = %d, clock=%d\n", __func__,
			mode->hdisplay,mode->vdisplay,mode->clock);

	return MODE_OK;
}

static void sn65dsi84_mode_set(struct sn65dsi84_data *sn65dsi84,
			     struct drm_display_mode *mode,
			     struct drm_display_mode *adj_mode)
{
	printk(KERN_INFO "%s \n", __func__);
	printk(KERN_INFO "%s %d %d %d %d %d %d %d %d %d\n", __func__, adj_mode->clock, adj_mode->hdisplay, adj_mode->hsync_start, adj_mode->hsync_end,
		adj_mode->htotal, adj_mode->vdisplay, adj_mode->vsync_start, adj_mode->vsync_end, adj_mode->vtotal);
	printk(KERN_INFO "%s %d %d %d %d %d %d %d %d %d\n", __func__, mode->clock, mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
		mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal);

	drm_mode_copy(&sn65dsi84->curr_mode, adj_mode);
}

/* Connector funcs */
static struct sn65dsi84_data *connector_to_sn65dsi84(struct drm_connector *connector)
{
	return container_of(connector, struct sn65dsi84_data, connector);
}

static int sn65dsi84_connector_get_modes(struct drm_connector *connector)
{
	struct sn65dsi84_data *sn65dsi84 = connector_to_sn65dsi84(connector);

	return sn65dsi84_get_modes(sn65dsi84, connector);
}

static enum drm_mode_status
sn65dsi84_connector_mode_valid(struct drm_connector *connector,
			     struct drm_display_mode *mode)
{
	struct sn65dsi84_data *sn65dsi84 = connector_to_sn65dsi84(connector);

	return sn65dsi84_mode_valid(sn65dsi84, mode);
}

static struct drm_connector_helper_funcs sn65dsi84_connector_helper_funcs = {
	.get_modes = sn65dsi84_connector_get_modes,
	.mode_valid = sn65dsi84_connector_mode_valid,
};

static enum drm_connector_status
sn65dsi84_connector_detect(struct drm_connector *connector, bool force)
{
	struct sn65dsi84_data *sn65dsi84 = connector_to_sn65dsi84(connector);

	return sn65dsi84_detect(sn65dsi84);
}

static const struct drm_connector_funcs sn65dsi84_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = sn65dsi84_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* Bridge funcs */
static struct sn65dsi84_data *bridge_to_sn65dsi84(struct drm_bridge *bridge)
{
	return container_of(bridge, struct sn65dsi84_data, bridge);
}

static void _sn65dsi84_enable(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s +\n", __func__);

	sn65dsi84_init_sequence(sn65dsi84);
	enable_irq(sn65dsi84->dsi84_irq);

	return;
}

static void _sn65dsi84_disable(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s\n", __func__);

	disable_irq_nosync(sn65dsi84->dsi84_irq);
	sn65dsi84_enable_irq(sn65dsi84, false);
	sn65dsi84_write(sn65dsi84->client, SN_PLL_EN, 0);
	sn65dsi84_chip_shutdown(sn65dsi84);


	return;
}

void sn65dsi84_bridge_enable(struct drm_bridge *bridge)
{
	struct sn65dsi84_data *sn65dsi84 = bridge_to_sn65dsi84(bridge);

	printk(KERN_INFO "%s sn65dsi84->enabled=%d\n", __func__, sn65dsi84->enabled);

	if (sn65dsi84->enabled)
		return;

	#if 0
	lvds_power_off(sn65dsi84);
	msleep(500);
	sn65dsi84_init_sequence(sn65dsi84);
	lvds_power_on(sn65dsi84);
	#else
	lvds_power_on(sn65dsi84);
	msleep(sn65dsi84->t2);
	sn65dsi84_init_sequence(sn65dsi84);
	msleep(sn65dsi84->t3);
	#endif

	if (sn65dsi84->backlight) {
        	sn65dsi84->backlight->props.power = FB_BLANK_UNBLANK;
        	backlight_update_status(sn65dsi84->backlight);
    	}

	sn65dsi84->enabled = true;

	enable_irq(sn65dsi84->dsi84_irq);
	return;
}

static void sn65dsi84_bridge_disable(struct drm_bridge *bridge)
{
	struct sn65dsi84_data *sn65dsi84 = bridge_to_sn65dsi84(bridge);

	printk(KERN_INFO "%s sn65dsi84->enabled=%d\n", __func__, sn65dsi84->enabled);
	if (!sn65dsi84->enabled)
		return;

	disable_irq(sn65dsi84->dsi84_irq);
	sn65dsi84_enable_irq(sn65dsi84, false);

	if (sn65dsi84->backlight) {
        	sn65dsi84->backlight->props.power = FB_BLANK_POWERDOWN;
        	backlight_update_status(sn65dsi84->backlight);
		msleep(sn65dsi84->t4);
    	}

	sn65dsi84_write(sn65dsi84->client, SN_PLL_EN, 0);
	sn65dsi84_chip_shutdown(sn65dsi84);
	msleep(sn65dsi84->t5);
	lvds_power_off(sn65dsi84);
	msleep(sn65dsi84->t7);

	sn65dsi84->enabled = false;

	return;
}

static void sn65dsi84_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj_mode)
{
	struct sn65dsi84_data *sn65dsi84 = bridge_to_sn65dsi84(bridge);

	sn65dsi84_mode_set(sn65dsi84, mode, adj_mode);
}

int sn65dsi84_attach_dsi(struct sn65dsi84_data *sn65dsi84)
{
	struct device *dev = sn65dsi84->dev;
	struct mipi_dsi_host *host;
	struct mipi_dsi_device *dsi;
	int ret = 0;
	const struct mipi_dsi_device_info info = { .type = "sn65dsi84",
						   				.channel = 0,
										//.node = NULL;
										.node = sn65dsi84->bridge.of_node,
						 				};
	printk(KERN_INFO "%s +\n", __func__);

	host = of_find_mipi_dsi_host_by_node(sn65dsi84->host_node);
	if (!host) {
		dev_err(dev, "failed to find dsi host\n");
		return -EPROBE_DEFER;
	}

	dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(dsi)) {
		dev_err(dev, "failed to create dsi device\n");
		ret = PTR_ERR(dsi);
		goto err_dsi_device;
	}

	sn65dsi84->dsi = dsi;

	dsi->lanes = sn65dsi84->dsi_lanes;
	dsi->format = sn65dsi84->format;
	dsi->mode_flags = sn65dsi84->mode_flags;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach dsi to host\n");
		goto err_dsi_attach;
	}

	printk(KERN_INFO "%s -\n", __func__);

	return 0;

err_dsi_attach:
	mipi_dsi_device_unregister(dsi);
err_dsi_device:
	return ret;
}

void sn65dsi84_detach_dsi(struct sn65dsi84_data *sn65dsi84)
{
	printk(KERN_INFO "%s +\n", __func__);
	mipi_dsi_detach(sn65dsi84->dsi);
	mipi_dsi_device_unregister(sn65dsi84->dsi);
}

static int sn65dsi84_bridge_attach(struct drm_bridge *bridge)
{
	struct sn65dsi84_data *sn65dsi84 = bridge_to_sn65dsi84(bridge);
	int ret;

	printk(KERN_INFO "%s +\n", __func__);

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	sn65dsi84->connector.polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(bridge->dev, &sn65dsi84->connector,
				 &sn65dsi84_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		DRM_ERROR("Failed to initialize connector with drm\n");
		return ret;
	}
	drm_connector_helper_add(&sn65dsi84->connector,
				 &sn65dsi84_connector_helper_funcs);
	drm_mode_connector_attach_encoder(&sn65dsi84->connector, bridge->encoder);

	//ret = sn65dsi84_attach_dsi(sn65dsi84);

	printk(KERN_INFO "%s dsi->mode_flags=0x%lx-\n", __func__, sn65dsi84->dsi->mode_flags);
	return ret;
}

static const struct drm_bridge_funcs sn65dsi84_bridge_funcs = {
	.enable = sn65dsi84_bridge_enable,
	.disable = sn65dsi84_bridge_disable,
	.mode_set = sn65dsi84_bridge_mode_set,
	.attach = sn65dsi84_bridge_attach,
};

static const struct display_timing asus_sn65dsi84_default_mode = {
	.pixelclock = { 143700000, 143700000, 143700000 },
	.hactive = { 1920, 1920, 1920},
	.hfront_porch = { 80, 80, 80 },
	.hsync_len = { 88, 88, 88 },
	.hback_porch = {88, 88, 88 },
	.vactive = { 1080, 1080, 1080 },
	.vfront_porch = { 4, 4, 4 },
	.vsync_len = { 8, 8, 8 },
	.vback_porch = { 8, 8, 8 },
	.flags = DISPLAY_FLAGS_HSYNC_LOW |
			DISPLAY_FLAGS_VSYNC_LOW |
			DISPLAY_FLAGS_DE_HIGH |
			DISPLAY_FLAGS_PIXDATA_NEGEDGE,
};

static int sn65dsi84_parse_dt(struct device_node *np,
			   struct sn65dsi84_data *data)
{
	struct device_node *endpoint,  *backlight;
	struct device *dev = data->dev;
	//unsigned long irq_flags;
	int ret;

	printk(KERN_INFO "%s +\n", __func__);
	#if 0
	data->host_node = of_graph_get_remote_node(np, 0, 0);
	if (!data->host_node) {
		dev_err(dev, "sn65dsi84_parse_dt can not get host_node\n");
		return -ENODEV;
		dev_err(dev, "error:sn65dsi84_parse_dt can not get host_node\n");
		return -ENODEV;
	}
	#else
	endpoint = of_graph_get_next_endpoint(np, NULL);
	if (!endpoint) {
		dev_err(dev, "error:sn65dsi84_parse_dt can not get next_endpoint\n");
		return -ENODEV;
	} else {
		printk("sn65dsi84_parse_dt  endpoint->name=%s type=%s full_name=%s\n", endpoint->name, endpoint->type, endpoint->full_name);
	}
	data->host_node = of_graph_get_remote_port_parent(endpoint);
	if (!data->host_node) {
		of_node_put(endpoint);
		return -ENODEV;
	}else {
		printk("sn65dsi84_parse_dt  endpoint->name=%s type=%s full_name=%s\n", data->host_node->name, data->host_node->type, data->host_node->full_name);
	}
	#endif

	ret = of_property_read_u32(np, "lvds-clk-rate", &data->lvds_clk_rate);
	ret = of_property_read_u32(np, "dsi-lanes", &data->dsi_lanes);
	if (data->dsi_lanes < 1 || data->dsi_lanes > 4) {
		dev_err(dev, "Invalid dsi-lanes: %d\n", data->dsi_lanes);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "lvds-format", &data->lvds_format);
	if ((data->lvds_format != 2) && (data->lvds_format != 1)) {
		dev_err(dev, "Invalid lvds-format: %d\n", data->lvds_format);
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "lvds-bpp", &data->lvds_bpp);
	ret = of_property_read_u32(np, "lvds-width-mm", &data->width_mm);
	ret = of_property_read_u32(np, "lvds-height-mm", &data->height_mm);
	ret = of_property_read_u32(np, "sync_delay", &data->sync_delay);
	ret = of_property_read_u32(np, "refclk_multiplier", &data->refclk_multiplier);
	ret = of_property_read_u32(np, "lvds_voltage", &data->lvds_voltage);
	if (ret) {
		data->lvds_voltage = 0x0F;
		printk(KERN_INFO "sn65dsi84_parse_dt use 0xF as default value for lvds_voltage\n");
	}

	data->test_pattern_en = of_property_read_bool(np, "test-pattern");
	data->dual_link = of_property_read_bool(np, "dual-link");
	data->clk_from_refclk = of_property_read_bool(np, "clk_from_refclk");

	ret = of_property_read_u32(dev->of_node, "bus-format", &data->bus_format);
	ret = of_property_read_u32(dev->of_node, "bpc", &data->bpc);
	ret = of_property_read_u32(dev->of_node, "dsi,flags", &data->mode_flags);
	ret = of_property_read_u32(dev->of_node, "dsi,format", &data->format);
	ret = of_property_read_u32(dev->of_node, "dsi,lanes", &data->dsi_lanes);

	ret = of_property_read_u32(dev->of_node,"t1", &data->t1);
	ret = of_property_read_u32(dev->of_node,"t2", &data->t2);
	ret = of_property_read_u32(dev->of_node,"t3", &data->t3);
	ret = of_property_read_u32(dev->of_node,"t4", &data->t4);
	ret = of_property_read_u32(dev->of_node,"t5", &data->t5);
	ret = of_property_read_u32(dev->of_node,"t6", &data->t6);
	ret = of_property_read_u32(dev->of_node,"t7", &data->t7);

	ret = of_get_videomode(np, &data->vm, 0);
	printk(KERN_INFO "sn65dsi84_parse_dt pixelclock=%lu hactive=%u vactive=%u vm.flags=%u \n",
		data->vm.pixelclock, data->vm.hactive, data->vm.vactive, data->vm.flags);
	printk(KERN_INFO "sn65dsi84_parse_dt hfront_porch=%u .hsync_len=%u hback_porch=%u \n",
		data->vm.hfront_porch, data->vm.hsync_len, data->vm.hback_porch);
	printk(KERN_INFO "sn65dsi84_parse_dt vfront_porch=%u vsync_len=%u vback_porch=%u \n",
		data->vm.vfront_porch, data->vm.vsync_len, data->vm.vback_porch);
	printk(KERN_INFO "sn65dsi84_parse_dt refclk_multiplier=%u lvds_voltage=0x%x\n", data->refclk_multiplier, data->lvds_voltage);
	printk(KERN_INFO "sn65dsi84_parse_dt bus_format=%x data->bpc=%u format =%u mode_flags=%u\n",
		data->bus_format, data->bpc, data->format, data->mode_flags);
	printk(KERN_INFO "sn65dsi84_parse_dt t1=%u t2=%u t3=%u t4=%u t5=%u t6=%u t7=%u\n",
		data->t1, data->t2, data->t3, data->t4, data->t5, data->t6, data->t7);

	if (ret < 0) {
		videomode_from_timing(&asus_sn65dsi84_default_mode, &data->vm);
	}

	data->sn65dsi84_en_gpio = devm_gpiod_get_optional(dev, "EN",  GPIOD_OUT_LOW);
	if (IS_ERR(data->sn65dsi84_en_gpio)) {
		printk(KERN_INFO "sn65dsi84_parse_dt: failed to get EN GPIO \n");
	}

	data->lvds_vdd_en_gpio = devm_gpiod_get_optional(dev, "lvds_vdd_en", GPIOD_OUT_LOW);
	if (IS_ERR(data->lvds_vdd_en_gpio)) {
		printk(KERN_INFO "sn65dsi84_parse_dt: failed to get lvds_vdd_en_gpio\n");
	}

	data->lvds_hdmi_sel_gpio = devm_gpiod_get_optional(dev, "lvds_hdmi_sel", GPIOD_IN);
	if (IS_ERR(data->lvds_hdmi_sel_gpio)) {
		printk(KERN_INFO "sn65dsi84_parse_dt: failed to get lvds_hdmi_sel_gpio\n");
	}

	//data->dsi84_irq_gpio = of_get_named_gpio_flags(np, "dsi84_irq", 0, (enum of_gpio_flags *)&irq_flags);
	//printk(KERN_INFO "sn65dsi84_parse_dt  dsi84_irq_gpio=%u\n", data->dsi84_irq_gpio);
	data->dsi84_irq_gpio = devm_gpiod_get_optional(dev, "dsi84_irq", GPIOD_IN);
	if (IS_ERR(data->dsi84_irq_gpio)) {
		printk(KERN_INFO "sn65dsi84_parse_dt: failed to get dsi84_irq_gpio\n");
	}

	data->pwr_source_gpio = devm_gpiod_get_optional(dev, "pwr_source", GPIOD_OUT_LOW);
	if (IS_ERR(data->pwr_source_gpio)) {
		printk(KERN_INFO "sn65dsi84_parse_dt: failed to get  pwr_source gpio\n");
	}

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		data->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!data->backlight) {
			printk(KERN_ERR "sn65dsi84_parse_dt: failed to find backlight\n");
		}
	} else {
		printk(KERN_ERR "sn65dsi84_parse_dt: failed to parse backlight handle\n");
	}

	printk(KERN_INFO "sn65dsi84_parse_dt  lvds_clk_rate=%u dsi_lanes=%u lvds_format=%u lvds_bpp=%u width_mm=%u height_mm=%u\n",
		   data->lvds_clk_rate, data->dsi_lanes,  data->lvds_format, data->lvds_bpp, data->width_mm, data->height_mm);
	printk(KERN_INFO "sn65dsi84_parse_dt sync_delay=%u test_pattern_en=%u dual_link=%u \n",
		data->sync_delay, data->test_pattern_en, data->dual_link);
	printk(KERN_INFO "%s -\n", __func__);

	of_node_put(endpoint);
	of_node_put(data->host_node);
	return ret;
}

static irqreturn_t sn65dsi84_irq(int irq, void *dev)
{
	struct sn65dsi84_data *sn65dsi84 = (struct sn65dsi84_data *)dev;\

	disable_irq_nosync(sn65dsi84->dsi84_irq);// Disable ts interrupt

	if (!work_pending(&sn65dsi84->work))
		queue_work(sn65dsi84->wq, &sn65dsi84->work);

	return IRQ_HANDLED;
}

static void sn65dsi84_irq_worker(struct work_struct *work)
{
	struct sn65dsi84_data *sn65dsi84 = container_of(work, struct sn65dsi84_data, work);
	uint8_t val = 0xFF;

	sn65dsi84_read(sn65dsi84->client, SN_IRQ_STAT, &val);
	if (sn65dsi84->debug)
	printk(KERN_ERR "error: sn65dsi84_irq_worker SN_IRQ_STAT = %x\n", val);

	sn65dsi84_write(sn65dsi84->client, SN_IRQ_STAT, 0xFF);
	msleep(2);
	sn65dsi84_read(sn65dsi84->client, SN_IRQ_STAT, &val);
	if (sn65dsi84->debug)
	printk(KERN_ERR "error: sn65dsi84_irq_worker, the 2nd SN_IRQ_STAT = %x\n", val);

	enable_irq(sn65dsi84->dsi84_irq);
}

static ssize_t sn65dsi84_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sn65dsi84_data *sn65dsi84 = dev_get_drvdata(dev);

	_sn65dsi84_disable(sn65dsi84);
	_sn65dsi84_enable(sn65dsi84);

	return strnlen(buf, count);
}

static ssize_t sn65dsi84_reg_show(struct device *dev, struct device_attribute *attr, char *buf2)
{
	struct sn65dsi84_data *sn65dsi84 = dev_get_drvdata(dev);
	uint8_t buf[1] = {0};

	sn65dsi84_read(sn65dsi84->client, SN_SOFT_RESET, buf);
	printk(KERN_ERR "sn65dsi84_show  0x09= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_PLL_EN, buf);
	printk(KERN_ERR "sn65dsi84_show 0x0d = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_CLK_SRC, buf);
	printk(KERN_ERR "sn65dsi84_show  0x0a= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_CLK_DIV, buf);
	printk(KERN_ERR "sn65dsi84_show  0x0b= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_DSI_LANES, buf);
	printk(KERN_ERR "sn65dsi84_show  0x10= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_DSI_EQ, buf);
	printk(KERN_ERR "sn65dsi84_show  0x11= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_DSI_CLK, buf);
	printk(KERN_ERR "sn65dsi84_show  0x12= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x13, buf);
	printk(KERN_ERR "sn65dsi84_show  0x13= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_FORMAT, buf);
	printk(KERN_ERR "sn65dsi84_show  0x18= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_LVDS_VOLTAGE, buf);
	printk(KERN_ERR "sn65dsi84_show 0x19 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_LVDS_TERM, buf);
	printk(KERN_ERR "sn65dsi84_show 0x1a = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_LVDS_CM_VOLTAGE, buf);
	printk(KERN_ERR "sn65dsi84_show  0x1b= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HACTIVE_LOW, buf);
	printk(KERN_ERR "sn65dsi84_show  0x20= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HACTIVE_HIGH, buf);
	printk(KERN_ERR "sn65dsi84_show  0x21= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x22, buf);
	printk(KERN_ERR "sn65dsi84_show  0x22= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x23, buf);
	printk(KERN_ERR "sn65dsi84_show  0x23= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VACTIVE_LOW, buf);
	printk(KERN_ERR "sn65dsi84_show 0x24 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VACTIVE_HIGH, buf);
	printk(KERN_ERR "sn65dsi84_show 0x25 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x26, buf);
	printk(KERN_ERR "sn65dsi84_show  0x26= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x27, buf);
	printk(KERN_ERR "sn65dsi84_show 0x27 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_SYNC_DELAY_LOW, buf);
	printk(KERN_ERR "sn65dsi84_show  0x28= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_SYNC_DELAY_HIGH, buf);
	printk(KERN_ERR "sn65dsi84_show  0x29= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x2A, buf);
	printk(KERN_ERR "sn65dsi84_show  0x2A= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x2B, buf);
	printk(KERN_ERR "sn65dsi84_show  0x2b= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HSYNC_LOW, buf);
	printk(KERN_ERR "sn65dsi84_show 0x2c = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HSYNC_HIGH, buf);
	printk(KERN_ERR "sn65dsi84_show  0x2d = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x2E, buf);
	printk(KERN_ERR "sn65dsi84_show  0x2e= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x2F, buf);
	printk(KERN_ERR "sn65dsi84_show  0x2F= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VSYNC_LOW, buf);
	printk(KERN_ERR "sn65dsi84_show  0x30= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VSYNC_HIGH, buf);
	printk(KERN_ERR "sn65dsi84_show  0x31 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x32, buf);
	printk(KERN_ERR "sn65dsi84_show 0x32 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x33, buf);
	printk(KERN_ERR "sn65dsi84_show 0x33 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HBP, buf);
	printk(KERN_ERR "sn65dsi84_show  0x34 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x35, buf);
	printk(KERN_ERR "sn65dsi84_show  0x35 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VBP, buf);
	printk(KERN_ERR "sn65dsi84_show  0x36 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x37, buf);
	printk(KERN_ERR "sn65dsi84_show  0x37 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_HFP, buf);
	printk(KERN_ERR "sn65dsi84_show  0x38 = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x39, buf);
	printk(KERN_ERR "sn65dsi84_show  0x39= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_VFP, buf);
	printk(KERN_ERR "sn65dsi84_show  0x3a= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x3B, buf);
	printk(KERN_ERR "sn65dsi84_show 0x3b = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_TEST_PATTERN, buf);
	printk(KERN_ERR "sn65dsi84_show  0x3c = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x3D, buf);
	printk(KERN_ERR "sn65dsi84_show  0x3D = 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, 0x3E, buf);
	printk(KERN_ERR "sn65dsi84_show  0x3E= 0x%x\n", buf[0]);
	sn65dsi84_read(sn65dsi84->client, SN_IRQ_STAT, buf);
	printk(KERN_ERR "sn65dsi84_show  0xe5  SN_IRQ_STAT = %x\n", buf[0]);

	return 0;
}

static ssize_t sn65dsi84_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sn65dsi84_data *sn65dsi84 = dev_get_drvdata(dev);
	unsigned val;
	int ret;
	char *endp;
	unsigned reg = simple_strtol(buf, &endp, 16);

	if (reg > 0xe5)
		return count;

	if (!endp)
		return count;

	printk("%s: %c\n", __func__, *endp);
	val = simple_strtol(endp+1, &endp, 16);
	if (val >= 0x100)
		return count;

	printk("%s:reg=0x%x, val=0x%x\n", __func__, reg, val);
	ret = sn65dsi84_write(sn65dsi84->client, reg, val);
	if (ret < 0)
		return ret;

	return strnlen(buf, count);
}

static ssize_t sn65dsi84_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sn65dsi84_data *sn65dsi84 = dev_get_drvdata(dev);
	unsigned long debug;
	int rc;

	rc = kstrtoul(buf, 0, &debug);
	if (rc)
		return rc;

	sn65dsi84->debug = !!debug;

	return strnlen(buf, count);
}

static ssize_t sn65dsi84_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sn65dsi84_data *sn65dsi84 = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sn65dsi84->debug);
}

static DEVICE_ATTR(sn65dsi84_reinit, S_IRUGO | S_IWUSR, NULL, sn65dsi84_store);
static DEVICE_ATTR(sn65dsi84_reg, S_IRUGO | S_IWUSR, sn65dsi84_reg_show, sn65dsi84_reg_store);
static DEVICE_ATTR(sn65dsi84_debug, S_IRUGO | S_IWUSR, sn65dsi84_debug_show, sn65dsi84_debug_store);

static struct attribute *sn65dsi84_attributes[] = {
	&dev_attr_sn65dsi84_reinit.attr,
	&dev_attr_sn65dsi84_reg.attr,
	&dev_attr_sn65dsi84_debug.attr,
	NULL
};

static const struct attribute_group sn65dsi84_attr_group = {
	.attrs = sn65dsi84_attributes,
};

static int sn65dsi84_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct sn65dsi84_data *sn65dsi84;
	struct device *dev = &i2c->dev;
	int ret;

	printk(KERN_INFO "%s +\n", __func__);
	if (!dev->of_node)
		return -EINVAL;

	sn65dsi84 = devm_kzalloc(dev, sizeof(struct sn65dsi84_data), GFP_KERNEL);
	if (!sn65dsi84)
		return -ENOMEM;
	dev->driver_data = sn65dsi84;
	sn65dsi84->dev = dev;
	sn65dsi84->client = i2c;
	sn65dsi84->enabled = false;
	sn65dsi84->powered = false;
	sn65dsi84->status = connector_status_disconnected;

	ret = sn65dsi84_parse_dt(dev->of_node, sn65dsi84);
	if (ret)
		return ret;

	if (sn65dsi84->lvds_hdmi_sel_gpio) {
		//printk(KERN_INFO "%s lvds_hdmi_sel_gpio=%d\n", __func__, gpiod_get_value(sn65dsi84->lvds_hdmi_sel_gpio));
		//switch_to_lvds = !!gpiod_get_value(sn65dsi84->lvds_hdmi_sel_gpio);
	}

	ConvertBoard_power_on(sn65dsi84);
	lvds_power_off(sn65dsi84);
	//msleep(1000); //t15

	sn65dsi84_chip_shutdown(sn65dsi84);
	sn65dsi84_chip_enable(sn65dsi84);
	sn65dsi84_detect(sn65dsi84);
	sn65dsi84_chip_shutdown(sn65dsi84);
	if (sn65dsi84->status == connector_status_connected) {
		printk(KERN_INFO "%s : sn65dsi84 is connected!\n", __func__);
		switch_to_lvds = true;
	} else {
		printk(KERN_INFO "%s : sn65dsi84 is disconnected!\n", __func__);
		switch_to_lvds = false;
		ret = -ENODEV;
		return ret;
	}

	i2c_set_clientdata(i2c, sn65dsi84);


	sn65dsi84->bridge.funcs = &sn65dsi84_bridge_funcs;
	sn65dsi84->bridge.of_node = dev->of_node;

	drm_bridge_add(&sn65dsi84->bridge);

	#if 0
	 ret = sn65dsi84_attach_dsi(sn65dsi84);
	 if (ret) {
		printk(KERN_INFO "%s : sn65dsi84 attach dsi fail!\n", __func__);
		switch_to_lvds = false;
		ret = -ENODEV;
		return ret;
	}
	#else
	sn65dsi84_attach_dsi(sn65dsi84);
	#endif

	 INIT_WORK(&sn65dsi84->work, sn65dsi84_irq_worker);
	sn65dsi84->wq = create_singlethread_workqueue("sn65dsi84_irq_wq");
	if (!sn65dsi84->wq) {
		printk(KERN_ERR  "error: sn65dsi84_probe could not create workqueue\n");
	}

	/* IRQ config*/
	//sn65dsi84->dsi84_irq = gpio_to_irq(sn65dsi84->dsi84_irq_gpio);
	sn65dsi84->dsi84_irq = gpiod_to_irq(sn65dsi84->dsi84_irq_gpio);
	/* Init irq */
	ret = request_irq(sn65dsi84->dsi84_irq, sn65dsi84_irq, IRQF_TRIGGER_RISING, DRIVER_NAME, sn65dsi84);
	if ( ret ) {
		dev_err(dev, "error: sn65dsi84_probe, unable to request irq for device %s.\n", DRIVER_NAME);
	}
	disable_irq(sn65dsi84->dsi84_irq);

	//ret = device_create_file(&i2c->dev, &dev_attr_sn65dsi84_debug);
	ret = sysfs_create_group(&i2c->dev.kobj, &sn65dsi84_attr_group);
	printk(KERN_INFO "%s -\n", __func__);

	return 0;
}

static int sn65dsi84_remove(struct i2c_client *i2c)
{
	struct sn65dsi84_data *sn65dsi84 = i2c_get_clientdata(i2c);

	sn65dsi84_bridge_disable(&sn65dsi84->bridge);
	sn65dsi84_detach_dsi(sn65dsi84);
	drm_bridge_remove(&sn65dsi84->bridge);

	return 0;
}

static void  sn65dsi84_shutdown(struct i2c_client *i2c)
{
	struct sn65dsi84_data *sn65dsi84 = i2c_get_clientdata(i2c);

	printk("sn65dsi84_shutdown\n");

	sn65dsi84_bridge_disable(&sn65dsi84->bridge);

	ConvertBoard_power_off(sn65dsi84);

	return;
}

static const struct i2c_device_id sn65dsi84_i2c_ids[] = {
	{ "sn65dsi84"},
	{ }
};
MODULE_DEVICE_TABLE(i2c, sn65dsi84_i2c_ids);

static const struct of_device_id sn65dsi84_of_ids[] = {
	{ .compatible = "asus,sn65dsi84",},
	{ }
};
MODULE_DEVICE_TABLE(of, sn65dsi84_of_ids);

static struct mipi_dsi_driver sn65dsi84_dsi_driver = {
	.driver.name = "sn65dsi84",
};

static struct i2c_driver sn65dsi84_driver = {
	.driver = {
		.name = "sn65dsi84",
		.of_match_table = sn65dsi84_of_ids,
	},
	.id_table = sn65dsi84_i2c_ids,
	.probe = sn65dsi84_probe,
	.remove = sn65dsi84_remove,
	.shutdown = sn65dsi84_shutdown,
};

static int __init sn65dsi84_init(void)
{
	if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
		mipi_dsi_driver_register(&sn65dsi84_dsi_driver);

	return i2c_add_driver(&sn65dsi84_driver);
}
module_init(sn65dsi84_init);

static void __exit sn65dsi84_exit(void)
{
	i2c_del_driver(&sn65dsi84_driver);

	if (IS_ENABLED(CONFIG_DRM_MIPI_DSI))
		mipi_dsi_driver_unregister(&sn65dsi84_dsi_driver);
}
module_exit(sn65dsi84_exit);

MODULE_AUTHOR("ASUS");
MODULE_DESCRIPTION("sn65dsi84 mipi to lvds driver");
MODULE_LICENSE("GPL");
