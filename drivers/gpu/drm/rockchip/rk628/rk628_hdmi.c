// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Chen Shunqing <csq@rock-chips.com>
 */

#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/mfd/rk628.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#include <drm/drm_of.h>
#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <sound/hdmi-codec.h>

#define HDMI_BASE			0x70000
#define HDMI_REG_STRIDE			4

#define DDC_SEGMENT_ADDR		0x30

enum PWR_MODE {
	NORMAL,
	LOWER_PWR,
};

#define HDMI_SCL_RATE			(100 * 1000)
#define DDC_BUS_FREQ_L			0x4b
#define DDC_BUS_FREQ_H			0x4c

#define HDMI_SYS_CTRL			0x00
#define RST_ANALOG_MASK			BIT(6)
#define NOT_RST_ANALOG(x)		UPDATE(x, 6, 6)
#define RST_DIGITAL_MASK		BIT(5)
#define NOT_RST_DIGITAL(x)		UPDATE(x, 5, 5)
#define REG_CLK_INV_MASK		BIT(4)
#define REG_CLK_INV(x)			UPDATE(x, 4, 4)
#define VCLK_INV_MASK			BIT(3)
#define VCLK_INV(x)			UPDATE(x, 3, 3)
#define REG_CLK_SOURCE_MASK		BIT(2)
#define REG_CLK_SOURCE(x)		UPDATE(x, 2, 2)
#define POWER_MASK			BIT(1)
#define PWR_OFF(x)			UPDATE(x, 1, 1)
#define INT_POL_MASK			BIT(0)
#define INT_POL(x)			UPDATE(x, 0, 0)

#define HDMI_VIDEO_CONTROL1		0x01
#define VIDEO_INPUT_FORMAT_MASK		GENMASK(3, 1)
#define VIDEO_INPUT_SDR_RGB444		UPDATE(0x0, 3, 1)
#define VIDEO_INPUT_DDR_RGB444		UPDATE(0x5, 3, 1)
#define VIDEO_INPUT_DDR_YCBCR422	UPDATE(0x6, 3, 1)
#define DE_SOURCE_MASK			BIT(0)
#define DE_SOURCE(x)			UPDATE(x, 0, 0)

#define HDMI_VIDEO_CONTROL2		0x02
#define VIDEO_OUTPUT_COLOR_MASK		GENMASK(7, 6)
#define VIDEO_OUTPUT_RRGB444		UPDATE(0x0, 7, 6)
#define VIDEO_OUTPUT_YCBCR444		UPDATE(0x1, 7, 6)
#define VIDEO_OUTPUT_YCBCR422		UPDATE(0x2, 7, 6)
#define VIDEO_INPUT_BITS_MASK		GENMASK(5, 4)
#define VIDEO_INPUT_12BITS		UPDATE(0x0, 5, 4)
#define VIDEO_INPUT_10BITS		UPDATE(0x1, 5, 4)
#define VIDEO_INPUT_REVERT		UPDATE(0x2, 5, 4)
#define VIDEO_INPUT_8BITS		UPDATE(0x3, 5, 4)
#define VIDEO_INPUT_CSP_MASK		BIT(1)
#define VIDEO_INPUT_CSP(x)		UPDATE(x, 0, 0)

#define HDMI_VIDEO_CONTROL		0x03
#define VIDEO_AUTO_CSC_MASK		BIT(7)
#define VIDEO_AUTO_CSC(x)		UPDATE(x, 7, 7)
#define VIDEO_C0_C2_SWAP_MASK		BIT(0)
#define VIDEO_C0_C2_SWAP(x)		UPDATE(x, 0, 0)
enum {
	C0_C2_CHANGE_ENABLE = 0,
	C0_C2_CHANGE_DISABLE = 1,
	AUTO_CSC_DISABLE = 0,
	AUTO_CSC_ENABLE = 1,
};

#define HDMI_VIDEO_CONTROL3		0x04
#define COLOR_DEPTH_NOT_INDICATED_MASK	BIT(4)
#define COLOR_DEPTH_NOT_INDICATED(x)	UPDATE(x, 4, 4)
#define SOF_MASK			BIT(3)
#define SOF_DISABLE(x)			UPDATE(x, 3, 3)
#define CSC_MASK			BIT(0)
#define CSC_ENABLE(x)			UPDATE(x, 0, 0)

#define HDMI_AV_MUTE			0x05
#define AVMUTE_CLEAR_MASK		BIT(7)
#define AVMUTE_CLEAR(x)			UPDATE(x, 7, 7)
#define AVMUTE_ENABLE_MASK		BIT(6)
#define AVMUTE_ENABLE(x)		UPDATE(x, 6, 6)
#define AUDIO_PD_MASK			BIT(2)
#define AUDIO_PD(x)			UPDATE(x, 2, 2)
#define AUDIO_MUTE_MASK			BIT(1)
#define AUDIO_MUTE(x)			UPDATE(x, 1, 1)
#define VIDEO_BLACK_MASK		BIT(0)
#define VIDEO_MUTE(x)			UPDATE(x, 0, 0)

#define HDMI_VIDEO_TIMING_CTL		0x08
#define HSYNC_POLARITY(x)		UPDATE(x, 3, 3)
#define VSYNC_POLARITY(x)		UPDATE(x, 2, 2)
#define INETLACE(x)			UPDATE(x, 1, 1)
#define EXTERANL_VIDEO(x)		UPDATE(x, 0, 0)

#define HDMI_VIDEO_EXT_HTOTAL_L		0x09
#define HDMI_VIDEO_EXT_HTOTAL_H		0x0a
#define HDMI_VIDEO_EXT_HBLANK_L		0x0b
#define HDMI_VIDEO_EXT_HBLANK_H		0x0c
#define HDMI_VIDEO_EXT_HDELAY_L		0x0d
#define HDMI_VIDEO_EXT_HDELAY_H		0x0e
#define HDMI_VIDEO_EXT_HDURATION_L	0x0f
#define HDMI_VIDEO_EXT_HDURATION_H	0x10
#define HDMI_VIDEO_EXT_VTOTAL_L		0x11
#define HDMI_VIDEO_EXT_VTOTAL_H		0x12
#define HDMI_VIDEO_EXT_VBLANK		0x13
#define HDMI_VIDEO_EXT_VDELAY		0x14
#define HDMI_VIDEO_EXT_VDURATION	0x15

#define HDMI_VIDEO_CSC_COEF		0x18

#define HDMI_AUDIO_CTRL1		0x35
enum {
	CTS_SOURCE_INTERNAL = 0,
	CTS_SOURCE_EXTERNAL = 1,
};

#define CTS_SOURCE(x)			UPDATE(x, 7, 7)

enum {
	DOWNSAMPLE_DISABLE = 0,
	DOWNSAMPLE_1_2 = 1,
	DOWNSAMPLE_1_4 = 2,
};

#define DOWN_SAMPLE(x)			UPDATE(x, 6, 5)

enum {
	AUDIO_SOURCE_IIS = 0,
	AUDIO_SOURCE_SPDIF = 1,
};

#define AUDIO_SOURCE(x)			UPDATE(x, 4, 3)
#define MCLK_ENABLE(x)			UPDATE(x, 2, 2)

enum {
	MCLK_128FS = 0,
	MCLK_256FS = 1,
	MCLK_384FS = 2,
	MCLK_512FS = 3,
};

#define MCLK_RATIO(x)			UPDATE(x, 1, 0)

#define AUDIO_SAMPLE_RATE		0x37
enum {
	AUDIO_32K = 0x3,
	AUDIO_441K = 0x0,
	AUDIO_48K = 0x2,
	AUDIO_882K = 0x8,
	AUDIO_96K = 0xa,
	AUDIO_1764K = 0xc,
	AUDIO_192K = 0xe,
};

#define AUDIO_I2S_MODE			0x38
enum {
	I2S_CHANNEL_1_2 = 1,
	I2S_CHANNEL_3_4 = 3,
	I2S_CHANNEL_5_6 = 7,
	I2S_CHANNEL_7_8 = 0xf
};

#define I2S_CHANNEL(x)			UPDATE(x, 5, 2)

enum {
	I2S_STANDARD = 0,
	I2S_LEFT_JUSTIFIED = 1,
	I2S_RIGHT_JUSTIFIED = 2,
};

#define I2S_MODE(x)			UPDATE(x, 1, 0)

#define AUDIO_I2S_MAP			0x39
#define AUDIO_I2S_SWAPS_SPDIF		0x3a
#define N_32K				0x1000
#define N_441K				0x1880
#define N_882K				0x3100
#define N_1764K				0x6200
#define N_48K				0x1800
#define N_96K				0x3000
#define N_192K				0x6000

#define HDMI_AUDIO_CHANNEL_STATUS	0x3e
#define AUDIO_STATUS_NLPCM_MASK		BIT(7)
#define AUDIO_STATUS_NLPCM(x)		UPDATE(x, 7, 7)
#define AUDIO_STATUS_USE_MASK		BIT(6)
#define AUDIO_STATUS_COPYRIGHT_MASK	BIT(5)
#define AUDIO_STATUS_ADDITION_MASK	GENMASK(3, 2)
#define AUDIO_STATUS_CLK_ACCURACY_MASK	GENMASK(1, 1)

#define AUDIO_N_H			0x3f
#define AUDIO_N_M			0x40
#define AUDIO_N_L			0x41

#define HDMI_AUDIO_CTS_H		0x45
#define HDMI_AUDIO_CTS_M		0x46
#define HDMI_AUDIO_CTS_L		0x47

#define HDMI_DDC_CLK_L			0x4b
#define HDMI_DDC_CLK_H			0x4c

#define HDMI_EDID_SEGMENT_POINTER	0x4d
#define HDMI_EDID_WORD_ADDR		0x4e
#define HDMI_EDID_FIFO_OFFSET		0x4f
#define HDMI_EDID_FIFO_ADDR		0x50

#define HDMI_PACKET_SEND_MANUAL		0x9c
#define HDMI_PACKET_SEND_AUTO		0x9d
#define PACKET_GCP_EN_MASK		BIT(7)
#define PACKET_GCP_EN(x)		UPDATE(x, 7, 7)
#define PACKET_MSI_EN_MASK		BIT(6)
#define PACKET_MSI_EN(x)		UPDATE(x, 6, 6)
#define PACKET_SDI_EN_MASK		BIT(5)
#define PACKET_SDI_EN(x)		UPDATE(x, 5, 5)
#define PACKET_VSI_EN_MASK		BIT(4)
#define PACKET_VSI_EN(x)		UPDATE(x, 4, 4)

#define HDMI_CONTROL_PACKET_BUF_INDEX	0x9f
enum {
	INFOFRAME_VSI = 0x05,
	INFOFRAME_AVI = 0x06,
	INFOFRAME_AAI = 0x08,
};

#define HDMI_CONTROL_PACKET_ADDR	0xa0
#define HDMI_MAXIMUM_INFO_FRAME_SIZE	0x11
enum {
	AVI_COLOR_MODE_RGB = 0,
	AVI_COLOR_MODE_YCBCR422 = 1,
	AVI_COLOR_MODE_YCBCR444 = 2,
	AVI_COLORIMETRY_NO_DATA = 0,

	AVI_COLORIMETRY_SMPTE_170M = 1,
	AVI_COLORIMETRY_ITU709 = 2,
	AVI_COLORIMETRY_EXTENDED = 3,

	AVI_CODED_FRAME_ASPECT_NO_DATA = 0,
	AVI_CODED_FRAME_ASPECT_4_3 = 1,
	AVI_CODED_FRAME_ASPECT_16_9 = 2,

	ACTIVE_ASPECT_RATE_SAME_AS_CODED_FRAME = 0x08,
	ACTIVE_ASPECT_RATE_4_3 = 0x09,
	ACTIVE_ASPECT_RATE_16_9 = 0x0A,
	ACTIVE_ASPECT_RATE_14_9 = 0x0B,
};

#define HDMI_HDCP_CTRL			0x52
#define HDMI_DVI_MASK			BIT(1)
#define HDMI_DVI(x)			UPDATE(x, 1, 1)

#define HDMI_INTERRUPT_MASK1		0xc0
#define INT_EDID_READY_MASK		BIT(2)
#define HDMI_INTERRUPT_STATUS1		0xc1
#define	INT_ACTIVE_VSYNC_MASK		BIT(5)
#define INT_EDID_READY			BIT(2)

#define HDMI_INTERRUPT_MASK2		0xc2
#define HDMI_INTERRUPT_STATUS2		0xc3
#define INT_HDCP_ERR			BIT(7)
#define INT_BKSV_FLAG			BIT(6)
#define INT_HDCP_OK			BIT(4)

#define HDMI_STATUS			0xc8
#define HOTPLUG_STATUS			BIT(7)
#define MASK_INT_HOTPLUG_MASK		BIT(5)
#define MASK_INT_HOTPLUG(x)		UPDATE(x, 5, 5)
#define INT_HOTPLUG			BIT(1)

#define HDMI_COLORBAR                   0xc9

#define HDMI_PHY_SYNC			0xce
#define HDMI_PHY_SYS_CTL		0xe0
#define TMDS_CLK_SOURCE_MASK		BIT(5)
#define TMDS_CLK_SOURCE(x)		UPDATE(x, 5, 5)
#define PHASE_CLK_MASK			BIT(4)
#define PHASE_CLK(x)			UPDATE(x, 4, 4)
#define TMDS_PHASE_SEL_MASK		BIT(3)
#define TMDS_PHASE_SEL(x)		UPDATE(x, 3, 3)
#define BANDGAP_PWR_MASK		BIT(2)
#define BANDGAP_PWR(x)			UPDATE(x, 2, 2)
#define PLL_PWR_DOWN_MASK		BIT(1)
#define PLL_PWR_DOWN(x)			UPDATE(x, 1, 1)
#define TMDS_CHG_PWR_DOWN_MASK		BIT(0)
#define TMDS_CHG_PWR_DOWN(x)		UPDATE(x, 0, 0)

#define HDMI_PHY_CHG_PWR		0xe1
#define CLK_CHG_PWR(x)			UPDATE(x, 3, 3)
#define DATA_CHG_PWR(x)			UPDATE(x, 2, 0)

#define HDMI_PHY_DRIVER			0xe2
#define CLK_MAIN_DRIVER(x)		UPDATE(x, 7, 4)
#define DATA_MAIN_DRIVER(x)		UPDATE(x, 3, 0)

#define HDMI_PHY_PRE_EMPHASIS		0xe3
#define PRE_EMPHASIS(x)			UPDATE(x, 6, 4)
#define CLK_PRE_DRIVER(x)		UPDATE(x, 3, 2)
#define DATA_PRE_DRIVER(x)		UPDATE(x, 1, 0)

#define PHY_FEEDBACK_DIV_RATIO_LOW	0xe7
#define FEEDBACK_DIV_LOW(x)		UPDATE(x, 7, 0)
#define PHY_FEEDBACK_DIV_RATIO_HIGH	0xe8
#define FEEDBACK_DIV_HIGH(x)		UPDATE(x, 0, 0)

#define HDMI_PHY_PRE_DIV_RATIO		0xed
#define PRE_DIV_RATIO(x)		UPDATE(x, 4, 0)

#define HDMI_CEC_CTRL			0xd0
#define ADJUST_FOR_HISENSE_MASK		BIT(6)
#define REJECT_RX_BROADCAST_MASK	BIT(5)
#define BUSFREETIME_ENABLE_MASK		BIT(2)
#define REJECT_RX_MASK			BIT(1)
#define START_TX_MASK			BIT(0)

#define HDMI_CEC_DATA			0xd1
#define HDMI_CEC_TX_OFFSET		0xd2
#define HDMI_CEC_RX_OFFSET		0xd3
#define HDMI_CEC_CLK_H			0xd4
#define HDMI_CEC_CLK_L			0xd5
#define HDMI_CEC_TX_LENGTH		0xd6
#define HDMI_CEC_RX_LENGTH		0xd7
#define HDMI_CEC_TX_INT_MASK		0xd8
#define TX_DONE_MASK			BIT(3)
#define TX_NOACK_MASK			BIT(2)
#define TX_BROADCAST_REJ_MASK		BIT(1)
#define TX_BUSNOTFREE_MASK		BIT(0)

#define HDMI_CEC_RX_INT_MASK		0xd9
#define RX_LA_ERR_MASK			BIT(4)
#define RX_GLITCH_MASK			BIT(3)
#define RX_DONE_MASK			BIT(0)

#define HDMI_CEC_TX_INT			0xda
#define HDMI_CEC_RX_INT			0xdb
#define HDMI_CEC_BUSFREETIME_L		0xdc
#define HDMI_CEC_BUSFREETIME_H		0xdd
#define HDMI_CEC_LOGICADDR		0xde

#define HDMI_REG(x)			(HDMI_BASE + (x) * HDMI_REG_STRIDE)
#define HDMI_MAX_REG			HDMI_REG(0xed)

struct audio_info {
	int sample_rate;
	int channels;
	int sample_width;
};

struct hdmi_data_info {
	int vic;
	bool sink_is_hdmi;
	bool sink_has_audio;
	unsigned int enc_in_format;
	unsigned int enc_out_format;
	unsigned int colorimetry;
};

struct rk628_hdmi_i2c {
	struct i2c_adapter adap;
	u8 ddc_addr;
	u8 segment_addr;
	/* i2c lock */
	struct mutex lock;
};

struct rk628_hdmi_phy_config {
	unsigned long mpixelclock;
	u8 pre_emphasis;	/* pre-emphasis value */
	u8 vlev_ctr;		/* voltage level control */
};

struct rk628_hdmi {
	struct device *dev;
	int irq;
	struct regmap *grf;
	struct regmap *regmap;
	struct rk628 *parent;
	struct clk *pclk;
	struct clk *dclk;

	struct drm_bridge bridge;
	struct drm_connector connector;

	struct rk628_hdmi_i2c *i2c;
	struct i2c_adapter *ddc;

	unsigned int tmds_rate;

	struct platform_device *audio_pdev;
	bool audio_enable;

	struct hdmi_data_info	hdmi_data;
	struct drm_display_mode previous_mode;
#ifdef CONFIG_SWITCH
	struct switch_dev switchdev;
#endif
};

enum {
	CSC_ITU601_16_235_TO_RGB_0_255_8BIT,
	CSC_ITU601_0_255_TO_RGB_0_255_8BIT,
	CSC_ITU709_16_235_TO_RGB_0_255_8BIT,
	CSC_RGB_0_255_TO_ITU601_16_235_8BIT,
	CSC_RGB_0_255_TO_ITU709_16_235_8BIT,
	CSC_RGB_0_255_TO_RGB_16_235_8BIT,
};

static const char coeff_csc[][24] = {
	/*
	 * YUV2RGB:601 SD mode(Y[16:235], UV[16:240], RGB[0:255]):
	 *   R = 1.164*Y + 1.596*V - 204
	 *   G = 1.164*Y - 0.391*U - 0.813*V + 154
	 *   B = 1.164*Y + 2.018*U - 258
	 */
	{
		0x04, 0xa7, 0x00, 0x00, 0x06, 0x62, 0x02, 0xcc,
		0x04, 0xa7, 0x11, 0x90, 0x13, 0x40, 0x00, 0x9a,
		0x04, 0xa7, 0x08, 0x12, 0x00, 0x00, 0x03, 0x02
	},
	/*
	 * YUV2RGB:601 SD mode(YUV[0:255],RGB[0:255]):
	 *   R = Y + 1.402*V - 248
	 *   G = Y - 0.344*U - 0.714*V + 135
	 *   B = Y + 1.772*U - 227
	 */
	{
		0x04, 0x00, 0x00, 0x00, 0x05, 0x9b, 0x02, 0xf8,
		0x04, 0x00, 0x11, 0x60, 0x12, 0xdb, 0x00, 0x87,
		0x04, 0x00, 0x07, 0x16, 0x00, 0x00, 0x02, 0xe3
	},
	/*
	 * YUV2RGB:709 HD mode(Y[16:235],UV[16:240],RGB[0:255]):
	 *   R = 1.164*Y + 1.793*V - 248
	 *   G = 1.164*Y - 0.213*U - 0.534*V + 77
	 *   B = 1.164*Y + 2.115*U - 289
	 */
	{
		0x04, 0xa7, 0x00, 0x00, 0x07, 0x2c, 0x02, 0xf8,
		0x04, 0xa7, 0x10, 0xda, 0x12, 0x22, 0x00, 0x4d,
		0x04, 0xa7, 0x08, 0x74, 0x00, 0x00, 0x03, 0x21
	},

	/*
	 * RGB2YUV:601 SD mode:
	 *   Cb = -0.291G - 0.148R + 0.439B + 128
	 *   Y  = 0.504G  + 0.257R + 0.098B + 16
	 *   Cr = -0.368G + 0.439R - 0.071B + 128
	 */
	{
		0x11, 0x5f, 0x01, 0x82, 0x10, 0x23, 0x00, 0x80,
		0x02, 0x1c, 0x00, 0xa1, 0x00, 0x36, 0x00, 0x1e,
		0x11, 0x29, 0x10, 0x59, 0x01, 0x82, 0x00, 0x80
	},
	/*
	 * RGB2YUV:709 HD mode:
	 *   Cb = - 0.338G - 0.101R + 0.439B + 128
	 *   Y  = 0.614G   + 0.183R + 0.062B + 16
	 *   Cr = - 0.399G + 0.439R - 0.040B + 128
	 */
	{
		0x11, 0x98, 0x01, 0xc1, 0x10, 0x28, 0x00, 0x80,
		0x02, 0x74, 0x00, 0xbb, 0x00, 0x3f, 0x00, 0x10,
		0x11, 0x5a, 0x10, 0x67, 0x01, 0xc1, 0x00, 0x80
	},
	/*
	 * RGB[0:255]2RGB[16:235]:
	 *   R' = R x (235-16)/255 + 16;
	 *   G' = G x (235-16)/255 + 16;
	 *   B' = B x (235-16)/255 + 16;
	 */
	{
		0x00, 0x00, 0x03, 0x6F, 0x00, 0x00, 0x00, 0x10,
		0x03, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x03, 0x6F, 0x00, 0x10
	},
};

static inline struct rk628_hdmi *bridge_to_hdmi(struct drm_bridge *b)
{
	return container_of(b, struct rk628_hdmi, bridge);
}

static inline struct rk628_hdmi *connector_to_hdmi(struct drm_connector *c)
{
	return container_of(c, struct rk628_hdmi, connector);
}

static inline u32 hdmi_readb(struct rk628_hdmi *hdmi, u32 offset)
{
	u32 val;

	regmap_read(hdmi->regmap, (HDMI_BASE + ((offset) * HDMI_REG_STRIDE)),
		    &val);

	return val;
}

static inline void hdmi_writeb(struct rk628_hdmi *hdmi, u32 offset, u32 val)
{
	regmap_write(hdmi->regmap, (HDMI_BASE + ((offset) * HDMI_REG_STRIDE)),
		     val);
}

static inline void hdmi_modb(struct rk628_hdmi *hdmi, u32 offset,
			     u32 msk, u32 val)
{
	u8 temp = hdmi_readb(hdmi, offset) & ~msk;

	temp |= val & msk;
	hdmi_writeb(hdmi, offset, temp);
}

static void rk628_hdmi_i2c_init(struct rk628_hdmi *hdmi)
{
	int ddc_bus_freq;

	ddc_bus_freq = (hdmi->tmds_rate >> 2) / HDMI_SCL_RATE;

	hdmi_writeb(hdmi, DDC_BUS_FREQ_L, ddc_bus_freq & 0xFF);
	hdmi_writeb(hdmi, DDC_BUS_FREQ_H, (ddc_bus_freq >> 8) & 0xFF);

	/* Clear the EDID interrupt flag and mute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, INT_EDID_READY);
}

static void rk628_hdmi_sys_power(struct rk628_hdmi *hdmi, bool enable)
{
	if (enable)
		hdmi_modb(hdmi, HDMI_SYS_CTRL, POWER_MASK, PWR_OFF(0));
	else
		hdmi_modb(hdmi, HDMI_SYS_CTRL, POWER_MASK, PWR_OFF(1));
}

static struct rk628_hdmi_phy_config rk628_hdmi_phy_config[] = {
	/* pixelclk pre-emp vlev */
	{ 74250000,  0x3f, 0x88 },
	{ 165000000, 0x3f, 0x88 },
	{ ~0UL,	     0x00, 0x00 }
};

static void rk628_hdmi_set_pwr_mode(struct rk628_hdmi *hdmi, int mode)
{
	const struct rk628_hdmi_phy_config *phy_config =
						rk628_hdmi_phy_config;

	switch (mode) {
	case NORMAL:
		rk628_hdmi_sys_power(hdmi, false);
		for (; phy_config->mpixelclock != ~0UL; phy_config++)
			if (hdmi->tmds_rate <= phy_config->mpixelclock)
				break;
		if (!phy_config->mpixelclock)
			return;
		hdmi_writeb(hdmi, HDMI_PHY_PRE_EMPHASIS,
			    phy_config->pre_emphasis);
		hdmi_writeb(hdmi, HDMI_PHY_DRIVER, phy_config->vlev_ctr);

		hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x15);
		hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x14);
		hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x10);
		hdmi_writeb(hdmi, HDMI_PHY_CHG_PWR, 0x0f);
		hdmi_writeb(hdmi, HDMI_PHY_SYNC, 0x00);
		hdmi_writeb(hdmi, HDMI_PHY_SYNC, 0x01);

		rk628_hdmi_sys_power(hdmi, true);
		break;

	case LOWER_PWR:
		rk628_hdmi_sys_power(hdmi, false);
		hdmi_writeb(hdmi, HDMI_PHY_DRIVER, 0x00);
		hdmi_writeb(hdmi, HDMI_PHY_PRE_EMPHASIS, 0x00);
		hdmi_writeb(hdmi, HDMI_PHY_CHG_PWR, 0x00);
		hdmi_writeb(hdmi, HDMI_PHY_SYS_CTL, 0x15);
		break;

	default:
		dev_err(hdmi->dev, "Unknown power mode %d\n", mode);
	}
}

static void rk628_hdmi_reset(struct rk628_hdmi *hdmi)
{
	u32 val;
	u32 msk;

	hdmi_modb(hdmi, HDMI_SYS_CTRL, RST_DIGITAL_MASK, NOT_RST_DIGITAL(1));
	usleep_range(100, 110);

	hdmi_modb(hdmi, HDMI_SYS_CTRL, RST_ANALOG_MASK, NOT_RST_ANALOG(1));
	usleep_range(100, 110);

	msk = REG_CLK_INV_MASK | REG_CLK_SOURCE_MASK | POWER_MASK |
	      INT_POL_MASK;
	val = REG_CLK_INV(1) | REG_CLK_SOURCE(1) | PWR_OFF(0) | INT_POL(1);
	hdmi_modb(hdmi, HDMI_SYS_CTRL, msk, val);

	rk628_hdmi_set_pwr_mode(hdmi, NORMAL);
}

static int rk628_hdmi_upload_frame(struct rk628_hdmi *hdmi, int setup_rc,
				   union hdmi_infoframe *frame, u32 frame_index,
				   u32 mask, u32 disable, u32 enable)
{
	if (mask)
		hdmi_modb(hdmi, HDMI_PACKET_SEND_AUTO, mask, disable);

	hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_BUF_INDEX, frame_index);

	if (setup_rc >= 0) {
		u8 packed_frame[HDMI_MAXIMUM_INFO_FRAME_SIZE];
		ssize_t rc, i;

		rc = hdmi_infoframe_pack(frame, packed_frame,
					 sizeof(packed_frame));
		if (rc < 0)
			return rc;

		for (i = 0; i < rc; i++)
			hdmi_writeb(hdmi, HDMI_CONTROL_PACKET_ADDR + i,
				    packed_frame[i]);

		if (mask)
			hdmi_modb(hdmi, HDMI_PACKET_SEND_AUTO, mask, enable);
	}

	return setup_rc;
}

static int rk628_hdmi_config_video_vsi(struct rk628_hdmi *hdmi,
				       struct drm_display_mode *mode)
{
	union hdmi_infoframe frame;
	int rc;

	rc = drm_hdmi_vendor_infoframe_from_display_mode(&frame.vendor.hdmi,
							 mode);

	return rk628_hdmi_upload_frame(hdmi, rc, &frame,
				       INFOFRAME_VSI, PACKET_VSI_EN_MASK,
				       PACKET_VSI_EN(0), PACKET_VSI_EN(1));
}

static int rk628_hdmi_config_video_avi(struct rk628_hdmi *hdmi,
				       struct drm_display_mode *mode)
{
	union hdmi_infoframe frame;
	int rc;

	rc = drm_hdmi_avi_infoframe_from_display_mode(&frame.avi, mode, false);

	if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV444)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV444;
	else if (hdmi->hdmi_data.enc_out_format == HDMI_COLORSPACE_YUV422)
		frame.avi.colorspace = HDMI_COLORSPACE_YUV422;
	else
		frame.avi.colorspace = HDMI_COLORSPACE_RGB;

	if (frame.avi.colorspace != HDMI_COLORSPACE_RGB)
		frame.avi.colorimetry = hdmi->hdmi_data.colorimetry;

	frame.avi.scan_mode = HDMI_SCAN_MODE_NONE;

	return rk628_hdmi_upload_frame(hdmi, rc, &frame,
				       INFOFRAME_AVI, 0, 0, 0);
}

static int rk628_hdmi_config_audio_aai(struct rk628_hdmi *hdmi,
				       struct audio_info *audio)
{
	struct hdmi_audio_infoframe *faudio;
	union hdmi_infoframe frame;
	int rc;

	rc = hdmi_audio_infoframe_init(&frame.audio);
	faudio = (struct hdmi_audio_infoframe *)&frame;

	faudio->channels = audio->channels;

	return rk628_hdmi_upload_frame(hdmi, rc, &frame,
				       INFOFRAME_AAI, 0, 0, 0);
}

static int rk628_hdmi_config_video_csc(struct rk628_hdmi *hdmi)
{
	struct hdmi_data_info *data = &hdmi->hdmi_data;
	int c0_c2_change = 0;
	int csc_enable = 0;
	int csc_mode = 0;
	int auto_csc = 0;
	int value;
	int i;

	/* Input video mode is SDR RGB24bit, data enable signal from external */
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTROL1, DE_SOURCE(1) |
		    VIDEO_INPUT_SDR_RGB444);

	/* Input color hardcode to RGB, and output color hardcode to RGB888 */
	value = VIDEO_INPUT_8BITS | VIDEO_OUTPUT_RRGB444 |
		VIDEO_INPUT_CSP(0);
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTROL2, value);

	if (data->enc_in_format == data->enc_out_format) {
		if ((data->enc_in_format == HDMI_COLORSPACE_RGB) ||
		    (data->enc_in_format >= HDMI_COLORSPACE_YUV444)) {
			value = SOF_DISABLE(1) | COLOR_DEPTH_NOT_INDICATED(1);
			hdmi_writeb(hdmi, HDMI_VIDEO_CONTROL3, value);

			hdmi_modb(hdmi, HDMI_VIDEO_CONTROL,
				  VIDEO_AUTO_CSC_MASK | VIDEO_C0_C2_SWAP_MASK,
				  VIDEO_AUTO_CSC(AUTO_CSC_DISABLE) |
				  VIDEO_C0_C2_SWAP(C0_C2_CHANGE_DISABLE));
			return 0;
		}
	}

	if (data->colorimetry == HDMI_COLORIMETRY_ITU_601) {
		if ((data->enc_in_format == HDMI_COLORSPACE_RGB) &&
		    (data->enc_out_format == HDMI_COLORSPACE_YUV444)) {
			csc_mode = CSC_RGB_0_255_TO_ITU601_16_235_8BIT;
			auto_csc = AUTO_CSC_DISABLE;
			c0_c2_change = C0_C2_CHANGE_DISABLE;
			csc_enable = CSC_ENABLE(1);
		} else if ((data->enc_in_format == HDMI_COLORSPACE_YUV444) &&
			   (data->enc_out_format == HDMI_COLORSPACE_RGB)) {
			csc_mode = CSC_ITU601_16_235_TO_RGB_0_255_8BIT;
			auto_csc = AUTO_CSC_ENABLE;
			c0_c2_change = C0_C2_CHANGE_DISABLE;
			csc_enable = CSC_ENABLE(0);
		}
	} else {
		if ((data->enc_in_format == HDMI_COLORSPACE_RGB) &&
		    (data->enc_out_format == HDMI_COLORSPACE_YUV444)) {
			csc_mode = CSC_RGB_0_255_TO_ITU709_16_235_8BIT;
			auto_csc = AUTO_CSC_DISABLE;
			c0_c2_change = C0_C2_CHANGE_DISABLE;
			csc_enable = CSC_ENABLE(1);
		} else if ((data->enc_in_format == HDMI_COLORSPACE_YUV444) &&
			   (data->enc_out_format == HDMI_COLORSPACE_RGB)) {
			csc_mode = CSC_ITU709_16_235_TO_RGB_0_255_8BIT;
			auto_csc = AUTO_CSC_ENABLE;
			c0_c2_change = C0_C2_CHANGE_DISABLE;
			csc_enable = CSC_ENABLE(0);
		}
	}

	for (i = 0; i < 24; i++)
		hdmi_writeb(hdmi, HDMI_VIDEO_CSC_COEF + i,
			    coeff_csc[csc_mode][i]);

	value = SOF_DISABLE(1) | csc_enable | COLOR_DEPTH_NOT_INDICATED(1);
	hdmi_writeb(hdmi, HDMI_VIDEO_CONTROL3, value);
	hdmi_modb(hdmi, HDMI_VIDEO_CONTROL,
		  VIDEO_AUTO_CSC_MASK | VIDEO_C0_C2_SWAP_MASK,
		  VIDEO_AUTO_CSC(auto_csc) | VIDEO_C0_C2_SWAP(c0_c2_change));

	return 0;
}

static int rk628_hdmi_config_video_timing(struct rk628_hdmi *hdmi,
					  struct drm_display_mode *mode)
{
	int value;

	/* Set detail external video timing polarity and interlace mode */
	value = EXTERANL_VIDEO(1);
	value |= mode->flags & DRM_MODE_FLAG_PHSYNC ?
		 HSYNC_POLARITY(1) : HSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_PVSYNC ?
		 VSYNC_POLARITY(1) : VSYNC_POLARITY(0);
	value |= mode->flags & DRM_MODE_FLAG_INTERLACE ?
		 INETLACE(1) : INETLACE(0);
	hdmi_writeb(hdmi, HDMI_VIDEO_TIMING_CTL, value);

	/* Set detail external video timing */
	value = mode->htotal;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HTOTAL_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HTOTAL_H, (value >> 8) & 0xFF);

	value = mode->htotal - mode->hdisplay;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HBLANK_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HBLANK_H, (value >> 8) & 0xFF);

	value = mode->htotal - mode->hsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDELAY_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDELAY_H, (value >> 8) & 0xFF);

	value = mode->hsync_end - mode->hsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDURATION_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_HDURATION_H, (value >> 8) & 0xFF);

	value = mode->vtotal;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VTOTAL_L, value & 0xFF);
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VTOTAL_H, (value >> 8) & 0xFF);

	value = mode->vtotal - mode->vdisplay;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VBLANK, value & 0xFF);

	value = mode->vtotal - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDELAY, value & 0xFF);

	value = mode->vsync_end - mode->vsync_start;
	hdmi_writeb(hdmi, HDMI_VIDEO_EXT_VDURATION, value & 0xFF);

	hdmi_writeb(hdmi, HDMI_PHY_PRE_DIV_RATIO, 0x1e);
	hdmi_writeb(hdmi, PHY_FEEDBACK_DIV_RATIO_LOW, 0x2c);
	hdmi_writeb(hdmi, PHY_FEEDBACK_DIV_RATIO_HIGH, 0x01);

	return 0;
}

static int rk628_hdmi_setup(struct rk628_hdmi *hdmi,
			    struct drm_display_mode *mode)
{
	hdmi->hdmi_data.vic = drm_match_cea_mode(mode);

	hdmi->hdmi_data.enc_in_format = HDMI_COLORSPACE_RGB;
	hdmi->hdmi_data.enc_out_format = HDMI_COLORSPACE_RGB;

	if ((hdmi->hdmi_data.vic == 6) || (hdmi->hdmi_data.vic == 7) ||
	    (hdmi->hdmi_data.vic == 21) || (hdmi->hdmi_data.vic == 22) ||
	    (hdmi->hdmi_data.vic == 2) || (hdmi->hdmi_data.vic == 3) ||
	    (hdmi->hdmi_data.vic == 17) || (hdmi->hdmi_data.vic == 18))
		hdmi->hdmi_data.colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		hdmi->hdmi_data.colorimetry = HDMI_COLORIMETRY_ITU_709;

	/* Mute video and audio output */
	hdmi_modb(hdmi, HDMI_AV_MUTE, AUDIO_MUTE_MASK | VIDEO_BLACK_MASK,
		  AUDIO_MUTE(1) | VIDEO_MUTE(1));

	/* Set HDMI Mode */
	hdmi_writeb(hdmi, HDMI_HDCP_CTRL,
		    HDMI_DVI(hdmi->hdmi_data.sink_is_hdmi));

	rk628_hdmi_config_video_timing(hdmi, mode);

	rk628_hdmi_config_video_csc(hdmi);

	if (hdmi->hdmi_data.sink_is_hdmi) {
		rk628_hdmi_config_video_avi(hdmi, mode);
		rk628_hdmi_config_video_vsi(hdmi, mode);
	}

	/*
	 * When IP controller have configured to an accurate video
	 * timing, then the TMDS clock source would be switched to
	 * DCLK_LCDC, so we need to init the TMDS rate to mode pixel
	 * clock rate, and reconfigure the DDC clock.
	 */
	hdmi->tmds_rate = mode->clock * 1000;
	rk628_hdmi_i2c_init(hdmi);

	/* Unmute video and audio output */
	hdmi_modb(hdmi, HDMI_AV_MUTE, VIDEO_BLACK_MASK, VIDEO_MUTE(0));
	if (hdmi->audio_enable)
		hdmi_modb(hdmi, HDMI_AV_MUTE, AUDIO_MUTE_MASK, AUDIO_MUTE(0));

	return 0;
}

static enum drm_connector_status
rk628_hdmi_connector_detect(struct drm_connector *connector, bool force)
{
	struct rk628_hdmi *hdmi = connector_to_hdmi(connector);
	int status;

	status = hdmi_readb(hdmi, HDMI_STATUS) & HOTPLUG_STATUS;
#ifdef CONFIG_SWITCH
	if (status)
		switch_set_state(&hdmi->switchdev, 1);
	else
		switch_set_state(&hdmi->switchdev, 0);
#endif
	return status ? connector_status_connected :
			connector_status_disconnected;
}

static int rk628_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct rk628_hdmi *hdmi = connector_to_hdmi(connector);
	struct drm_display_mode *mode;
	struct drm_display_info *info = &connector->display_info;
	const u8 def_modes[6] = {4, 16, 31, 19, 17, 2};
	struct edid *edid = NULL;
	int ret = 0;
	u8 i;

	if (!hdmi->ddc)
		return 0;

	clk_prepare_enable(hdmi->dclk);
	if ((hdmi_readb(hdmi, HDMI_STATUS) & HOTPLUG_STATUS))
		edid = drm_get_edid(connector, hdmi->ddc);
	clk_disable_unprepare(hdmi->dclk);

	if (edid) {
		hdmi->hdmi_data.sink_is_hdmi = drm_detect_hdmi_monitor(edid);
		hdmi->hdmi_data.sink_has_audio = drm_detect_monitor_audio(edid);
		drm_mode_connector_update_edid_property(connector, edid);
		ret = drm_add_edid_modes(connector, edid);
		kfree(edid);
	} else {
		hdmi->hdmi_data.sink_is_hdmi = true;
		hdmi->hdmi_data.sink_has_audio = true;
		for (i = 0; i < sizeof(def_modes); i++) {
			mode = drm_display_mode_from_vic_index(connector,
							       def_modes,
							       31, i);
			if (mode) {
				if (!i)
					mode->type = DRM_MODE_TYPE_PREFERRED;
				drm_mode_probed_add(connector, mode);
				ret++;
			}
		}
		info->edid_hdmi_dc_modes = 0;
		info->hdmi.y420_dc_modes = 0;
		info->color_formats = 0;

		dev_info(hdmi->dev, "failed to get edid\n");
	}

	return ret;
}

static enum drm_mode_status
rk628_hdmi_connector_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	if ((mode->hdisplay == 1920 && mode->vdisplay == 1080) ||
	    (mode->hdisplay == 1280 && mode->vdisplay == 720))
		return MODE_OK;
	else
		return MODE_BAD;
}

static struct drm_encoder *
rk628_hdmi_connector_best_encoder(struct drm_connector *connector)
{
	struct rk628_hdmi *hdmi = connector_to_hdmi(connector);

	return hdmi->bridge.encoder;
}

static int
rk628_hdmi_probe_single_connector_modes(struct drm_connector *connector,
					u32 maxX, u32 maxY)
{
	return drm_helper_probe_single_connector_modes(connector, 1920, 1080);
}

static const struct drm_connector_funcs rk628_hdmi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = rk628_hdmi_probe_single_connector_modes,
	.detect = rk628_hdmi_connector_detect,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs
rk628_hdmi_connector_helper_funcs = {
	.get_modes = rk628_hdmi_connector_get_modes,
	.mode_valid = rk628_hdmi_connector_mode_valid,
	.best_encoder = rk628_hdmi_connector_best_encoder,
};

static void rk628_hdmi_bridge_mode_set(struct drm_bridge *bridge,
				       struct drm_display_mode *mode,
				       struct drm_display_mode *adj_mode)
{
	struct rk628_hdmi *hdmi = bridge_to_hdmi(bridge);

	/* Store the display mode for plugin/DPMS poweron events */
	memcpy(&hdmi->previous_mode, mode, sizeof(hdmi->previous_mode));
}

static void rk628_hdmi_bridge_enable(struct drm_bridge *bridge)
{
	struct rk628_hdmi *hdmi = bridge_to_hdmi(bridge);

	rk628_hdmi_setup(hdmi, &hdmi->previous_mode);
	rk628_hdmi_set_pwr_mode(hdmi, NORMAL);
}

static void rk628_hdmi_bridge_disable(struct drm_bridge *bridge)
{
	struct rk628_hdmi *hdmi = bridge_to_hdmi(bridge);

	rk628_hdmi_set_pwr_mode(hdmi, LOWER_PWR);
}

static int rk628_hdmi_bridge_attach(struct drm_bridge *bridge)
{
	struct rk628_hdmi *hdmi = bridge_to_hdmi(bridge);
	struct device *dev = hdmi->dev;
	struct drm_connector *connector = &hdmi->connector;
	struct drm_device *drm = bridge->dev;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->port = dev->of_node;

	ret = drm_connector_init(drm, connector, &rk628_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(hdmi->dev, "Failed to initialize connector with drm\n");
		return ret;
	}

	drm_connector_helper_add(connector,
				 &rk628_hdmi_connector_helper_funcs);
	drm_mode_connector_attach_encoder(connector, bridge->encoder);

	return 0;
}

static const struct drm_bridge_funcs rk628_hdmi_bridge_funcs = {
	.attach = rk628_hdmi_bridge_attach,
	.mode_set = rk628_hdmi_bridge_mode_set,
	.enable = rk628_hdmi_bridge_enable,
	.disable = rk628_hdmi_bridge_disable,
};

static int
rk628_hdmi_audio_config_set(struct rk628_hdmi *hdmi, struct audio_info *audio)
{
	int rate, N, channel;

	if (audio->channels < 3)
		channel = I2S_CHANNEL_1_2;
	else if (audio->channels < 5)
		channel = I2S_CHANNEL_3_4;
	else if (audio->channels < 7)
		channel = I2S_CHANNEL_5_6;
	else
		channel = I2S_CHANNEL_7_8;

	switch (audio->sample_rate) {
	case 32000:
		rate = AUDIO_32K;
		N = N_32K;
		break;
	case 44100:
		rate = AUDIO_441K;
		N = N_441K;
		break;
	case 48000:
		rate = AUDIO_48K;
		N = N_48K;
		break;
	case 88200:
		rate = AUDIO_882K;
		N = N_882K;
		break;
	case 96000:
		rate = AUDIO_96K;
		N = N_96K;
		break;
	case 176400:
		rate = AUDIO_1764K;
		N = N_1764K;
		break;
	case 192000:
		rate = AUDIO_192K;
		N = N_192K;
		break;
	default:
		dev_err(hdmi->dev, "[%s] not support such sample rate %d\n",
			__func__, audio->sample_rate);
		return -ENOENT;
	}

	/* set_audio source I2S */
	hdmi_writeb(hdmi, HDMI_AUDIO_CTRL1, 0x01);
	hdmi_writeb(hdmi, AUDIO_SAMPLE_RATE, rate);
	hdmi_writeb(hdmi, AUDIO_I2S_MODE,
		    I2S_MODE(I2S_STANDARD) | I2S_CHANNEL(channel));

	hdmi_writeb(hdmi, AUDIO_I2S_MAP, 0x00);
	hdmi_writeb(hdmi, AUDIO_I2S_SWAPS_SPDIF, 0);

	/* Set N value */
	hdmi_writeb(hdmi, AUDIO_N_H, (N >> 16) & 0x0F);
	hdmi_writeb(hdmi, AUDIO_N_M, (N >> 8) & 0xFF);
	hdmi_writeb(hdmi, AUDIO_N_L, N & 0xFF);

	/*Set hdmi nlpcm mode to support hdmi bitstream*/
	hdmi_writeb(hdmi, HDMI_AUDIO_CHANNEL_STATUS, AUDIO_STATUS_NLPCM(0));

	return rk628_hdmi_config_audio_aai(hdmi, audio);
}

static int rk628_hdmi_audio_hw_params(struct device *dev, void *d,
				      struct hdmi_codec_daifmt *daifmt,
				      struct hdmi_codec_params *params)
{
	struct rk628_hdmi *hdmi = dev_get_drvdata(dev);
	struct audio_info audio = {
		.sample_width = params->sample_width,
		.sample_rate = params->sample_rate,
		.channels = params->channels,
	};

	if (!hdmi->hdmi_data.sink_has_audio) {
		dev_err(hdmi->dev, "Sink do not support audio!\n");
		return -ENODEV;
	}

	if (!hdmi->bridge.encoder->crtc)
		return -ENODEV;

	switch (daifmt->fmt) {
	case HDMI_I2S:
		break;
	default:
		dev_err(dev, "%s: Invalid format %d\n", __func__, daifmt->fmt);
		return -EINVAL;
	}

	return rk628_hdmi_audio_config_set(hdmi, &audio);
}

static void rk628_hdmi_audio_shutdown(struct device *dev, void *d)
{
	/* do nothing */
}

static int rk628_hdmi_audio_digital_mute(struct device *dev, void *d, bool mute)
{
	struct rk628_hdmi *hdmi = dev_get_drvdata(dev);

	if (!hdmi->hdmi_data.sink_has_audio) {
		dev_err(hdmi->dev, "Sink do not support audio!\n");
		return -ENODEV;
	}

	hdmi->audio_enable = !mute;

	if (mute)
		hdmi_modb(hdmi, HDMI_AV_MUTE, AUDIO_MUTE_MASK | AUDIO_PD_MASK,
			  AUDIO_MUTE(1) | AUDIO_PD(1));
	else
		hdmi_modb(hdmi, HDMI_AV_MUTE, AUDIO_MUTE_MASK | AUDIO_PD_MASK,
			  AUDIO_MUTE(0) | AUDIO_PD(0));

	return 0;
}

static int rk628_hdmi_audio_get_eld(struct device *dev, void *d,
				    u8 *buf, size_t len)
{
	struct rk628_hdmi *hdmi = dev_get_drvdata(dev);
	struct drm_mode_config *config = &hdmi->bridge.dev->mode_config;
	struct drm_connector *connector;
	int ret = -ENODEV;

	mutex_lock(&config->mutex);
	list_for_each_entry(connector, &config->connector_list, head) {
		if (hdmi->bridge.encoder == connector->encoder) {
			memcpy(buf, connector->eld,
			       min(sizeof(connector->eld), len));
			ret = 0;
		}
	}
	mutex_unlock(&config->mutex);

	return ret;
}

static const struct hdmi_codec_ops audio_codec_ops = {
	.hw_params = rk628_hdmi_audio_hw_params,
	.audio_shutdown = rk628_hdmi_audio_shutdown,
	.digital_mute = rk628_hdmi_audio_digital_mute,
	.get_eld = rk628_hdmi_audio_get_eld,
};

static int rk628_hdmi_audio_codec_init(struct rk628_hdmi *hdmi,
				       struct device *dev)
{
	struct hdmi_codec_pdata codec_data = {
		.i2s = 1,
		.ops = &audio_codec_ops,
		.max_i2s_channels = 8,
	};

	hdmi->audio_enable = false;
	hdmi->audio_pdev = platform_device_register_data(dev,
				HDMI_CODEC_DRV_NAME, PLATFORM_DEVID_NONE,
				&codec_data, sizeof(codec_data));

	return PTR_ERR_OR_ZERO(hdmi->audio_pdev);
}

static irqreturn_t rk628_hdmi_irq(int irq, void *dev_id)
{
	struct rk628_hdmi *hdmi = dev_id;
	u8 interrupt;

	interrupt = hdmi_readb(hdmi, HDMI_STATUS);
	if (!(interrupt & INT_HOTPLUG))
		return IRQ_HANDLED;

	hdmi_modb(hdmi, HDMI_STATUS, INT_HOTPLUG, INT_HOTPLUG);
	if (hdmi->connector.dev)
		drm_helper_hpd_irq_event(hdmi->connector.dev);

	return IRQ_HANDLED;
}

static int rk628_hdmi_i2c_read(struct rk628_hdmi *hdmi, struct i2c_msg *msgs)
{
	int length = msgs->len;
	u8 *buf = msgs->buf;
	int i;
	u32 c;

	for (i = 0; i < 5; i++) {
		msleep(20);
		c = hdmi_readb(hdmi, HDMI_INTERRUPT_STATUS1);
		if (c & INT_EDID_READY)
			break;
	}
	if ((c & INT_EDID_READY) == 0)
		return -EAGAIN;

	while (length--)
		*buf++ = hdmi_readb(hdmi, HDMI_EDID_FIFO_ADDR);

	return 0;
}

static int rk628_hdmi_i2c_write(struct rk628_hdmi *hdmi, struct i2c_msg *msgs)
{
	/*
	 * The DDC module only support read EDID message, so
	 * we assume that each word write to this i2c adapter
	 * should be the offset of EDID word address.
	 */
	if ((msgs->len != 1) ||
	    ((msgs->addr != DDC_ADDR) && (msgs->addr != DDC_SEGMENT_ADDR)))
		return -EINVAL;

	if (msgs->addr == DDC_ADDR)
		hdmi->i2c->ddc_addr = msgs->buf[0];
	if (msgs->addr == DDC_SEGMENT_ADDR) {
		hdmi->i2c->segment_addr = msgs->buf[0];
		return 0;
	}

	/* Set edid fifo first addr */
	hdmi_writeb(hdmi, HDMI_EDID_FIFO_OFFSET, 0x00);

	/* Set edid word address 0x00/0x80 */
	hdmi_writeb(hdmi, HDMI_EDID_WORD_ADDR, hdmi->i2c->ddc_addr);

	/* Set edid segment pointer */
	hdmi_writeb(hdmi, HDMI_EDID_SEGMENT_POINTER, hdmi->i2c->segment_addr);

	return 0;
}

static int rk628_hdmi_i2c_xfer(struct i2c_adapter *adap,
			       struct i2c_msg *msgs, int num)
{
	struct rk628_hdmi *hdmi = i2c_get_adapdata(adap);
	struct rk628_hdmi_i2c *i2c = hdmi->i2c;
	int i, ret = 0;

	mutex_lock(&i2c->lock);

	hdmi->i2c->ddc_addr = 0;
	hdmi->i2c->segment_addr = 0;

	/* Clear the EDID interrupt flag and unmute the interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, INT_EDID_READY);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, INT_EDID_READY_MASK);

	for (i = 0; i < num; i++) {
		dev_dbg(hdmi->dev, "xfer: num: %d/%d, len: %d, flags: %#x\n",
			i + 1, num, msgs[i].len, msgs[i].flags);

		if (msgs[i].flags & I2C_M_RD)
			ret = rk628_hdmi_i2c_read(hdmi, &msgs[i]);
		else
			ret = rk628_hdmi_i2c_write(hdmi, &msgs[i]);

		if (ret < 0)
			break;
	}

	if (!ret)
		ret = num;

	/* Mute HDMI EDID interrupt */
	hdmi_writeb(hdmi, HDMI_INTERRUPT_MASK1, 0);
	hdmi_writeb(hdmi, HDMI_INTERRUPT_STATUS1, INT_EDID_READY);

	mutex_unlock(&i2c->lock);

	return ret;
}

static u32 rk628_hdmi_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm rk628_hdmi_algorithm = {
	.master_xfer	= rk628_hdmi_i2c_xfer,
	.functionality	= rk628_hdmi_i2c_func,
};

static struct i2c_adapter *rk628_hdmi_i2c_adapter(struct rk628_hdmi *hdmi)
{
	struct i2c_adapter *adap;
	struct rk628_hdmi_i2c *i2c;
	int ret;

	i2c = devm_kzalloc(hdmi->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return ERR_PTR(-ENOMEM);

	mutex_init(&i2c->lock);

	adap = &i2c->adap;
	adap->class = I2C_CLASS_DDC;
	adap->owner = THIS_MODULE;
	adap->dev.parent = hdmi->dev;
	adap->dev.of_node = hdmi->dev->of_node;
	adap->algo = &rk628_hdmi_algorithm;
	strlcpy(adap->name, "RK628 HDMI", sizeof(adap->name));
	i2c_set_adapdata(adap, hdmi);

	ret = i2c_add_adapter(adap);
	if (ret) {
		dev_warn(hdmi->dev, "cannot add %s I2C adapter\n", adap->name);
		devm_kfree(hdmi->dev, i2c);
		return ERR_PTR(ret);
	}

	hdmi->i2c = i2c;

	dev_info(hdmi->dev, "registered %s I2C bus driver\n", adap->name);

	return adap;
}

static const struct regmap_range rk628_hdmi_volatile_reg_ranges[] = {
	regmap_reg_range(HDMI_BASE, HDMI_MAX_REG),
};

static const struct regmap_access_table rk628_hdmi_volatile_regs = {
	.yes_ranges = rk628_hdmi_volatile_reg_ranges,
	.n_yes_ranges = ARRAY_SIZE(rk628_hdmi_volatile_reg_ranges),
};

static bool rk628_is_read_enable_reg(struct device *dev, unsigned int reg)
{
	if (reg >= HDMI_BASE && reg <= HDMI_MAX_REG)
		return true;

	return false;
}

static bool rk628_hdmi_register_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case HDMI_REG(HDMI_EDID_FIFO_ADDR):
	case HDMI_REG(HDMI_INTERRUPT_STATUS1):
	case HDMI_REG(HDMI_INTERRUPT_STATUS2):
	case HDMI_REG(HDMI_STATUS):
	case HDMI_REG(HDMI_CEC_TX_INT):
	case HDMI_REG(HDMI_CEC_RX_INT):
		return true;
	default:
		return false;
	}
}

static const struct regmap_config rk628_hdmi_regmap_config = {
	.name = "hdmi",
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = HDMI_REG_STRIDE,
	.max_register = HDMI_MAX_REG,
	.cache_type = REGCACHE_RBTREE,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
	.readable_reg = rk628_is_read_enable_reg,
	.writeable_reg = rk628_is_read_enable_reg,
	.volatile_reg = rk628_hdmi_register_volatile,
	.rd_table = &rk628_hdmi_volatile_regs,
};

static int rk628_hdmi_probe(struct platform_device *pdev)
{
	struct rk628 *rk628 = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct rk628_hdmi *hdmi;
	int irq;
	int ret;

	if (!of_device_is_available(dev->of_node))
		return -ENODEV;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	hdmi->grf = rk628->grf;
	if (!hdmi->grf)
		return -ENODEV;

	hdmi->dev = dev;
	hdmi->parent = rk628;
	platform_set_drvdata(pdev, hdmi);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	hdmi->regmap = devm_regmap_init_i2c(rk628->client,
					    &rk628_hdmi_regmap_config);
	if (IS_ERR(hdmi->regmap)) {
		ret = PTR_ERR(hdmi->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		return PTR_ERR(hdmi->regmap);
	}

	hdmi->dclk = devm_clk_get(dev, "dclk");
	if (IS_ERR(hdmi->dclk)) {
		dev_err(dev, "Unable to get dclk %ld\n",
			PTR_ERR(hdmi->dclk));
		return PTR_ERR(hdmi->dclk);
	}

	hdmi->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(hdmi->pclk)) {
		dev_err(dev, "Unable to get pclk %ld\n",
			PTR_ERR(hdmi->pclk));
		return PTR_ERR(hdmi->pclk);
	}
	clk_prepare_enable(hdmi->pclk);

	/* hdmitx vclk pllref select Pin_vclk */
	regmap_update_bits(hdmi->grf, GRF_POST_PROC_CON,
			   SW_HDMITX_VCLK_PLLREF_SEL_MASK,
			   SW_HDMITX_VCLK_PLLREF_SEL(1));
	/* set output mode to HDMI */
	regmap_update_bits(hdmi->grf, GRF_SYSTEM_CON0, SW_OUTPUT_MODE_MASK,
			   SW_OUTPUT_MODE(OUTPUT_MODE_HDMI));

	rk628_hdmi_reset(hdmi);

	hdmi->ddc = rk628_hdmi_i2c_adapter(hdmi);
	if (IS_ERR(hdmi->ddc)) {
		ret = PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		goto fail;
	}

	/*
	 * When IP controller haven't configured to an accurate video
	 * timing, then the TMDS clock source would be switched to
	 * PCLK_HDMI, so we need to init the TMDS rate to PCLK rate,
	 * and reconfigure the DDC clock.
	 */
	hdmi->tmds_rate = clk_get_rate(hdmi->pclk);

	rk628_hdmi_i2c_init(hdmi);

	rk628_hdmi_audio_codec_init(hdmi, dev);

	ret = devm_request_threaded_irq(dev, irq, NULL,
					rk628_hdmi_irq,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					dev_name(dev), hdmi);
	if (ret) {
		dev_err(dev, "failed to request hdmi irq: %d\n", ret);
		goto fail;
	}

	/* Unmute hotplug interrupt */
	hdmi_modb(hdmi, HDMI_STATUS, MASK_INT_HOTPLUG_MASK,
		  MASK_INT_HOTPLUG(1));
	hdmi->bridge.funcs = &rk628_hdmi_bridge_funcs;
	hdmi->bridge.of_node = dev->of_node;
	ret = drm_bridge_add(&hdmi->bridge);
	if (ret) {
		dev_err(dev, "failed to add drm_bridge: %d\n", ret);
		goto fail;
	}

#ifdef CONFIG_SWITCH
	hdmi->switchdev.name = "hdmi";
	switch_dev_register(&hdmi->switchdev);
#endif

	return 0;

fail:
	clk_disable_unprepare(hdmi->pclk);
	return ret;
}

static int rk628_hdmi_remove(struct platform_device *pdev)
{
	struct rk628_hdmi *hdmi = platform_get_drvdata(pdev);

	drm_bridge_remove(&hdmi->bridge);
	i2c_put_adapter(hdmi->ddc);
	clk_disable_unprepare(hdmi->pclk);
#ifdef CONFIG_SWITCH
	switch_dev_unregister(&hdmi->switchdev);
#endif

	return 0;
}

static const struct of_device_id rk628_hdmi_dt_ids[] = {
	{ .compatible = "rockchip,rk628-hdmi", },
	{},
};
MODULE_DEVICE_TABLE(of, rk628_hdmi_dt_ids);

static struct platform_driver rk628_hdmi_driver = {
	.probe  = rk628_hdmi_probe,
	.remove = rk628_hdmi_remove,
	.driver = {
		.name = "rk628-hdmi",
		.of_match_table = rk628_hdmi_dt_ids,
	},
};

module_platform_driver(rk628_hdmi_driver);

MODULE_AUTHOR("Chen Shunqing <csq@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RK628 HDMI driver");
MODULE_LICENSE("GPL v2");
