#ifndef DW_HDMI_CEC_H
#define DW_HDMI_CEC_H

struct dw_hdmi;

struct dw_hdmi_cec_ops {
	void (*enable)(struct dw_hdmi *);
	void (*disable)(struct dw_hdmi *);
};

struct dw_hdmi_cec_data {
	int irq;
	const struct dw_hdmi_cec_ops *ops;
	struct dw_hdmi *hdmi;
	void (*write)(struct dw_hdmi *hdmi, u8 val, int offset);
	u8 (*read)(struct dw_hdmi *hdmi, int offset);
	void (*mod)(struct dw_hdmi *hdmi, u8 data, u8 mask, unsigned int reg);
};

#endif
