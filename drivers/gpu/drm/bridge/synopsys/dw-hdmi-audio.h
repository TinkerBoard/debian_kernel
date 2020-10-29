/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DW_HDMI_AUDIO_H
#define DW_HDMI_AUDIO_H

struct dw_hdmi;
extern bool hdmi_i2s_audio_disable;

struct dw_hdmi_audio_data {
	phys_addr_t phys;
	void __iomem *base;
	int irq;
	struct dw_hdmi *hdmi;
	u8 *eld;
};

struct dw_hdmi_i2s_audio_data {
	struct dw_hdmi *hdmi;
	struct platform_device *pdev;

	void (*write)(struct dw_hdmi *hdmi, u8 val, int offset);
	u8 (*read)(struct dw_hdmi *hdmi, int offset);
	void (*mod)(struct dw_hdmi *hdmi, u8 data, u8 mask, unsigned int reg);
};

#endif
