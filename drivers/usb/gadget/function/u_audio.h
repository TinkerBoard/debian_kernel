/*
 * u_audio.h -- interface to USB gadget "ALSA sound card" utilities
 *
 * Copyright (C) 2016
 * Author: Ruslan Bilovol <ruslan.bilovol@gmail.com>
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
 *
 */

#ifndef __U_AUDIO_H
#define __U_AUDIO_H

#include <linux/usb/composite.h>

#define UAC_VOLUME_CUR			0x0000
#define UAC_VOLUME_RES			0x0001 /* 1/256 dB */
#define UAC_VOLUME_MAX			0x7FFF /* +127.9961 dB */
#define UAC_VOLUME_MIN			0x8001 /* -127.9961 dB */
#define UAC_MAX_RATES 10
struct uac_params {
	/* playback */
	int p_volume;
	int p_mute;
	int p_chmask;	/* channel mask */
	int p_srate[UAC_MAX_RATES];	/* rate in Hz */
	int p_srate_active;		/* selected rate in Hz */
	int p_ssize;	/* sample size */

	/* capture */
	int c_volume;
	int c_mute;
	int c_chmask;	/* channel mask */
	int c_srate[UAC_MAX_RATES];	/* rate in Hz */
	int c_srate_active;		/* selected rate in Hz */
	int c_ssize;	/* sample size */

	int req_number; /* number of preallocated requests */
};

enum usb_state_index {
	SET_INTERFACE_OUT,
	SET_INTERFACE_IN,
	SET_SAMPLE_RATE_OUT,
	SET_SAMPLE_RATE_IN,
	SET_VOLUME_OUT,
	SET_VOLUME_IN,
	SET_MUTE_OUT,
	SET_MUTE_IN,
	SET_USB_STATE_MAX,
};

enum stream_state_index {
	STATE_OUT,
	STATE_IN,
};

struct g_audio {
	struct device *device;
	bool usb_state[SET_USB_STATE_MAX];
	bool stream_state[2];
	struct work_struct work;

	struct usb_function func;
	struct usb_gadget *gadget;

	struct usb_ep *in_ep;
	struct usb_ep *out_ep;

	/* Max packet size for all in_ep possible speeds */
	unsigned int in_ep_maxpsize;
	/* Max packet size for all out_ep possible speeds */
	unsigned int out_ep_maxpsize;

	/* The ALSA Sound Card it represents on the USB-Client side */
	struct snd_uac_chip *uac;

	struct uac_params params;
};

static inline struct g_audio *func_to_g_audio(struct usb_function *f)
{
	return container_of(f, struct g_audio, func);
}

static inline uint num_channels(uint chanmask)
{
	uint num = 0;

	while (chanmask) {
		num += (chanmask & 1);
		chanmask >>= 1;
	}

	return num;
}

/*
 * g_audio_setup - initialize one virtual ALSA sound card
 * @g_audio: struct with filled params, in_ep_maxpsize, out_ep_maxpsize
 * @pcm_name: the id string for a PCM instance of this sound card
 * @card_name: name of this soundcard
 *
 * This sets up the single virtual ALSA sound card that may be exported by a
 * gadget driver using this framework.
 *
 * Context: may sleep
 *
 * Returns zero on success, or a negative error on failure.
 */
int g_audio_setup(struct g_audio *g_audio, const char *pcm_name,
					const char *card_name);
void g_audio_cleanup(struct g_audio *g_audio);

int u_audio_start_capture(struct g_audio *g_audio);
void u_audio_stop_capture(struct g_audio *g_audio);
int u_audio_start_playback(struct g_audio *g_audio);
void u_audio_stop_playback(struct g_audio *g_audio);
int u_audio_set_capture_srate(struct g_audio *audio_dev, int srate);
int u_audio_set_playback_srate(struct g_audio *audio_dev, int srate);
int u_audio_fu_set_cmd(struct usb_audio_control *con, u8 cmd, int value);
int u_audio_fu_get_cmd(struct usb_audio_control *con, u8 cmd);

#endif /* __U_AUDIO_H */
