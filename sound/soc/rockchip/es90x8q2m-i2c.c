/*
 * Driver for ES9x8Q2M
 *
 * Author: Xiao
 *      Copyright 2017 XQY
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "es90x8q2m-dac.h"


/* ES90X8Q2M Codec Private Data */
struct es90x8q2m_dac_priv {
	struct regmap *regmap;
	unsigned int fmt;
};


/* ES90X8Q2M Codec Default Register Value */
static const struct reg_default es90x8q2m_dac_reg_defaults[] = {
	{ ES9028Q2M_REG_10, 0x00 },
	{ ES9028Q2M_REG_20, 0x00 },
	{ ES9028Q2M_REG_21, 0x00 },
	{ ES9028Q2M_REG_22, 0x00 },
	{ ES9028Q2M_REG_23, 0x00 },
//	{ ES9028Q2M_REG_24, 0x00 },
};


static bool es90x8q2m_dac_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ES9028Q2M_REG_10:
	case ES9028Q2M_REG_20:
	case ES9028Q2M_REG_21:
	case ES9028Q2M_REG_22:
	case ES9028Q2M_REG_23:
//	case ES9028Q2M_REG_24:
		return true;

	default:
		return false;
	}
}

static bool es90x8q2m_dac_readable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ES9028Q2M_REG_01:
	case ES9028Q2M_REG_02:
	case ES9028Q2M_REG_10:
	case ES9028Q2M_REG_20:
	case ES9028Q2M_REG_21:
	case ES9028Q2M_REG_22:
	case ES9028Q2M_REG_23:
//	case ES9028Q2M_REG_24:
		return true;

	default:
		return false;
	}
}

static bool es90x8q2m_dac_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ES9028Q2M_REG_01:
	case ES9028Q2M_REG_02:
		return true;

	default:
		return false;
	}
}


/* Volume Scale */
static const DECLARE_TLV_DB_SCALE(volume_tlv, -10000, 100, 0);


/* Filter Type */
static const char * const fir_filter_type_texts[] = {
	"Fast Roll-Off",
	"Slow Roll-Off",
	"Minimum Phase",
};

static SOC_ENUM_SINGLE_DECL(i_sabre_fir_filter_type_enum,
				ES9028Q2M_REG_22, 0, fir_filter_type_texts);


/* IIR Filter Select */
static const char * const iir_filter_texts[] = {
	"47kHz",
	"50kHz",
	"60kHz",
	"70kHz",
};

static SOC_ENUM_SINGLE_DECL(i_sabre_iir_filter_enum,
				ES9028Q2M_REG_23, 0, iir_filter_texts);


/* I2S / SPDIF Select */
/*static const char * const iis_spdif_sel_texts[] = {
	"I2S",
	"SPDIF",
};*/

/*static SOC_ENUM_SINGLE_DECL(i_sabre_iis_spdif_sel_enum,
				ES9028Q2M_REG_24, 0, iis_spdif_sel_texts);
*/

/* Control */
static const struct snd_kcontrol_new es90x8q2m_dac_controls[] = {
SOC_SINGLE_RANGE_TLV("Digital Playback Volume", ES9028Q2M_REG_20, 0, 0, 100, 1, volume_tlv),

//SOC_DOUBLE("Mute Switch", ES9028Q2M_REG_21, 0, 1, 1, 0),

SOC_ENUM("FIR Filter Type", i_sabre_fir_filter_type_enum),
SOC_ENUM("IIR Filter Select", i_sabre_iir_filter_enum),
//SOC_ENUM("I2S/SPDIF Select", i_sabre_iis_spdif_sel_enum),
};


static const u32 es90x8q2m_dac_dai_rates_slave[] = {
	8000, 11025, 16000, 22050, 32000,
	44100, 48000, 64000, 88200, 96000, 176400, 192000, 352800, 384000
};

static const struct snd_pcm_hw_constraint_list constraints_slave = {
	.list  = es90x8q2m_dac_dai_rates_slave,
	.count = ARRAY_SIZE(es90x8q2m_dac_dai_rates_slave),
};

static int es90x8q2m_dac_dai_startup_slave(
		struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret;

	ret = snd_pcm_hw_constraint_list(substream->runtime,
			0, SNDRV_PCM_HW_PARAM_RATE, &constraints_slave);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to setup rates constraints: %d\n", ret);
	}

	return ret;
}

static int es90x8q2m_dac_dai_startup(
		struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct snd_soc_codec      *codec = dai->codec;
	struct es90x8q2m_dac_priv *es90x8q2m_dac
					= snd_soc_codec_get_drvdata(codec);

	switch (es90x8q2m_dac->fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		return es90x8q2m_dac_dai_startup_slave(substream, dai);

	default:
		return (-EINVAL);
	}
}

static int es90x8q2m_dac_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec      *codec = dai->codec;
	struct es90x8q2m_dac_priv *es90x8q2m_dac
					= snd_soc_codec_get_drvdata(codec);
	unsigned int daifmt;
	int format_width;

	dev_dbg(codec->dev, "hw_params %u Hz, %u channels\n",
			params_rate(params), params_channels(params));

	/* Check I2S Format (Bit Size) */
	format_width = snd_pcm_format_width(params_format(params));
	if ((format_width != 32) && (format_width != 16)) {
		dev_err(codec->dev, "Bad frame size: %d\n",
				snd_pcm_format_width(params_format(params)));
		return (-EINVAL);
	}

	/* Check Slave Mode */
	daifmt = es90x8q2m_dac->fmt & SND_SOC_DAIFMT_MASTER_MASK;
	if (daifmt != SND_SOC_DAIFMT_CBS_CFS) {
		return (-EINVAL);
	}

	/* Notify Sampling Frequency  */
	switch (params_rate(params))
	{
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 176400:
	case 192000:
		snd_soc_update_bits(codec, ES9028Q2M_REG_10, 0x01, 0x00);
		break;

	case 352800:
	case 384000:
		snd_soc_update_bits(codec, ES9028Q2M_REG_10, 0x01, 0x01);
		break;
	}

	return 0;
}

static int es90x8q2m_dac_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec      *codec = dai->codec;
	struct es90x8q2m_dac_priv *es90x8q2m_dac
					= snd_soc_codec_get_drvdata(codec);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
	default:
		return (-EINVAL);
	}

	/* clock inversion */
	if ((fmt & SND_SOC_DAIFMT_INV_MASK) != SND_SOC_DAIFMT_NB_NF) {
		return (-EINVAL);
	}

	/* Set Audio Data Format */
	es90x8q2m_dac->fmt = fmt;

	return 0;
}

static int es90x8q2m_dac_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	if (mute) {
//		snd_soc_update_bits(codec, ES9028Q2M_REG_21, 0x01, 0x01);

		snd_soc_update_bits(codec, ES9028Q2M_REG_21, 0x01, 0x00);
	} else {
		snd_soc_update_bits(codec, ES9028Q2M_REG_21, 0x01, 0x00);
	}

	return 0;
}

static const struct snd_soc_dai_ops es90x8q2m_dac_dai_ops = {
	.startup      = es90x8q2m_dac_dai_startup,
	.hw_params    = es90x8q2m_dac_hw_params,
	.set_fmt      = es90x8q2m_dac_set_fmt,
	.digital_mute = es90x8q2m_dac_mute,
};

static struct snd_soc_dai_driver es90x8q2m_dac_dai = {
	.name = "es90x8q2m-dac-dai",
	.playback = {
		.stream_name  = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats      = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &es90x8q2m_dac_dai_ops,
};

static struct snd_soc_codec_driver es90x8q2m_dac_codec_driver = {
	.component_driver = {
		.controls         = es90x8q2m_dac_controls,
		.num_controls     = ARRAY_SIZE(es90x8q2m_dac_controls),
	}
};


static const struct regmap_config es90x8q2m_dac_regmap = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = ES9028Q2M_MAX_REG,

	.reg_defaults     = es90x8q2m_dac_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es90x8q2m_dac_reg_defaults),

	.writeable_reg    = es90x8q2m_dac_writeable,
	.readable_reg     = es90x8q2m_dac_readable,
	.volatile_reg     = es90x8q2m_dac_volatile,

	.cache_type       = REGCACHE_RBTREE,
};


static int es90x8q2m_dac_probe(struct device *dev, struct regmap *regmap)
{
	struct es90x8q2m_dac_priv *es90x8q2m_dac;
	int ret;

	es90x8q2m_dac = devm_kzalloc(dev, sizeof(*es90x8q2m_dac), GFP_KERNEL);
	if (!es90x8q2m_dac) {
		dev_err(dev, "devm_kzalloc");
		return (-ENOMEM);
	}

	es90x8q2m_dac->regmap = regmap;

	dev_set_drvdata(dev, es90x8q2m_dac);

	ret = snd_soc_register_codec(dev,
			&es90x8q2m_dac_codec_driver, &es90x8q2m_dac_dai, 1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}

	return 0;
}

static void es90x8q2m_dac_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);
}


static int es90x8q2m_dac_i2c_probe(
		struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct regmap *regmap;

	regmap = devm_regmap_init_i2c(i2c, &es90x8q2m_dac_regmap);
	if (IS_ERR(regmap)) {
		return PTR_ERR(regmap);
	}

	return es90x8q2m_dac_probe(&i2c->dev, regmap);
}

static int es90x8q2m_dac_i2c_remove(struct i2c_client *i2c)
{
	es90x8q2m_dac_remove(&i2c->dev);

	return 0;
}


static const struct i2c_device_id es90x8q2m_dac_i2c_id[] = {
	{ "es90x8q2m-dac", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es90x8q2m_dac_i2c_id);

static const struct of_device_id es90x8q2m_dac_of_match[] = {
	{ .compatible = "xiao,es90x8q2m-i2c", }, //see es90x8q2m-dac-overlay.dts
	{ }
};
MODULE_DEVICE_TABLE(of, es90x8q2m_dac_of_match);

static struct i2c_driver es90x8q2m_dac_i2c_driver = {
	.driver = {
		.name           = "es90x8q2m-codec-i2c",
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(es90x8q2m_dac_of_match),
	},
	.probe    = es90x8q2m_dac_i2c_probe,
	.remove   = es90x8q2m_dac_i2c_remove,
	.id_table = es90x8q2m_dac_i2c_id,
};
module_i2c_driver(es90x8q2m_dac_i2c_driver);


MODULE_DESCRIPTION("ASoC ES90X8Q2M codec driver");
MODULE_AUTHOR("Xiao <xiaoqingyong@sina.com>");
MODULE_LICENSE("GPL");
