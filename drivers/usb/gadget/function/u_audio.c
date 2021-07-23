/*
 * u_audio.c -- interface to USB gadget "ALSA sound card" utilities
 *
 * Copyright (C) 2016
 * Author: Ruslan Bilovol <ruslan.bilovol@gmail.com>
 *
 * Sound card implementation was cut-and-pasted with changes
 * from f_uac2.c and has:
 *    Copyright (C) 2011
 *    Yadwinder Singh (yadi.brar01@gmail.com)
 *    Jaswinder Singh (jaswinder.singh@linaro.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/usb/audio.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "u_audio.h"

#define BUFF_SIZE_MAX	(PAGE_SIZE * 16)
#define PRD_SIZE_MAX	PAGE_SIZE
#define MIN_PERIODS	4

static struct class *audio_class;

struct uac_req {
	struct uac_rtd_params *pp; /* parent param */
	struct usb_request *req;
};

/* Runtime data params for one stream */
struct uac_rtd_params {
	struct snd_uac_chip *uac; /* parent chip */
	bool ep_enabled; /* if the ep is enabled */

	struct snd_pcm_substream *ss;

	/* Ring buffer */
	ssize_t hw_ptr;

	void *rbuf;

	unsigned max_psize;	/* MaxPacketSize of endpoint */
	struct uac_req *ureq;

	spinlock_t lock;
};

struct snd_uac_chip {
	struct g_audio *audio_dev;

	struct uac_rtd_params p_prm;
	struct uac_rtd_params c_prm;

	struct snd_card *card;
	struct snd_pcm *pcm;

	/* timekeeping for the playback endpoint */
	unsigned int p_interval;
	unsigned int p_residue;

	/* pre-calculated values for playback iso completion */
	unsigned int p_pktsize;
	unsigned int p_pktsize_residue;
	unsigned int p_framesize;
};

static const struct snd_pcm_hardware uac_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER
		 | SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID
		 | SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.rates = SNDRV_PCM_RATE_CONTINUOUS,
	.periods_max = BUFF_SIZE_MAX / PRD_SIZE_MAX,
	.buffer_bytes_max = BUFF_SIZE_MAX,
	.period_bytes_max = PRD_SIZE_MAX,
	.periods_min = MIN_PERIODS,
};

static void u_audio_iso_complete(struct usb_ep *ep, struct usb_request *req)
{
	unsigned pending;
	unsigned long flags, flags2;
	unsigned int hw_ptr;
	int status = req->status;
	struct uac_req *ur = req->context;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	struct uac_rtd_params *prm = ur->pp;
	struct snd_uac_chip *uac = prm->uac;

	/* i/f shutting down */
	if (!prm->ep_enabled || req->status == -ESHUTDOWN)
		return;

	/*
	 * We can't really do much about bad xfers.
	 * Afterall, the ISOCH xfers could fail legitimately.
	 */
	if (status)
		pr_debug("%s: iso_complete status(%d) %d/%d\n",
			__func__, status, req->actual, req->length);

	substream = prm->ss;

	/* Do nothing if ALSA isn't active */
	if (!substream)
		goto exit;

	snd_pcm_stream_lock_irqsave(substream, flags2);

	runtime = substream->runtime;
	if (!runtime || !snd_pcm_running(substream)) {
		snd_pcm_stream_unlock_irqrestore(substream, flags2);
		goto exit;
	}

	spin_lock_irqsave(&prm->lock, flags);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/*
		 * For each IN packet, take the quotient of the current data
		 * rate and the endpoint's interval as the base packet size.
		 * If there is a residue from this division, add it to the
		 * residue accumulator.
		 */
		req->length = uac->p_pktsize;
		uac->p_residue += uac->p_pktsize_residue;

		/*
		 * Whenever there are more bytes in the accumulator than we
		 * need to add one more sample frame, increase this packet's
		 * size and decrease the accumulator.
		 */
		if (uac->p_residue / uac->p_interval >= uac->p_framesize) {
			req->length += uac->p_framesize;
			uac->p_residue -= uac->p_framesize *
					   uac->p_interval;
		}

		req->actual = req->length;
	}

	hw_ptr = prm->hw_ptr;

	spin_unlock_irqrestore(&prm->lock, flags);

	/* Pack USB load in ALSA ring buffer */
	pending = runtime->dma_bytes - hw_ptr;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (unlikely(pending < req->actual)) {
			memcpy(req->buf, runtime->dma_area + hw_ptr, pending);
			memcpy(req->buf + pending, runtime->dma_area,
			       req->actual - pending);
		} else {
			memcpy(req->buf, runtime->dma_area + hw_ptr,
			       req->actual);
		}
	} else {
		if (unlikely(pending < req->actual)) {
			memcpy(runtime->dma_area + hw_ptr, req->buf, pending);
			memcpy(runtime->dma_area, req->buf + pending,
			       req->actual - pending);
		} else {
			memcpy(runtime->dma_area + hw_ptr, req->buf,
			       req->actual);
		}
	}

	spin_lock_irqsave(&prm->lock, flags);
	/* update hw_ptr after data is copied to memory */
	prm->hw_ptr = (hw_ptr + req->actual) % runtime->dma_bytes;
	hw_ptr = prm->hw_ptr;
	spin_unlock_irqrestore(&prm->lock, flags);
	snd_pcm_stream_unlock_irqrestore(substream, flags2);

	if ((hw_ptr % snd_pcm_lib_period_bytes(substream)) < req->actual)
		snd_pcm_period_elapsed(substream);

exit:
	if (usb_ep_queue(ep, req, GFP_ATOMIC))
		dev_err(uac->card->dev, "%d Error!\n", __LINE__);
}

static int uac_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_uac_chip *uac = snd_pcm_substream_chip(substream);
	struct uac_rtd_params *prm;
	struct g_audio *audio_dev;
	struct uac_params *params;
	unsigned long flags;
	int err = 0;

	audio_dev = uac->audio_dev;
	params = &audio_dev->params;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		prm = &uac->p_prm;
	else
		prm = &uac->c_prm;

	spin_lock_irqsave(&prm->lock, flags);

	/* Reset */
	prm->hw_ptr = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		prm->ss = substream;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		prm->ss = NULL;
		break;
	default:
		err = -EINVAL;
	}

	spin_unlock_irqrestore(&prm->lock, flags);

	/* Clear buffer after Play stops */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && !prm->ss)
		memset(prm->rbuf, 0, prm->max_psize * params->req_number);

	return err;
}

static snd_pcm_uframes_t uac_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_uac_chip *uac = snd_pcm_substream_chip(substream);
	struct uac_rtd_params *prm;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		prm = &uac->p_prm;
	else
		prm = &uac->c_prm;

	return bytes_to_frames(substream->runtime, prm->hw_ptr);
}

static int uac_pcm_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int uac_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int uac_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_uac_chip *uac = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct g_audio *audio_dev = uac->audio_dev;
	struct uac_params *params;
	int p_ssize, c_ssize;
	int p_srate, c_srate;
	int p_chmask, c_chmask;

	params = &audio_dev->params;
	p_ssize = params->p_ssize;
	c_ssize = params->c_ssize;
	p_srate = params->p_srate_active;
	c_srate = params->c_srate_active;
	p_chmask = params->p_chmask;
	c_chmask = params->c_chmask;
	uac->p_residue = 0;

	runtime->hw = uac_pcm_hardware;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		spin_lock_init(&uac->p_prm.lock);
		runtime->hw.rate_min = p_srate;
		switch (p_ssize) {
		case 3:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S24_3LE;
			break;
		case 4:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S32_LE;
			break;
		default:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
			break;
		}
		runtime->hw.channels_min = num_channels(p_chmask);
		runtime->hw.period_bytes_min = 2 * uac->p_prm.max_psize
						/ runtime->hw.periods_min;
	} else {
		spin_lock_init(&uac->c_prm.lock);
		runtime->hw.rate_min = c_srate;
		switch (c_ssize) {
		case 3:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S24_3LE;
			break;
		case 4:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S32_LE;
			break;
		default:
			runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
			break;
		}
		runtime->hw.channels_min = num_channels(c_chmask);
		runtime->hw.period_bytes_min = 2 * uac->c_prm.max_psize
						/ runtime->hw.periods_min;
	}

	runtime->hw.rate_max = runtime->hw.rate_min;
	runtime->hw.channels_max = runtime->hw.channels_min;

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	return 0;
}

static int uac_pcm_rate_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 324000;
	return 0;
}

static int uac_pcm_rate_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uac_chip *uac = snd_kcontrol_chip(kcontrol);
	struct g_audio *audio_dev = uac->audio_dev;
	struct uac_params *params = &audio_dev->params;

	if (kcontrol->private_value == SNDRV_PCM_STREAM_CAPTURE)
		ucontrol->value.integer.value[0] = params->c_srate_active;
	else if (kcontrol->private_value == SNDRV_PCM_STREAM_PLAYBACK)
		ucontrol->value.integer.value[0] = params->p_srate_active;
	else
		return -EINVAL;

	return 0;
}

static struct snd_kcontrol_new uac_pcm_controls[] = {
{
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "Capture Rate",
	.access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info = uac_pcm_rate_info,
	.get = uac_pcm_rate_get,
	.private_value = SNDRV_PCM_STREAM_CAPTURE,
},
{
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "Playback Rate",
	.access = SNDRV_CTL_ELEM_ACCESS_READ | SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info = uac_pcm_rate_info,
	.get = uac_pcm_rate_get,
	.private_value = SNDRV_PCM_STREAM_PLAYBACK,
},
};

/* ALSA cries without these function pointers */
static int uac_pcm_null(struct snd_pcm_substream *substream)
{
	return 0;
}

static const struct snd_pcm_ops uac_pcm_ops = {
	.open = uac_pcm_open,
	.close = uac_pcm_null,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = uac_pcm_hw_params,
	.hw_free = uac_pcm_hw_free,
	.trigger = uac_pcm_trigger,
	.pointer = uac_pcm_pointer,
	.prepare = uac_pcm_null,
};

static inline void free_ep(struct uac_rtd_params *prm, struct usb_ep *ep)
{
	struct snd_uac_chip *uac = prm->uac;
	struct g_audio *audio_dev;
	struct uac_params *params;
	int i;

	if (!prm->ep_enabled)
		return;

	prm->ep_enabled = false;

	audio_dev = uac->audio_dev;
	params = &audio_dev->params;

	for (i = 0; i < params->req_number; i++) {
		if (prm->ureq[i].req) {
			usb_ep_dequeue(ep, prm->ureq[i].req);
			usb_ep_free_request(ep, prm->ureq[i].req);
			prm->ureq[i].req = NULL;
		}
	}

	if (usb_ep_disable(ep))
		dev_err(uac->card->dev, "%s:%d Error!\n", __func__, __LINE__);
}

static struct snd_kcontrol *u_audio_get_ctl(struct g_audio *audio_dev,
		const char *name)
{
	struct snd_ctl_elem_id elem_id;

	memset(&elem_id, 0, sizeof(elem_id));
	elem_id.iface = SNDRV_CTL_ELEM_IFACE_PCM;
	strlcpy(elem_id.name, name, sizeof(elem_id.name));
	return snd_ctl_find_id(audio_dev->uac->card, &elem_id);
}

int u_audio_set_capture_srate(struct g_audio *audio_dev, int srate)
{
	struct snd_kcontrol *ctl = u_audio_get_ctl(audio_dev, "Capture Rate");
	struct uac_params *params = &audio_dev->params;
	int i;

	for (i = 0; i < UAC_MAX_RATES; i++) {
		if (params->c_srate[i] == srate) {
			audio_dev->usb_state[SET_SAMPLE_RATE_OUT] = true;
			schedule_work(&audio_dev->work);

			params->c_srate_active = srate;
			snd_ctl_notify(audio_dev->uac->card,
					SNDRV_CTL_EVENT_MASK_VALUE, &ctl->id);
			return 0;
		}
		if (params->c_srate[i] == 0)
			break;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(u_audio_set_capture_srate);

static void u_audio_set_playback_pktsize(struct g_audio *audio_dev, int srate)
{
	struct uac_params *params = &audio_dev->params;
	struct snd_uac_chip *uac = audio_dev->uac;
	struct usb_gadget *gadget = audio_dev->gadget;
	const struct usb_endpoint_descriptor *ep_desc;
	struct uac_rtd_params *prm;
	unsigned int factor;

	prm = &uac->p_prm;
	/* set srate before starting playback, epin is not configured */
	if (!prm->ep_enabled)
		return;

	ep_desc = audio_dev->in_ep->desc;

	/* pre-calculate the playback endpoint's interval */
	if (gadget->speed == USB_SPEED_FULL)
		factor = 1000;
	else
		factor = 8000;

	/* pre-compute some values for iso_complete() */
	uac->p_framesize = params->p_ssize *
			    num_channels(params->p_chmask);
	uac->p_interval = factor / (1 << (ep_desc->bInterval - 1));
	uac->p_pktsize = min_t(unsigned int,
				uac->p_framesize *
				(params->p_srate_active / uac->p_interval),
				prm->max_psize);

	if (uac->p_pktsize < prm->max_psize)
		uac->p_pktsize_residue = uac->p_framesize *
			(params->p_srate_active % uac->p_interval);
	else
		uac->p_pktsize_residue = 0;
}

int u_audio_set_playback_srate(struct g_audio *audio_dev, int srate)
{
	struct snd_kcontrol *ctl = u_audio_get_ctl(audio_dev, "Playback Rate");
	struct uac_params *params = &audio_dev->params;
	int i;

	for (i = 0; i < UAC_MAX_RATES; i++) {
		if (params->p_srate[i] == srate) {
			audio_dev->usb_state[SET_SAMPLE_RATE_IN] = true;
			schedule_work(&audio_dev->work);

			params->p_srate_active = srate;
			u_audio_set_playback_pktsize(audio_dev, srate);
			snd_ctl_notify(audio_dev->uac->card,
					SNDRV_CTL_EVENT_MASK_VALUE, &ctl->id);
			return 0;
		}
		if (params->p_srate[i] == 0)
			break;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(u_audio_set_playback_srate);

int u_audio_start_capture(struct g_audio *audio_dev)
{
	struct snd_uac_chip *uac = audio_dev->uac;
	struct usb_gadget *gadget = audio_dev->gadget;
	struct device *dev = &gadget->dev;
	struct usb_request *req;
	struct usb_ep *ep;
	struct uac_rtd_params *prm;
	struct uac_params *params = &audio_dev->params;
	int req_len, i;

	audio_dev->usb_state[SET_INTERFACE_OUT] = true;
	audio_dev->stream_state[STATE_OUT] = true;
	schedule_work(&audio_dev->work);

	ep = audio_dev->out_ep;
	prm = &uac->c_prm;
	config_ep_by_speed(gadget, &audio_dev->func, ep);
	req_len = prm->max_psize;

	prm->ep_enabled = true;
	usb_ep_enable(ep);

	for (i = 0; i < params->req_number; i++) {
		if (!prm->ureq[i].req) {
			req = usb_ep_alloc_request(ep, GFP_ATOMIC);
			if (req == NULL)
				return -ENOMEM;

			prm->ureq[i].req = req;
			prm->ureq[i].pp = prm;

			req->zero = 0;
			req->context = &prm->ureq[i];
			req->length = req_len;
			req->complete = u_audio_iso_complete;
			req->buf = prm->rbuf + i * prm->max_psize;
		}

		if (usb_ep_queue(ep, prm->ureq[i].req, GFP_ATOMIC))
			dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(u_audio_start_capture);

void u_audio_stop_capture(struct g_audio *audio_dev)
{
	struct snd_uac_chip *uac = audio_dev->uac;

	free_ep(&uac->c_prm, audio_dev->out_ep);

	audio_dev->usb_state[SET_INTERFACE_OUT] = true;
	audio_dev->stream_state[STATE_OUT] = false;
	schedule_work(&audio_dev->work);
}
EXPORT_SYMBOL_GPL(u_audio_stop_capture);

int u_audio_start_playback(struct g_audio *audio_dev)
{
	struct snd_uac_chip *uac = audio_dev->uac;
	struct usb_gadget *gadget = audio_dev->gadget;
	struct device *dev = &gadget->dev;
	struct usb_request *req;
	struct usb_ep *ep;
	struct uac_rtd_params *prm;
	struct uac_params *params = &audio_dev->params;
	int req_len, i;

	audio_dev->usb_state[SET_INTERFACE_IN] = true;
	audio_dev->stream_state[STATE_IN] = true;
	schedule_work(&audio_dev->work);

	dev_dbg(dev, "start playback with rate %d\n", params->p_srate_active);
	ep = audio_dev->in_ep;
	prm = &uac->p_prm;
	config_ep_by_speed(gadget, &audio_dev->func, ep);

	prm->ep_enabled = true;
	usb_ep_enable(ep);

	u_audio_set_playback_pktsize(audio_dev, params->p_srate_active);
	req_len = uac->p_pktsize;
	uac->p_residue = 0;

	for (i = 0; i < params->req_number; i++) {
		if (!prm->ureq[i].req) {
			req = usb_ep_alloc_request(ep, GFP_ATOMIC);
			if (req == NULL)
				return -ENOMEM;

			prm->ureq[i].req = req;
			prm->ureq[i].pp = prm;

			req->zero = 0;
			req->context = &prm->ureq[i];
			req->length = req_len;
			req->complete = u_audio_iso_complete;
			req->buf = prm->rbuf + i * prm->max_psize;
		}

		if (usb_ep_queue(ep, prm->ureq[i].req, GFP_ATOMIC))
			dev_err(dev, "%s:%d Error!\n", __func__, __LINE__);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(u_audio_start_playback);

void u_audio_stop_playback(struct g_audio *audio_dev)
{
	struct snd_uac_chip *uac = audio_dev->uac;

	free_ep(&uac->p_prm, audio_dev->in_ep);

	audio_dev->usb_state[SET_INTERFACE_IN] = true;
	audio_dev->stream_state[STATE_IN] = false;
	schedule_work(&audio_dev->work);
}
EXPORT_SYMBOL_GPL(u_audio_stop_playback);

int u_audio_fu_set_cmd(struct usb_audio_control *con, u8 cmd, int value)
{
	struct g_audio *audio_dev = (struct g_audio *)con->context;
	struct uac_params *params = &audio_dev->params;

	switch (cmd) {
	case UAC_SET_CUR:
		if (!strncmp(con->name, "Capture Mute", 12)) {
			params->c_mute = value;
			audio_dev->usb_state[SET_MUTE_OUT] = true;
		} else if (!strncmp(con->name, "Capture Volume", 14)) {
			params->c_volume = value;
			audio_dev->usb_state[SET_VOLUME_OUT] = true;
		} else if (!strncmp(con->name, "Playback Mute", 13)) {
			params->p_mute = value;
			audio_dev->usb_state[SET_MUTE_IN] = true;
		} else if (!strncmp(con->name, "Playback Volume", 15)) {
			params->p_volume = value;
			audio_dev->usb_state[SET_VOLUME_IN] = true;
		}
		break;
	case UAC_SET_RES:
		/* fall through */
	default:
		return 0;
	}

	con->data[cmd] = value;
	schedule_work(&audio_dev->work);

	return 0;
}
EXPORT_SYMBOL_GPL(u_audio_fu_set_cmd);

int u_audio_fu_get_cmd(struct usb_audio_control *con, u8 cmd)
{
	return con->data[cmd];
}
EXPORT_SYMBOL_GPL(u_audio_fu_get_cmd);

static void g_audio_work(struct work_struct *data)
{
	struct g_audio *audio = container_of(data, struct g_audio, work);
	struct uac_params *params = &audio->params;
	struct usb_gadget *gadget = audio->gadget;
	struct device *dev = &gadget->dev;
	char *uac_event[4]  = { NULL, NULL, NULL, NULL };
	char str[19];
	int i;

	for (i = 0; i < SET_USB_STATE_MAX; i++) {
		if (!audio->usb_state[i])
			continue;

		switch (i) {
		case SET_INTERFACE_OUT:
			uac_event[0] = "USB_STATE=SET_INTERFACE";
			uac_event[1] = "STREAM_DIRECTION=OUT";
			uac_event[2] = audio->stream_state[STATE_OUT] ?
				       "STREAM_STATE=ON" : "STREAM_STATE=OFF";
			break;
		case SET_INTERFACE_IN:
			uac_event[0] = "USB_STATE=SET_INTERFACE";
			uac_event[1] = "STREAM_DIRECTION=IN";
			uac_event[2] = audio->stream_state[STATE_IN] ?
				       "STREAM_STATE=ON" : "STREAM_STATE=OFF";
			break;
		case SET_SAMPLE_RATE_OUT:
			uac_event[0] = "USB_STATE=SET_SAMPLE_RATE";
			uac_event[1] = "STREAM_DIRECTION=OUT";
			snprintf(str, sizeof(str), "SAMPLE_RATE=%d",
						params->c_srate_active);
			uac_event[2] = str;
			break;
		case SET_SAMPLE_RATE_IN:
			uac_event[0] = "USB_STATE=SET_SAMPLE_RATE";
			uac_event[1] = "STREAM_DIRECTION=IN";
			snprintf(str, sizeof(str), "SAMPLE_RATE=%d",
						params->p_srate_active);
			uac_event[2] = str;
			break;
		case SET_MUTE_OUT:
			uac_event[0] = "USB_STATE=SET_MUTE";
			uac_event[1] = "STREAM_DIRECTION=OUT";
			snprintf(str, sizeof(str), "MUTE=%d", params->c_mute);
			uac_event[2] = str;
			break;
		case SET_MUTE_IN:
			uac_event[0] = "USB_STATE=SET_MUTE";
			uac_event[1] = "STREAM_DIRECTION=IN";
			snprintf(str, sizeof(str), "MUTE=%d", params->p_mute);
			uac_event[2] = str;
			break;
		case SET_VOLUME_OUT:
			uac_event[0] = "USB_STATE=SET_VOLUME";
			uac_event[1] = "STREAM_DIRECTION=OUT";
			snprintf(str, sizeof(str), "VOLUME=%6d", (int16_t)params->c_volume);
			uac_event[2] = str;
			break;
		case SET_VOLUME_IN:
			uac_event[0] = "USB_STATE=SET_VOLUME";
			uac_event[1] = "STREAM_DIRECTION=IN";
			snprintf(str, sizeof(str), "VOLUME=%6d", (int16_t)params->p_volume);
			uac_event[2] = str;
			break;
		default:
			break;
		}

		audio->usb_state[i] = false;
		kobject_uevent_env(&audio->device->kobj, KOBJ_CHANGE,
				   uac_event);
		dev_dbg(dev, "%s: sent uac uevent %s %s %s\n", __func__,
			uac_event[0], uac_event[1], uac_event[2]);
	}
}

int g_audio_setup(struct g_audio *g_audio, const char *pcm_name,
					const char *card_name)
{
	struct snd_uac_chip *uac;
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct uac_params *params;
	int p_chmask, c_chmask;
	int err;
	int i;

	if (!g_audio)
		return -EINVAL;

	uac = kzalloc(sizeof(*uac), GFP_KERNEL);
	if (!uac)
		return -ENOMEM;
	g_audio->uac = uac;
	uac->audio_dev = g_audio;

	params = &g_audio->params;
	p_chmask = params->p_chmask;
	c_chmask = params->c_chmask;

	if (c_chmask) {
		struct uac_rtd_params *prm = &uac->c_prm;

		uac->c_prm.uac = uac;
		prm->max_psize = g_audio->out_ep_maxpsize;

		prm->ureq = kcalloc(params->req_number, sizeof(struct uac_req),
				GFP_KERNEL);
		if (!prm->ureq) {
			err = -ENOMEM;
			goto fail;
		}

		prm->rbuf = kcalloc(params->req_number, prm->max_psize,
				GFP_KERNEL);
		if (!prm->rbuf) {
			prm->max_psize = 0;
			err = -ENOMEM;
			goto fail;
		}
	}

	if (p_chmask) {
		struct uac_rtd_params *prm = &uac->p_prm;

		uac->p_prm.uac = uac;
		prm->max_psize = g_audio->in_ep_maxpsize;

		prm->ureq = kcalloc(params->req_number, sizeof(struct uac_req),
				GFP_KERNEL);
		if (!prm->ureq) {
			err = -ENOMEM;
			goto fail;
		}

		prm->rbuf = kcalloc(params->req_number, prm->max_psize,
				GFP_KERNEL);
		if (!prm->rbuf) {
			prm->max_psize = 0;
			err = -ENOMEM;
			goto fail;
		}
	}

	/* Choose any slot, with no id */
	err = snd_card_new(&g_audio->gadget->dev,
			-1, NULL, THIS_MODULE, 0, &card);
	if (err < 0)
		goto fail;

	uac->card = card;

	/*
	 * Create first PCM device
	 * Create a substream only for non-zero channel streams
	 */
	err = snd_pcm_new(uac->card, pcm_name, 0,
			       p_chmask ? 1 : 0, c_chmask ? 1 : 0, &pcm);
	if (err < 0)
		goto snd_fail;

	strlcpy(pcm->name, pcm_name, sizeof(pcm->name));
	pcm->private_data = uac;
	uac->pcm = pcm;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &uac_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &uac_pcm_ops);

	strlcpy(card->driver, card_name, sizeof(card->driver));
	strlcpy(card->shortname, card_name, sizeof(card->shortname));
	sprintf(card->longname, "%s %i", card_name, card->dev->id);

	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
		snd_dma_continuous_data(GFP_KERNEL), 0, BUFF_SIZE_MAX);

	/* Add controls */
	for (i = 0; i < ARRAY_SIZE(uac_pcm_controls); i++) {
		err = snd_ctl_add(card,
				snd_ctl_new1(&uac_pcm_controls[i], uac));
		if (err < 0)
			goto snd_fail;
	}

	err = snd_card_register(card);
	if (err < 0)
		goto snd_fail;

	g_audio->device = device_create(audio_class, NULL, MKDEV(0, 0), NULL,
					"%s", g_audio->uac->card->longname);
	if (IS_ERR(g_audio->device)) {
		err = PTR_ERR(g_audio->device);
		goto snd_fail;
	}

	INIT_WORK(&g_audio->work, g_audio_work);

	if (!err)
		return 0;

snd_fail:
	snd_card_free(card);
fail:
	kfree(uac->p_prm.ureq);
	kfree(uac->c_prm.ureq);
	kfree(uac->p_prm.rbuf);
	kfree(uac->c_prm.rbuf);
	kfree(uac);

	return err;
}
EXPORT_SYMBOL_GPL(g_audio_setup);

void g_audio_cleanup(struct g_audio *g_audio)
{
	struct snd_uac_chip *uac;
	struct snd_card *card;

	if (!g_audio || !g_audio->uac)
		return;

	cancel_work_sync(&g_audio->work);
	device_destroy(g_audio->device->class, g_audio->device->devt);
	g_audio->device = NULL;

	uac = g_audio->uac;
	card = uac->card;
	if (card)
		snd_card_free(card);

	free_ep(&uac->c_prm, g_audio->out_ep);
	free_ep(&uac->p_prm, g_audio->in_ep);

	kfree(uac->p_prm.ureq);
	kfree(uac->c_prm.ureq);
	kfree(uac->p_prm.rbuf);
	kfree(uac->c_prm.rbuf);
	kfree(uac);
}
EXPORT_SYMBOL_GPL(g_audio_cleanup);

static int __init u_audio_init(void)
{
	int err = 0;

	audio_class = class_create(THIS_MODULE, "u_audio");
	if (IS_ERR(audio_class)) {
		err = PTR_ERR(audio_class);
		audio_class = NULL;
	}

	return err;
}
module_init(u_audio_init);

static void __exit u_audio_exit(void)
{
	if (audio_class)
		class_destroy(audio_class);
}
module_exit(u_audio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USB gadget \"ALSA sound card\" utilities");
MODULE_AUTHOR("Ruslan Bilovol");
