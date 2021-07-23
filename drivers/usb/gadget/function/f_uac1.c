/*
 * f_uac1.c -- USB Audio Class 1.0 Function (using u_audio API)
 *
 * Copyright (C) 2016 Ruslan Bilovol <ruslan.bilovol@gmail.com>
 * Copyright (C) 2017 Julian Scheel <julian@jusst.de>
 *
 * This driver doesn't expect any real Audio codec to be present
 * on the device - the audio streams are simply sinked to and
 * sourced from a virtual ALSA sound card created.
 *
 * This file is based on f_uac1.c which is
 *   Copyright (C) 2008 Bryan Wu <cooloney@kernel.org>
 *   Copyright (C) 2008 Analog Devices, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/usb/audio.h>
#include <linux/module.h>

#include "u_audio.h"
#include "u_uac.h"

/*
 * DESCRIPTORS ... most are static, but strings and full
 * configuration descriptors are built on demand.
 */

/*
 * We have three interfaces - one AudioControl and two AudioStreaming
 *
 * The driver implements a simple UAC_1 topology.
 * USB-OUT -> IT_1 -> OT_2 -> ALSA_Capture
 * ALSA_Playback -> IT_3 -> OT_4 -> USB-IN
 */
#define F_AUDIO_AC_INTERFACE		0
#define F_AUDIO_AS_OUT_INTERFACE	1
#define F_AUDIO_AS_IN_INTERFACE		2
/* Number of streaming interfaces */
#define F_AUDIO_NUM_INTERFACES		2

static struct usb_interface_assoc_descriptor iad_desc = {
	.bLength = sizeof(iad_desc),
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,

	.bFirstInterface = 0,
	.bFunctionClass = USB_CLASS_AUDIO,
	.bFunctionSubClass = USB_SUBCLASS_AUDIOSTREAMING,
	.bFunctionProtocol = UAC_VERSION_1,
};

/* B.3.1  Standard AC Interface Descriptor */
static struct usb_interface_descriptor ac_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOCONTROL,
};

/*
 * The number of AudioStreaming and MIDIStreaming interfaces
 * in the Audio Interface Collection
 */
DECLARE_UAC_AC_HEADER_DESCRIPTOR(2);

/* B.3.2  Class-Specific AC Interface Descriptor */
static struct uac1_ac_header_descriptor_2 ac_header_desc = {
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_HEADER,
	.bcdADC =		cpu_to_le16(0x0100),
	/* .baInterfaceNr[0] = DYNAMIC */
	/* .baInterfaceNr[1] = DYNAMIC */
};

static struct uac_input_terminal_descriptor usb_out_it_desc = {
	.bLength =		UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_INPUT_TERMINAL,
	.wTerminalType =	cpu_to_le16(UAC_TERMINAL_STREAMING),
	.bAssocTerminal =	0,
	.wChannelConfig =	cpu_to_le16(0x3),
};

DECLARE_UAC_FEATURE_UNIT_DESCRIPTOR(0);

static struct uac_feature_unit_descriptor_0 io_out_ot_fu_desc = {
	.bLength		= UAC_DT_FEATURE_UNIT_SIZE(0),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FEATURE_UNIT,
	.bControlSize		= 2,
	.bmaControls[0]		= (UAC_CONTROL_BIT(UAC_FU_MUTE) |
				   UAC_CONTROL_BIT(UAC_FU_VOLUME)),
};

static struct usb_audio_control c_mute_control = {
	.list = LIST_HEAD_INIT(c_mute_control.list),
	.name = "Capture Mute",
	.type = UAC_FU_MUTE,
	.set = u_audio_fu_set_cmd,
	.get = u_audio_fu_get_cmd,
};

static struct usb_audio_control c_volume_control = {
	.list = LIST_HEAD_INIT(c_volume_control.list),
	.name = "Capture Volume",
	.type = UAC_FU_VOLUME,
	.set = u_audio_fu_set_cmd,
	.get = u_audio_fu_get_cmd,
};

static struct usb_audio_control_selector c_feature_unit = {
	.list = LIST_HEAD_INIT(c_feature_unit.list),
	.name = "Capture Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *)&io_out_ot_fu_desc,
};

static struct uac1_output_terminal_descriptor io_out_ot_desc = {
	.bLength		= UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_OUTPUT_TERMINAL,
	.wTerminalType		= cpu_to_le16(UAC_OUTPUT_TERMINAL_SPEAKER),
	.bAssocTerminal		= 0,
};

static struct uac_input_terminal_descriptor io_in_it_desc = {
	.bLength		= UAC_DT_INPUT_TERMINAL_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_INPUT_TERMINAL,
	.wTerminalType		= cpu_to_le16(UAC_INPUT_TERMINAL_MICROPHONE),
	.bAssocTerminal		= 0,
	.wChannelConfig		= cpu_to_le16(0x3),
};

static struct uac_feature_unit_descriptor_0 usb_in_ot_fu_desc = {
	.bLength		= UAC_DT_FEATURE_UNIT_SIZE(0),
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= UAC_FEATURE_UNIT,
	.bControlSize		= 2,
	.bmaControls[0]		= (UAC_FU_MUTE | UAC_FU_VOLUME),
};

static struct usb_audio_control p_mute_control = {
	.list = LIST_HEAD_INIT(p_mute_control.list),
	.name = "Playback Mute",
	.type = UAC_FU_MUTE,
	.set = u_audio_fu_set_cmd,
	.get = u_audio_fu_get_cmd,
};

static struct usb_audio_control p_volume_control = {
	.list = LIST_HEAD_INIT(p_volume_control.list),
	.name = "Playback Volume",
	.type = UAC_FU_VOLUME,
	.set = u_audio_fu_set_cmd,
	.get = u_audio_fu_get_cmd,
};

static struct usb_audio_control_selector p_feature_unit = {
	.list = LIST_HEAD_INIT(p_feature_unit.list),
	.name = "Playback Mute & Volume Control",
	.type = UAC_FEATURE_UNIT,
	.desc = (struct usb_descriptor_header *)&usb_in_ot_fu_desc,
};

static struct uac1_output_terminal_descriptor usb_in_ot_desc = {
	.bLength =		UAC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_OUTPUT_TERMINAL,
	.wTerminalType =	cpu_to_le16(UAC_TERMINAL_STREAMING),
	.bAssocTerminal =	0,
};

/* B.4.1  Standard AS Interface Descriptor */
static struct usb_interface_descriptor as_out_interface_alt_0_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_out_interface_alt_1_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_in_interface_alt_0_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

static struct usb_interface_descriptor as_in_interface_alt_1_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting =	1,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_AUDIO,
	.bInterfaceSubClass =	USB_SUBCLASS_AUDIOSTREAMING,
};

/* B.4.2  Class-Specific AS Interface Descriptor */
static struct uac1_as_header_descriptor as_out_header_desc = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_AS_GENERAL,
	.bDelay =		1,
	.wFormatTag =		cpu_to_le16(UAC_FORMAT_TYPE_I_PCM),
};

static struct uac1_as_header_descriptor as_in_header_desc = {
	.bLength =		UAC_DT_AS_HEADER_SIZE,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_AS_GENERAL,
	.bDelay =		1,
	.wFormatTag =		cpu_to_le16(UAC_FORMAT_TYPE_I_PCM),
};

DECLARE_UAC_FORMAT_TYPE_I_DISCRETE_DESC(UAC_MAX_RATES);
#define uac_format_type_i_discrete_descriptor \
	uac_format_type_i_discrete_descriptor_##UAC_MAX_RATES

static struct uac_format_type_i_discrete_descriptor as_out_type_i_desc = {
	.bLength =		0, /* filled on rate setup */
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_FORMAT_TYPE,
	.bFormatType =		UAC_FORMAT_TYPE_I,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =		0, /* filled on rate setup */
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_endpoint_descriptor as_out_ep_desc  = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_SYNC_ADAPTIVE
				| USB_ENDPOINT_XFER_ISOC,
	/* .wMaxPacketSize = DYNAMIC */
	.bInterval =		4,
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_out_desc = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype =	UAC_EP_GENERAL,
	.bmAttributes =		1,
	.bLockDelayUnits =	1,
	.wLockDelay =		cpu_to_le16(1),
};

static struct uac_format_type_i_discrete_descriptor as_in_type_i_desc = {
	.bLength =		0, /* filled on rate setup */
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubtype =	UAC_FORMAT_TYPE,
	.bFormatType =		UAC_FORMAT_TYPE_I,
	.bSubframeSize =	2,
	.bBitResolution =	16,
	.bSamFreqType =		0, /* filled on rate setup */
};

/* Standard ISO OUT Endpoint Descriptor */
static struct usb_endpoint_descriptor as_in_ep_desc  = {
	.bLength =		USB_DT_ENDPOINT_AUDIO_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_SYNC_ASYNC
				| USB_ENDPOINT_XFER_ISOC,
	/* .wMaxPacketSize = DYNAMIC */
	.bInterval =		4,
};

/* Class-specific AS ISO OUT Endpoint Descriptor */
static struct uac_iso_endpoint_descriptor as_iso_in_desc = {
	.bLength =		UAC_ISO_ENDPOINT_DESC_SIZE,
	.bDescriptorType =	USB_DT_CS_ENDPOINT,
	.bDescriptorSubtype =	UAC_EP_GENERAL,
	.bmAttributes =		1,
	.bLockDelayUnits =	0,
	.wLockDelay =		0,
};

static struct usb_descriptor_header *f_audio_desc[] = {
	(struct usb_descriptor_header *)&iad_desc,
	(struct usb_descriptor_header *)&ac_interface_desc,
	(struct usb_descriptor_header *)&ac_header_desc,

	(struct usb_descriptor_header *)&usb_out_it_desc,
	(struct usb_descriptor_header *)&io_out_ot_fu_desc,
	(struct usb_descriptor_header *)&io_out_ot_desc,
	(struct usb_descriptor_header *)&io_in_it_desc,
	(struct usb_descriptor_header *)&usb_in_ot_fu_desc,
	(struct usb_descriptor_header *)&usb_in_ot_desc,

	(struct usb_descriptor_header *)&as_out_interface_alt_0_desc,
	(struct usb_descriptor_header *)&as_out_interface_alt_1_desc,
	(struct usb_descriptor_header *)&as_out_header_desc,

	(struct usb_descriptor_header *)&as_out_type_i_desc,

	(struct usb_descriptor_header *)&as_out_ep_desc,
	(struct usb_descriptor_header *)&as_iso_out_desc,

	(struct usb_descriptor_header *)&as_in_interface_alt_0_desc,
	(struct usb_descriptor_header *)&as_in_interface_alt_1_desc,
	(struct usb_descriptor_header *)&as_in_header_desc,

	(struct usb_descriptor_header *)&as_in_type_i_desc,

	(struct usb_descriptor_header *)&as_in_ep_desc,
	(struct usb_descriptor_header *)&as_iso_in_desc,
	NULL,
};

enum {
	STR_ASSOC,
	STR_AC_IF,
	STR_USB_OUT_IT,
	STR_USB_OUT_IT_CH_NAMES,
	STR_IO_OUT_OT_FU,
	STR_IO_OUT_OT,
	STR_IO_IN_IT,
	STR_IO_IN_IT_CH_NAMES,
	STR_USB_IN_OT_FU,
	STR_USB_IN_OT,
	STR_AS_OUT_IF_ALT0,
	STR_AS_OUT_IF_ALT1,
	STR_AS_IN_IF_ALT0,
	STR_AS_IN_IF_ALT1,
};

static struct usb_string strings_uac1[] = {
	[STR_ASSOC].s = "Source/Sink",
	[STR_AC_IF].s = "AC Interface",
	[STR_USB_OUT_IT].s = "Playback Input terminal",
	[STR_USB_OUT_IT_CH_NAMES].s = "Playback Channels",
	[STR_IO_OUT_OT_FU].s = "Playback Feature Unit",
	[STR_IO_OUT_OT].s = "Playback Output terminal",
	[STR_IO_IN_IT].s = "Capture Input terminal",
	[STR_IO_IN_IT_CH_NAMES].s = "Capture Channels",
	[STR_USB_IN_OT_FU].s = "Capture Feature Unit",
	[STR_USB_IN_OT].s = "Capture Output terminal",
	[STR_AS_OUT_IF_ALT0].s = "Playback Inactive",
	[STR_AS_OUT_IF_ALT1].s = "Playback Active",
	[STR_AS_IN_IF_ALT0].s = "Capture Inactive",
	[STR_AS_IN_IF_ALT1].s = "Capture Active",
	{ },
};

static struct usb_gadget_strings str_uac1 = {
	.language = 0x0409,	/* en-us */
	.strings = strings_uac1,
};

static struct usb_gadget_strings *uac1_strings[] = {
	&str_uac1,
	NULL,
};

/*
 * This function is an ALSA sound card following USB Audio Class Spec 1.0.
 */
static void intf_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_uac *uac1 = req->context;
	int status = req->status;
	u32 data = 0;

	switch (status) {
	case 0:				/* normal completion? */
		if (uac1->set_con) {
			memcpy(&data, req->buf, req->length);
			uac1->set_con->set(uac1->set_con, uac1->set_cmd,
					le16_to_cpu(data));
			uac1->set_con = NULL;
		}
		break;
	default:
		break;
	}
}

static void uac_cs_attr_sample_rate(struct usb_ep *ep, struct usb_request *req)
{
	struct usb_function *fn = ep->driver_data;
	struct usb_composite_dev *cdev = fn->config->cdev;
	struct g_audio *agdev = func_to_g_audio(fn);
	struct f_uac *uac1 = func_to_uac(fn);
	struct f_uac_opts *opts = g_audio_to_uac_opts(agdev);
	u8 *buf = (u8 *)req->buf;
	u32 val = 0;

	if (req->actual != 3) {
		WARN(cdev, "Invalid data size for UAC_EP_CS_ATTR_SAMPLE_RATE.\n");
		return;
	}

	val = buf[0] | (buf[1] << 8) | (buf[2] << 16);

	if (uac1->ctl_id == agdev->in_ep->address) {
		opts->p_srate_active = val;
		u_audio_set_playback_srate(agdev, opts->p_srate_active);
	} else if (uac1->ctl_id == agdev->out_ep->address) {
		opts->c_srate_active = val;
		u_audio_set_capture_srate(agdev, opts->c_srate_active);
	}
}

static int audio_set_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_uac		*uac1 = func_to_uac(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &uac1->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel) {
					uac1->set_con = con;
					break;
				}
			}
			break;
		}
	}

	uac1->set_cmd = cmd;
	req->context = uac1;
	req->complete = intf_complete;

	return len;
}

static int audio_get_intf_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct f_uac		*uac1 = func_to_uac(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u8			id = ((le16_to_cpu(ctrl->wIndex) >> 8) & 0xFF);
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			con_sel = (w_value >> 8) & 0xFF;
	u8			cmd = (ctrl->bRequest & 0x0F);
	struct usb_audio_control_selector *cs;
	struct usb_audio_control *con;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, entity %d\n",
			ctrl->bRequest, w_value, len, id);

	list_for_each_entry(cs, &uac1->cs, list) {
		if (cs->id == id) {
			list_for_each_entry(con, &cs->control, list) {
				if (con->type == con_sel && con->get) {
					value = con->get(con, cmd);
					break;
				}
			}
			break;
		}
	}

	req->context = uac1;
	req->complete = intf_complete;
	len = min_t(size_t, sizeof(value), len);
	memcpy(req->buf, &value, len);

	return len;
}

static int audio_set_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = f->config->cdev->req;
	struct f_uac		*uac1 = func_to_uac(f);
	int			value = -EOPNOTSUPP;
	u8			ep = le16_to_cpu(ctrl->wIndex) & 0xff;
	u16			len = le16_to_cpu(ctrl->wLength);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u8			cs = w_value >> 8;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, endpoint %d\n",
			ctrl->bRequest, w_value, len, ep);

	switch (ctrl->bRequest) {
	case UAC_SET_CUR: {
		if (cs == UAC_EP_CS_ATTR_SAMPLE_RATE) {
			cdev->gadget->ep0->driver_data = f;
			uac1->ctl_id = ep;
			req->complete = uac_cs_attr_sample_rate;
		}
		value = len;
		break;
		}
	case UAC_SET_MIN:
		break;

	case UAC_SET_MAX:
		break;

	case UAC_SET_RES:
		break;

	case UAC_SET_MEM:
		break;

	default:
		break;
	}

	return value;
}

static int audio_get_endpoint_req(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = f->config->cdev->req;
	struct g_audio *agdev = func_to_g_audio(f);
	struct f_uac_opts *opts = g_audio_to_uac_opts(agdev);
	u8 *buf = (u8 *)req->buf;
	int value = -EOPNOTSUPP;
	u8 ep = le16_to_cpu(ctrl->wIndex) & 0xff;
	u16 len = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u8 cs = w_value >> 8;
	u32 val = 0;

	DBG(cdev, "bRequest 0x%x, w_value 0x%04x, len %d, endpoint %d\n",
			ctrl->bRequest, w_value, len, ep);

	switch (ctrl->bRequest) {
	case UAC_GET_CUR: {
		if (cs == UAC_EP_CS_ATTR_SAMPLE_RATE) {
			if (ep == agdev->in_ep->address)
				val = opts->p_srate_active;
			else if (ep == agdev->out_ep->address)
				val = opts->c_srate_active;
			buf[2] = (val >> 16) & 0xff;
			buf[1] = (val >> 8) & 0xff;
			buf[0] = val & 0xff;
		}
		value = len;
		break;
		}
	case UAC_GET_MIN:
	case UAC_GET_MAX:
	case UAC_GET_RES:
		value = len;
		break;
	case UAC_GET_MEM:
		break;
	default:
		break;
	}

	return value;
}

static int
f_audio_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything; interface
	 * activation uses set_alt().
	 */
	switch (ctrl->bRequestType) {
	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_set_intf_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE:
		value = audio_get_intf_req(f, ctrl);
		break;

	case USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_set_endpoint_req(f, ctrl);
		break;

	case USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT:
		value = audio_get_endpoint_req(f, ctrl);
		break;

	default:
		ERROR(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		DBG(cdev, "audio req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "audio response on err %d\n", value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int f_audio_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_gadget *gadget = cdev->gadget;
	struct device *dev = &gadget->dev;
	struct f_uac *uac1 = func_to_uac(f);
	int ret = 0;

	/* No i/f has more than 2 alt settings */
	if (alt > 1) {
		dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (intf == uac1->ac_intf) {
		/* Control I/f has only 1 AltSetting - 0 */
		if (alt) {
			dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
			return -EINVAL;
		}
		return 0;
	}

	if (intf == uac1->as_out_intf) {
		uac1->as_out_alt = alt;

		if (alt)
			ret = u_audio_start_capture(&uac1->g_audio);
		else
			u_audio_stop_capture(&uac1->g_audio);
	} else if (intf == uac1->as_in_intf) {
		uac1->as_in_alt = alt;

			if (alt)
				ret = u_audio_start_playback(&uac1->g_audio);
			else
				u_audio_stop_playback(&uac1->g_audio);
	} else {
		dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
		return -EINVAL;
	}

	return ret;
}

static int f_audio_get_alt(struct usb_function *f, unsigned intf)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_gadget *gadget = cdev->gadget;
	struct device *dev = &gadget->dev;
	struct f_uac *uac1 = func_to_uac(f);

	if (intf == uac1->ac_intf)
		return uac1->ac_alt;
	else if (intf == uac1->as_out_intf)
		return uac1->as_out_alt;
	else if (intf == uac1->as_in_intf)
		return uac1->as_in_alt;
	else
		dev_err(dev, "%s:%d Invalid Interface %d!\n",
			__func__, __LINE__, intf);

	return -EINVAL;
}


static void f_audio_disable(struct usb_function *f)
{
	struct f_uac *uac1 = func_to_uac(f);

	uac1->as_out_alt = 0;
	uac1->as_in_alt = 0;

	u_audio_stop_capture(&uac1->g_audio);
}

/*-------------------------------------------------------------------------*/
#define USBDHDR(p) (struct usb_descriptor_header *)(p)

static void setup_descriptor(struct f_uac_opts *opts)
{
	int i = 1;
	u16 len = 0;

	if (EPOUT_EN(opts))
		usb_out_it_desc.bTerminalID = i++;
	if (EPIN_EN(opts))
		io_in_it_desc.bTerminalID = i++;
	if (EPOUT_EN(opts) && EPOUT_FU(opts))
		io_out_ot_fu_desc.bUnitID = i++;
	if (EPIN_EN(opts) && EPIN_FU(opts))
		usb_in_ot_fu_desc.bUnitID = i++;
	if (EPOUT_EN(opts))
		io_out_ot_desc.bTerminalID = i++;
	if (EPIN_EN(opts))
		usb_in_ot_desc.bTerminalID = i++;

	if (EPIN_FU(opts)) {
		usb_in_ot_desc.bSourceID = usb_in_ot_fu_desc.bUnitID;
		usb_in_ot_fu_desc.bSourceID = io_in_it_desc.bTerminalID;
		p_feature_unit.id = usb_in_ot_fu_desc.bUnitID;
	} else {
		usb_in_ot_desc.bSourceID = io_in_it_desc.bTerminalID;
	}
	if (EPOUT_FU(opts)) {
		io_out_ot_desc.bSourceID = io_out_ot_fu_desc.bUnitID;
		io_out_ot_fu_desc.bSourceID = usb_out_it_desc.bTerminalID;
		c_feature_unit.id = io_out_ot_fu_desc.bUnitID;
	} else {
		io_out_ot_desc.bSourceID = usb_out_it_desc.bTerminalID;
	}
	as_out_header_desc.bTerminalLink = usb_out_it_desc.bTerminalID;
	as_in_header_desc.bTerminalLink = usb_in_ot_desc.bTerminalID;

	iad_desc.bInterfaceCount = 1;
	ac_header_desc.bInCollection = 0;

	if (EPIN_EN(opts)) {
		len += UAC_DT_INPUT_TERMINAL_SIZE + UAC_DT_OUTPUT_TERMINAL_SIZE;
		if (EPIN_FU(opts))
			len += UAC_DT_FEATURE_UNIT_SIZE(0);
		iad_desc.bInterfaceCount++;
		ac_header_desc.bInCollection++;
	}

	if (EPOUT_EN(opts)) {
		len += UAC_DT_INPUT_TERMINAL_SIZE + UAC_DT_OUTPUT_TERMINAL_SIZE;
		if (EPOUT_FU(opts))
			len += UAC_DT_FEATURE_UNIT_SIZE(0);
		iad_desc.bInterfaceCount++;
		ac_header_desc.bInCollection++;
	}
	ac_header_desc.bLength =
		UAC_DT_AC_HEADER_SIZE(ac_header_desc.bInCollection);
	ac_header_desc.wTotalLength = cpu_to_le16(len + ac_header_desc.bLength);

	i = 0;
	f_audio_desc[i++] = USBDHDR(&iad_desc);
	f_audio_desc[i++] = USBDHDR(&ac_interface_desc);
	f_audio_desc[i++] = USBDHDR(&ac_header_desc);

	if (EPOUT_EN(opts)) {
		f_audio_desc[i++] = USBDHDR(&usb_out_it_desc);
		if (EPOUT_FU(opts))
			f_audio_desc[i++] = USBDHDR(&io_out_ot_fu_desc);
		f_audio_desc[i++] = USBDHDR(&io_out_ot_desc);
	}

	if (EPIN_EN(opts)) {
		f_audio_desc[i++] = USBDHDR(&io_in_it_desc);
		if (EPIN_FU(opts))
			f_audio_desc[i++] = USBDHDR(&usb_in_ot_fu_desc);
		f_audio_desc[i++] = USBDHDR(&usb_in_ot_desc);
	}

	if (EPOUT_EN(opts)) {
		f_audio_desc[i++] = USBDHDR(&as_out_interface_alt_0_desc);
		f_audio_desc[i++] = USBDHDR(&as_out_interface_alt_1_desc);
		f_audio_desc[i++] = USBDHDR(&as_out_header_desc);
		f_audio_desc[i++] = USBDHDR(&as_out_type_i_desc);
		f_audio_desc[i++] = USBDHDR(&as_out_ep_desc);
		f_audio_desc[i++] = USBDHDR(&as_iso_out_desc);
	}

	if (EPIN_EN(opts)) {
		f_audio_desc[i++] = USBDHDR(&as_in_interface_alt_0_desc);
		f_audio_desc[i++] = USBDHDR(&as_in_interface_alt_1_desc);
		f_audio_desc[i++] = USBDHDR(&as_in_header_desc);
		f_audio_desc[i++] = USBDHDR(&as_in_type_i_desc);
		f_audio_desc[i++] = USBDHDR(&as_in_ep_desc);
		f_audio_desc[i++] = USBDHDR(&as_iso_in_desc);
	}
	f_audio_desc[i++] = NULL;
}

static int set_ep_max_packet_size(const struct f_uac_opts *opts,
	struct usb_endpoint_descriptor *ep_desc,
	enum usb_device_speed speed, bool is_playback)
{
	int chmask, srate = 0, ssize;
	u16 max_size_bw, max_size_ep;
	unsigned int factor;
	int i;

	switch (speed) {
	case USB_SPEED_FULL:
		max_size_ep = 1023;
		factor = 1000;
		break;

	case USB_SPEED_HIGH:
		max_size_ep = 1024;
		factor = 8000;
		break;

	default:
		return -EINVAL;
	}

	if (is_playback) {
		chmask = opts->p_chmask;
		for (i = 0; i < UAC_MAX_RATES; i++) {
			if (opts->p_srate[i] == 0)
				break;
			if (opts->p_srate[i] > srate)
				srate = opts->p_srate[i];
		}
		ssize = opts->p_ssize;
	} else {
		chmask = opts->c_chmask;
		for (i = 0; i < UAC_MAX_RATES; i++) {
			if (opts->c_srate[i] == 0)
				break;
			if (opts->c_srate[i] > srate)
				srate = opts->c_srate[i];
		}
		ssize = opts->c_ssize;
	}

	max_size_bw = num_channels(chmask) * ssize *
		((srate / (factor / (1 << (ep_desc->bInterval - 1)))) + 1);
	ep_desc->wMaxPacketSize = cpu_to_le16(min_t(u16, max_size_bw,
						    max_size_ep));

	return 0;
}

/* audio function driver setup/binding */
static int f_audio_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev	*cdev = c->cdev;
	struct usb_gadget		*gadget = cdev->gadget;
	struct f_uac			*uac1 = func_to_uac(f);
	struct g_audio			*audio = func_to_g_audio(f);
	struct f_uac_opts		*audio_opts;
	struct usb_ep			*ep = NULL;
	struct usb_string		*us;
	struct device			*dev = &gadget->dev;
	int				status;
	int				idx, i;

	audio_opts = container_of(f->fi, struct f_uac_opts, func_inst);

	us = usb_gstrings_attach(cdev, uac1_strings, ARRAY_SIZE(strings_uac1));
	if (IS_ERR(us))
		return PTR_ERR(us);

	iad_desc.iFunction = us[STR_ASSOC].id;
	ac_interface_desc.iInterface = us[STR_AC_IF].id;
	usb_out_it_desc.iTerminal = us[STR_USB_OUT_IT].id;
	usb_out_it_desc.iChannelNames = us[STR_USB_OUT_IT_CH_NAMES].id;
	io_out_ot_fu_desc.iFeature = us[STR_IO_OUT_OT_FU].id;
	io_out_ot_desc.iTerminal = us[STR_IO_OUT_OT].id;
	as_out_interface_alt_0_desc.iInterface = us[STR_AS_OUT_IF_ALT0].id;
	as_out_interface_alt_1_desc.iInterface = us[STR_AS_OUT_IF_ALT1].id;
	io_in_it_desc.iTerminal = us[STR_IO_IN_IT].id;
	io_in_it_desc.iChannelNames = us[STR_IO_IN_IT_CH_NAMES].id;
	usb_in_ot_fu_desc.iFeature = us[STR_USB_IN_OT_FU].id;
	usb_in_ot_desc.iTerminal = us[STR_USB_IN_OT].id;
	as_in_interface_alt_0_desc.iInterface = us[STR_AS_IN_IF_ALT0].id;
	as_in_interface_alt_1_desc.iInterface = us[STR_AS_IN_IF_ALT1].id;

	/* Set channel numbers */
	usb_out_it_desc.bNrChannels = num_channels(audio_opts->c_chmask);
	usb_out_it_desc.wChannelConfig = cpu_to_le16(audio_opts->c_chmask);
	as_out_type_i_desc.bNrChannels = num_channels(audio_opts->c_chmask);
	as_out_type_i_desc.bSubframeSize = audio_opts->c_ssize;
	as_out_type_i_desc.bBitResolution = audio_opts->c_ssize * 8;
	io_in_it_desc.bNrChannels = num_channels(audio_opts->p_chmask);
	io_in_it_desc.wChannelConfig = cpu_to_le16(audio_opts->p_chmask);
	as_in_type_i_desc.bNrChannels = num_channels(audio_opts->p_chmask);
	as_in_type_i_desc.bSubframeSize = audio_opts->p_ssize;
	as_in_type_i_desc.bBitResolution = audio_opts->p_ssize * 8;

	/* Set sample rates */
	for (i = 0, idx = 0; i < UAC_MAX_RATES; i++) {
		if (audio_opts->c_srate[i] == 0)
			break;
		memcpy(as_out_type_i_desc.tSamFreq[idx++],
				&audio_opts->c_srate[i], 3);
	}

	/*
	 * Calculate wMaxPacketSize according to audio bandwidth.
	 * Set the max packet with USB_SPEED_HIGH by default to
	 * be compatible with larger bandwidth requirements for
	 * high speed mode.
	 */
	status = set_ep_max_packet_size(audio_opts, &as_out_ep_desc,
					USB_SPEED_HIGH, false);
	if (status < 0) {
		dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
		goto fail;
	}

	status = set_ep_max_packet_size(audio_opts, &as_in_ep_desc,
					USB_SPEED_HIGH, true);
	if (status < 0) {
		dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
		goto fail;
	}

	as_out_type_i_desc.bLength = UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(idx);
	as_out_type_i_desc.bSamFreqType = idx;

	for (i = 0, idx = 0; i < UAC_MAX_RATES; i++) {
		if (audio_opts->p_srate[i] == 0)
			break;
		memcpy(as_in_type_i_desc.tSamFreq[idx++],
				&audio_opts->p_srate[i], 3);
	}
	as_in_type_i_desc.bLength = UAC_FORMAT_TYPE_I_DISCRETE_DESC_SIZE(idx);
	as_in_type_i_desc.bSamFreqType = idx;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	iad_desc.bFirstInterface = status;
	ac_interface_desc.bInterfaceNumber = status;
	uac1->ac_intf = status;
	uac1->ac_alt = 0;
	ac_header_desc.baInterfaceNr[0] = ++status;
	ac_header_desc.baInterfaceNr[1] = ++status;

	if (EPOUT_EN(audio_opts)) {
		status = usb_interface_id(c, f);
		if (status < 0)
			goto fail;
		as_out_interface_alt_0_desc.bInterfaceNumber = status;
		as_out_interface_alt_1_desc.bInterfaceNumber = status;
		uac1->as_out_intf = status;
		uac1->as_out_alt = 0;
	}

	if (EPIN_EN(audio_opts)) {
		status = usb_interface_id(c, f);
		if (status < 0)
			goto fail;
		as_in_interface_alt_0_desc.bInterfaceNumber = status;
		as_in_interface_alt_1_desc.bInterfaceNumber = status;
		uac1->as_in_intf = status;
		uac1->as_in_alt = 0;
	}

	audio->gadget = gadget;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	if (EPOUT_EN(audio_opts)) {
		ep = usb_ep_autoconfig(cdev->gadget, &as_out_ep_desc);
		if (!ep)
			goto fail;
		audio->out_ep = ep;
		audio->out_ep->desc = &as_out_ep_desc;
	}

	if (EPIN_EN(audio_opts)) {
		ep = usb_ep_autoconfig(cdev->gadget, &as_in_ep_desc);
		if (!ep)
			goto fail;
		ep->maxpacket = usb_endpoint_maxp(&as_in_ep_desc) & 0x7ff;
		audio->in_ep = ep;
		audio->in_ep->desc = &as_in_ep_desc;
	}

	setup_descriptor(audio_opts);
	/* copy descriptors, and track endpoint copies */
	status = usb_assign_descriptors(f, f_audio_desc, f_audio_desc, NULL);
	if (status)
		goto fail;

	audio->out_ep_maxpsize = le16_to_cpu(as_out_ep_desc.wMaxPacketSize);
	audio->in_ep_maxpsize = le16_to_cpu(as_in_ep_desc.wMaxPacketSize);
	audio->params.c_chmask = audio_opts->c_chmask;
	memcpy(audio->params.c_srate, audio_opts->c_srate,
			sizeof(audio->params.c_srate));
	audio->params.c_srate_active = audio_opts->c_srate_active;
	audio->params.c_ssize = audio_opts->c_ssize;
	audio->params.p_chmask = audio_opts->p_chmask;
	memcpy(audio->params.p_srate, audio_opts->p_srate,
			sizeof(audio->params.p_srate));
	audio->params.p_srate_active = audio_opts->p_srate_active;
	audio->params.p_ssize = audio_opts->p_ssize;
	audio->params.req_number = audio_opts->req_number;

	status = g_audio_setup(audio, "UAC1_PCM", "UAC1_Gadget");
	if (status)
		goto err_card_register;

	return 0;

err_card_register:
	usb_free_all_descriptors(f);
fail:
	return status;
}

/*-------------------------------------------------------------------------*/

/* Todo: add more control selecotor dynamically */
static int control_selector_init(struct f_uac *uac1)
{
	INIT_LIST_HEAD(&uac1->cs);

	/* playback feature unit */
	list_add(&p_feature_unit.list, &uac1->cs);

	INIT_LIST_HEAD(&p_feature_unit.control);
	list_add(&p_mute_control.list, &p_feature_unit.control);
	list_add(&p_volume_control.list, &p_feature_unit.control);

	p_volume_control.data[UAC__CUR] = UAC_VOLUME_CUR;
	p_volume_control.data[UAC__MIN] = UAC_VOLUME_MIN;
	p_volume_control.data[UAC__MAX] = UAC_VOLUME_MAX;
	p_volume_control.data[UAC__RES] = UAC_VOLUME_RES;

	p_volume_control.context = &uac1->g_audio;
	p_mute_control.context = &uac1->g_audio;

	/* capture feature unit */
	list_add(&c_feature_unit.list, &uac1->cs);

	INIT_LIST_HEAD(&c_feature_unit.control);
	list_add(&c_mute_control.list, &c_feature_unit.control);
	list_add(&c_volume_control.list, &c_feature_unit.control);

	c_volume_control.data[UAC__CUR] = UAC_VOLUME_CUR;
	c_volume_control.data[UAC__MIN] = UAC_VOLUME_MIN;
	c_volume_control.data[UAC__MAX] = UAC_VOLUME_MAX;
	c_volume_control.data[UAC__RES] = UAC_VOLUME_RES;

	c_volume_control.context = &uac1->g_audio;
	c_mute_control.context = &uac1->g_audio;

	return 0;
}

static struct configfs_item_operations f_uac1_item_ops = {
	.release	= f_uac_attr_release,
};

UAC_ATTRIBUTE(c_chmask);
UAC_ATTRIBUTE(c_ssize);
UAC_ATTRIBUTE(c_feature_unit);
UAC_ATTRIBUTE(p_chmask);
UAC_ATTRIBUTE(p_ssize);
UAC_ATTRIBUTE(p_feature_unit);
UAC_ATTRIBUTE(req_number);

UAC_RATE_ATTRIBUTE(p_srate);
UAC_RATE_ATTRIBUTE(c_srate);

static struct configfs_attribute *f_uac1_attrs[] = {
	&f_uac_opts_attr_c_chmask,
	&f_uac_opts_attr_c_srate,
	&f_uac_opts_attr_c_ssize,
	&f_uac_opts_attr_c_feature_unit,
	&f_uac_opts_attr_p_chmask,
	&f_uac_opts_attr_p_srate,
	&f_uac_opts_attr_p_ssize,
	&f_uac_opts_attr_p_feature_unit,
	&f_uac_opts_attr_req_number,
	NULL,
};

static const struct config_item_type f_uac1_func_type = {
	.ct_item_ops	= &f_uac1_item_ops,
	.ct_attrs	= f_uac1_attrs,
	.ct_owner	= THIS_MODULE,
};

static void f_audio_free_inst(struct usb_function_instance *f)
{
	struct f_uac_opts *opts;

	opts = container_of(f, struct f_uac_opts, func_inst);
	kfree(opts);
}

static struct usb_function_instance *f_audio_alloc_inst(void)
{
	struct f_uac_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = f_audio_free_inst;

	config_group_init_type_name(&opts->func_inst.group, "",
				    &f_uac1_func_type);

	opts->c_chmask = UAC_DEF_CCHMASK;
	opts->c_srate[0] = UAC_DEF_CSRATE;
	opts->c_srate_active = UAC_DEF_CSRATE;
	opts->c_ssize = UAC_DEF_CSSIZE;
	opts->c_feature_unit = UAC_DEF_CFU;
	opts->p_chmask = UAC_DEF_PCHMASK;
	opts->p_srate[0] = UAC_DEF_PSRATE;
	opts->p_srate_active = UAC_DEF_PSRATE;
	opts->p_ssize = UAC_DEF_PSSIZE;
	opts->p_feature_unit = UAC_DEF_PFU;
	opts->req_number = UAC_DEF_REQ_NUM;
	return &opts->func_inst;
}

static void f_audio_free(struct usb_function *f)
{
	struct g_audio *audio;
	struct f_uac_opts *opts;

	audio = func_to_g_audio(f);
	opts = container_of(f->fi, struct f_uac_opts, func_inst);
	kfree(audio);
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);
}

static void f_audio_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct g_audio *audio = func_to_g_audio(f);

	g_audio_cleanup(audio);
	usb_free_all_descriptors(f);

	audio->gadget = NULL;
}

static struct usb_function *f_audio_alloc(struct usb_function_instance *fi)
{
	struct f_uac *uac1;
	struct f_uac_opts *opts;

	/* allocate and initialize one new instance */
	uac1 = kzalloc(sizeof(*uac1), GFP_KERNEL);
	if (!uac1)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_uac_opts, func_inst);
	mutex_lock(&opts->lock);
	++opts->refcnt;
	mutex_unlock(&opts->lock);

	uac1->g_audio.func.name = "uac1_func";
	uac1->g_audio.func.bind = f_audio_bind;
	uac1->g_audio.func.unbind = f_audio_unbind;
	uac1->g_audio.func.set_alt = f_audio_set_alt;
	uac1->g_audio.func.get_alt = f_audio_get_alt;
	uac1->g_audio.func.setup = f_audio_setup;
	uac1->g_audio.func.disable = f_audio_disable;
	uac1->g_audio.func.free_func = f_audio_free;

	control_selector_init(uac1);

	return &uac1->g_audio.func;
}

DECLARE_USB_FUNCTION_INIT(uac1, f_audio_alloc_inst, f_audio_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ruslan Bilovol");
