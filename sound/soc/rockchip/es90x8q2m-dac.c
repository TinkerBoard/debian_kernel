/*
 * ASoC Driver for ESS Q2M
 *
 * Author: Xiao
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "es90x8q2m-dac.h"

#ifdef ROCKCHIP_AUDIO
#define ROCKCHIP_I2S_MCLK 512
#endif

static int snd_soc_es90x8q2m_dac_init(struct snd_soc_pcm_runtime *rtd)
{
	//struct snd_soc_codec *codec = rtd->codec;
	//unsigned int value;

	/* Device ID */
	//value = snd_soc_read(codec, ES9028Q2M_REG_01);
	//dev_info(codec->dev, "xxx Device ID : %02X\n", value);

	/* API revision */
	//value = snd_soc_read(codec, ES9028Q2M_REG_02);
	//dev_info(codec->dev, "xxx API revision : %02X\n", value);

	return 0;
}

static int snd_soc_es90x8q2m_dac_hw_params(
	struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd     = substream->private_data;
	struct snd_soc_dai         *cpu_dai = rtd->cpu_dai;
#ifdef ROCKCHIP_AUDIO
	unsigned int mclk;

	mclk = params_rate(params) * ROCKCHIP_I2S_MCLK;
	return snd_soc_dai_set_sysclk(cpu_dai, 0, mclk,
					SND_SOC_CLOCK_OUT);
#else
	int bclk_ratio;

	bclk_ratio = snd_pcm_format_physical_width(
			params_format(params)) * params_channels(params);
	return snd_soc_dai_set_bclk_ratio(cpu_dai, bclk_ratio);
#endif
}

/* machine stream operations */
static struct snd_soc_ops snd_soc_es90x8q2m_dac_ops = {
	.hw_params = snd_soc_es90x8q2m_dac_hw_params,
};


static struct snd_soc_dai_link snd_rpi_es90x8q2m_dai[] = {
	{
		.name           = "ES90x8Q2M",
		.stream_name    = "ES90x8Q2M DAC",
		.cpu_dai_name   = "bcm2708-i2s.0",
		.codec_dai_name = "es90x8q2m-dac-dai",
		.platform_name  = "bcm2708-i2s.0",
		.codec_name     = "es90x8q2m-codec-i2c.1-0048",
		.dai_fmt        = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBS_CFS,
		.init           = snd_soc_es90x8q2m_dac_init,
		.ops            = &snd_soc_es90x8q2m_dac_ops,
	}
};

/* audio machine driver */
static struct snd_soc_card snd_soc_es90x8q2m_dac = {
	.name      = "ES90x8Q2M DAC",
	.owner     = THIS_MODULE,
	.dai_link  = snd_rpi_es90x8q2m_dai,
	.num_links = ARRAY_SIZE(snd_rpi_es90x8q2m_dai)
};


static int snd_soc_es90x8q2m_dac_probe(struct platform_device *pdev)
{
	int ret = 0;

	snd_soc_es90x8q2m_dac.dev = &pdev->dev;
	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai;

		dai = &snd_rpi_es90x8q2m_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,
							"i2s-controller", 0);
		if (i2s_node) {
			dai->cpu_dai_name     = NULL;
			dai->cpu_of_node      = i2s_node;
			dai->platform_name    = NULL;
			dai->platform_of_node = i2s_node;
		} else {
			dev_err(&pdev->dev,
			    "Property 'i2s-controller' missing or invalid\n");
			return (-EINVAL);
		}

		dai->name        = "ES90x8Q2M";
		dai->stream_name = "ES90x8Q2M DAC";
		dai->dai_fmt     = SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS;
	}

	/* Wait for registering codec driver */
	mdelay(50);

	ret = snd_soc_register_card(&snd_soc_es90x8q2m_dac);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_card() failed: %d\n", ret);
	}

	return ret;
}

static int snd_soc_es90x8q2m_dac_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_soc_es90x8q2m_dac);
}

static const struct of_device_id snd_soc_es90x8q2m_dac_of_match[] = {
	{ .compatible = "xiao,es90x8q2m-dac", }, //see es90x8q2m-dac-overlay.dts
	{}
};
MODULE_DEVICE_TABLE(of, snd_soc_es90x8q2m_dac_of_match);

static struct platform_driver snd_soc_es90x8q2m_dac_driver = {
	.driver = {
		.name           = "snd-soc-es90x8q2m-dac",
		.owner          = THIS_MODULE,
		.of_match_table = snd_soc_es90x8q2m_dac_of_match,
	},
	.probe  = snd_soc_es90x8q2m_dac_probe,
	.remove = snd_soc_es90x8q2m_dac_remove,
};
module_platform_driver(snd_soc_es90x8q2m_dac_driver);

MODULE_DESCRIPTION("ASoC Driver for ES90X8Q2M");
MODULE_AUTHOR("Xiao <http://xiaoqingyong@sina.com>");
MODULE_LICENSE("GPL");
