/* http://git.freescale.com/git/cgit.cgi/imx/linux-2.6-imx.git/
 * tree/drivers/mxc/hdmi-cec/mxc_hdmi-cec.c?h=imx_3.0.35_4.1.0 */
#include <linux/hdmi-notifier.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <drm/drm_edid.h>

#include <media/cec.h>
#include <media/cec-edid.h>

#include "dw-hdmi-cec.h"

#define DEV_NAME "dw_hdmi_cec"

enum {
	HDMI_IH_CEC_STAT0	= 0x0106,
	HDMI_IH_MUTE_CEC_STAT0	= 0x0186,

	HDMI_CEC_CTRL		= 0x7d00,
	CEC_CTRL_START		= BIT(0),
	CEC_CTRL_NORMAL		= 1 << 1,

	HDMI_CEC_STAT		= 0x7d01,
	CEC_STAT_DONE		= BIT(0),
	CEC_STAT_EOM		= BIT(1),
	CEC_STAT_NACK		= BIT(2),
	CEC_STAT_ARBLOST	= BIT(3),
	CEC_STAT_ERROR_INIT	= BIT(4),
	CEC_STAT_ERROR_FOLL	= BIT(5),
	CEC_STAT_WAKEUP		= BIT(6),

	HDMI_CEC_MASK		= 0x7d02,
	HDMI_CEC_POLARITY	= 0x7d03,
	HDMI_CEC_INT		= 0x7d04,
	HDMI_CEC_ADDR_L		= 0x7d05,
	HDMI_CEC_ADDR_H		= 0x7d06,
	HDMI_CEC_TX_CNT		= 0x7d07,
	HDMI_CEC_RX_CNT		= 0x7d08,
	HDMI_CEC_TX_DATA0	= 0x7d10,
	HDMI_CEC_RX_DATA0	= 0x7d20,
	HDMI_CEC_LOCK		= 0x7d30,
	HDMI_CEC_WKUPCTRL	= 0x7d31,
};

struct dw_hdmi_cec {
	const struct dw_hdmi_cec_data *data;
	u32 addresses;
	struct cec_adapter *adap;
	struct cec_msg rx_msg;
	unsigned int tx_status;
	bool tx_done;
	bool rx_done;
	int retries;
	struct hdmi_notifier *n;
	struct notifier_block nb;
};

static inline void hdmi_writeb(struct dw_hdmi_cec *cec, u8 val, int offset)
{
	struct dw_hdmi *hdmi = cec->data->hdmi;

	cec->data->write(hdmi, val, offset);
}

static inline u8 hdmi_readb(struct dw_hdmi_cec *cec, int offset)
{
	struct dw_hdmi *hdmi = cec->data->hdmi;

	return cec->data->read(hdmi, offset);
}

static inline void hdmi_update_bits(struct dw_hdmi_cec *cec,
				    u8 data, u8 mask, unsigned int reg)
{
	struct dw_hdmi *hdmi = cec->data->hdmi;

	cec->data->mod(hdmi, data, mask, reg);
}

static int dw_hdmi_cec_log_addr(struct cec_adapter *adap, u8 logical_addr)
{
	struct dw_hdmi_cec *cec = adap->priv;
	u32 addresses;

	if (logical_addr == CEC_LOG_ADDR_INVALID)
		addresses = cec->addresses = BIT(15);
	else
		addresses = cec->addresses |= BIT(logical_addr);

	hdmi_writeb(cec, addresses & 255, HDMI_CEC_ADDR_L);
	hdmi_writeb(cec, addresses >> 8, HDMI_CEC_ADDR_H);

	return 0;
}

static int dw_hdmi_cec_transmit(struct cec_adapter *adap, u8 attempts,
				u32 signal_free_time, struct cec_msg *msg)
{
	struct dw_hdmi_cec *cec = adap->priv;
	unsigned i;

	cec->retries = attempts;

	for (i = 0; i < msg->len; i++)
		hdmi_writeb(cec, msg->msg[i], HDMI_CEC_TX_DATA0 + i);

	hdmi_writeb(cec, msg->len, HDMI_CEC_TX_CNT);
	hdmi_writeb(cec, CEC_CTRL_NORMAL | CEC_CTRL_START, HDMI_CEC_CTRL);

	return 0;
}

static irqreturn_t dw_hdmi_cec_hardirq(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_cec *cec = adap->priv;
	unsigned stat = hdmi_readb(cec, HDMI_IH_CEC_STAT0);
	irqreturn_t ret = IRQ_HANDLED;

	if (stat == 0)
		return IRQ_NONE;

	hdmi_writeb(cec, stat, HDMI_IH_CEC_STAT0);

	if (stat & CEC_STAT_ERROR_INIT) {
		if (cec->retries) {
			unsigned v = hdmi_readb(cec, HDMI_CEC_CTRL);
			hdmi_writeb(cec, v | CEC_CTRL_START, HDMI_CEC_CTRL);
			cec->retries -= 1;
		} else {
			cec->tx_status = CEC_TX_STATUS_MAX_RETRIES;
			cec->tx_done = true;
			ret = IRQ_WAKE_THREAD;
		}
	} else if (stat & CEC_STAT_DONE) {
		cec->tx_status = CEC_TX_STATUS_OK;
		cec->tx_done = true;
		ret = IRQ_WAKE_THREAD;
	} else if (stat & CEC_STAT_NACK) {
		cec->tx_status = CEC_TX_STATUS_NACK;
		cec->tx_done = true;
		ret = IRQ_WAKE_THREAD;
	}

	if (stat & CEC_STAT_EOM) {
		unsigned len, i;

		len = hdmi_readb(cec, HDMI_CEC_RX_CNT);
		if (len > sizeof(cec->rx_msg.msg))
			len = sizeof(cec->rx_msg.msg);

		for (i = 0; i < len; i++)
			cec->rx_msg.msg[i] =
				hdmi_readb(cec, HDMI_CEC_RX_DATA0 + i);

		hdmi_writeb(cec, 0, HDMI_CEC_LOCK);

		cec->rx_msg.len = len;
		smp_wmb();
		cec->rx_done = true;

		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static irqreturn_t dw_hdmi_cec_thread(int irq, void *data)
{
	struct cec_adapter *adap = data;
	struct dw_hdmi_cec *cec = adap->priv;

	if (cec->tx_done) {
		cec->tx_done = false;
		cec_transmit_done(adap, cec->tx_status, 0, 0, 0, 0);
	}
	if (cec->rx_done) {
		cec->rx_done = false;
		smp_rmb();
		cec_received_msg(adap, &cec->rx_msg);
	}
	return IRQ_HANDLED;
}

static int dw_hdmi_cec_enable(struct cec_adapter *adap, bool enable)
{
	struct dw_hdmi_cec *cec = adap->priv;

	if (!enable) {
		hdmi_writeb(cec, ~0, HDMI_CEC_MASK);
		hdmi_writeb(cec, ~0, HDMI_IH_MUTE_CEC_STAT0);
		hdmi_writeb(cec, 0, HDMI_CEC_POLARITY);

		cec->data->ops->disable(cec->data->hdmi);
	} else {
		unsigned irqs;

		hdmi_writeb(cec, 0, HDMI_CEC_CTRL);
		hdmi_writeb(cec, ~0, HDMI_IH_CEC_STAT0);
		hdmi_writeb(cec, 0, HDMI_CEC_LOCK);

		dw_hdmi_cec_log_addr(cec->adap, CEC_LOG_ADDR_INVALID);

		cec->data->ops->enable(cec->data->hdmi);

		irqs = CEC_STAT_ERROR_INIT | CEC_STAT_NACK | CEC_STAT_EOM |
		       CEC_STAT_DONE;
		hdmi_writeb(cec, irqs, HDMI_CEC_POLARITY);
		hdmi_writeb(cec, ~irqs, HDMI_CEC_MASK);
		hdmi_writeb(cec, ~irqs, HDMI_IH_MUTE_CEC_STAT0);
	}
	return 0;
}

static const struct cec_adap_ops dw_hdmi_cec_ops = {
	.adap_enable = dw_hdmi_cec_enable,
	.adap_log_addr = dw_hdmi_cec_log_addr,
	.adap_transmit = dw_hdmi_cec_transmit,
};

static unsigned int parse_hdmi_addr(const struct edid *edid)
{
	if (!edid || edid->extensions == 0)
		return (u16)~0;

	return cec_get_edid_phys_addr((u8 *)edid,
				EDID_LENGTH * (edid->extensions + 1), NULL);
}

static int dw_hdmi_cec_notify(struct notifier_block *nb, unsigned long event,
			      void *data)
{
	struct dw_hdmi_cec *cec = container_of(nb, struct dw_hdmi_cec, nb);
	struct hdmi_notifier *n = data;
	unsigned int phys;

	dev_info(n->dev, "event %lu\n", event);

	switch (event) {
	case HDMI_CONNECTED:
		break;

	case HDMI_DISCONNECTED:
		cec_s_phys_addr(cec->adap, CEC_PHYS_ADDR_INVALID, false);
		break;

	case HDMI_NEW_EDID:
		phys = parse_hdmi_addr(n->edid);
		cec_s_phys_addr(cec->adap, phys, false);
		break;
	}

	return NOTIFY_OK;
}

static void dw_hdmi_cec_del(void *data)
{
	struct dw_hdmi_cec *cec = data;

	cec_delete_adapter(cec->adap);
}

static int dw_hdmi_cec_probe(struct platform_device *pdev)
{
	struct dw_hdmi_cec_data *data = dev_get_platdata(&pdev->dev);
	struct dw_hdmi_cec *cec;
	int ret;

	if (!data)
		return -ENXIO;

	/*
	 * Our device is just a convenience - we want to link to the real
	 * hardware device here, so that userspace can see the association
	 * between the HDMI hardware and its associated CEC chardev.
	 */
	cec = devm_kzalloc(&pdev->dev, sizeof(*cec), GFP_KERNEL);
	if (!cec)
		return -ENOMEM;

	cec->data = data;
	cec->nb.notifier_call = dw_hdmi_cec_notify;

	platform_set_drvdata(pdev, cec);

	hdmi_writeb(cec, 0, HDMI_CEC_TX_CNT);
	hdmi_writeb(cec, ~0, HDMI_CEC_MASK);
	hdmi_writeb(cec, ~0, HDMI_IH_MUTE_CEC_STAT0);
	hdmi_writeb(cec, 0, HDMI_CEC_POLARITY);

	cec->adap = cec_allocate_adapter(&dw_hdmi_cec_ops, cec, "dw_hdmi",
					 CEC_CAP_LOG_ADDRS | CEC_CAP_TRANSMIT |
					 CEC_CAP_RC, CEC_MAX_LOG_ADDRS);
	if (IS_ERR(cec->adap))
		return PTR_ERR(cec->adap);

	/* override the module pointer */
	cec->adap->owner = THIS_MODULE;

	ret = devm_add_action(&pdev->dev, dw_hdmi_cec_del, cec);
	if (ret) {
		cec_delete_adapter(cec->adap);
		return ret;
	}

	ret = devm_request_threaded_irq(&pdev->dev, cec->data->irq,
					dw_hdmi_cec_hardirq,
					dw_hdmi_cec_thread, IRQF_SHARED,
					DEV_NAME, cec->adap);
	if (ret < 0)
		return ret;

	ret = cec_register_adapter(cec->adap, pdev->dev.parent);
	if (ret < 0)
		return ret;

	/*
	 * CEC documentation says we must not call cec_delete_adapter
	 * after a successful call to cec_register_adapter().
	 */
	devm_remove_action(&pdev->dev, dw_hdmi_cec_del, cec);

	cec->n = hdmi_notifier_get(pdev->dev.parent);
	if (!cec->n)
		return -ENOMEM;
	hdmi_notifier_register(cec->n, &cec->nb);

	return 0;
}

static int dw_hdmi_cec_remove(struct platform_device *pdev)
{
	struct dw_hdmi_cec *cec = platform_get_drvdata(pdev);

	hdmi_notifier_unregister(cec->n, &cec->nb);
	hdmi_notifier_put(cec->n);
	cec_unregister_adapter(cec->adap);

	return 0;
}

static struct platform_driver dw_hdmi_cec_driver = {
	.probe	= dw_hdmi_cec_probe,
	.remove	= dw_hdmi_cec_remove,
	.driver = {
		.name = "dw-hdmi-cec",
		.owner = THIS_MODULE,
	},
};
module_platform_driver(dw_hdmi_cec_driver);

MODULE_AUTHOR("Russell King <rmk+kernel@arm.linux.org.uk>");
MODULE_DESCRIPTION("Synopsis Designware HDMI CEC driver for i.MX");
MODULE_LICENSE("GPL");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX "dw-hdmi-cec");
