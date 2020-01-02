/*
 * ASoC Driver for Slice on-board sound
 *
 * Author:	James Adams <james@fiveninjas.com>
 * Based on the HifiBerry DAC driver by Florian Meier <florian.meier@koalo.de>
 *		Copyright 2014
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

#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/io.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

struct clk * gp0_clock;

static int snd_slice_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static int snd_slice_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int err;
	int ret;
	unsigned int rate = params_rate(params);
	unsigned int sysclk = 12288000;

	switch (rate) {
		case 32000:
			sysclk = 12288000;
			break;
		case 44100:
			sysclk = 11289600;
			break;
		case 48000:
			sysclk = 12288000;
			break;
		case 64000:
			sysclk = 12288000;
			break;
		case 88200:
			sysclk = 11289600;
			break;
		case 96000:
			sysclk = 12288000;
			break;
		case 128000:
			dev_err(rtd->card->dev,
			"Failed to set CS4265 SYSCLK, sample rate not supported in ALSA: 128000\n");
			break;
		case 176400:
			sysclk = 11289600;
			break;
		case 192000:
			sysclk = 12288000;
			break;
		default:
			dev_err(rtd->card->dev,
			"Failed to set CS4265 SYSCLK, sample rate not supported\n");
			break;
	}

	// Need two frequencies: 12.288 or 11.2896MHz
	// Source is 1,806,336,000
	// /4 /40 - 1128960
	// /7 /21 - 1228800
	clk_disable_unprepare(gp0_clock);

	err = clk_set_rate(gp0_clock, sysclk);
	if(err < 0)
		pr_err("Failed to set clock rate for gp0 clock\n");

	if((ret = clk_prepare_enable(gp0_clock)) < 0)
		pr_err("Failed to enable clock\n");

	dev_err(rtd->card->dev, "Set sampling frequency %d, using sysclk %d\n", rate, sysclk);

	err = snd_soc_dai_set_sysclk(codec_dai, 0, sysclk,
				     SND_SOC_CLOCK_OUT);

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);

	if (ret) {
		dev_err(cpu_dai->dev,
			"Failed to set the cpu dai format.\n");
		return ret;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret) {
		dev_err(cpu_dai->dev,
			"Failed to set the codec format.\n");
		return ret;
	}

	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	return 0;
}

static int snd_slice_params_fixup(struct snd_soc_pcm_runtime *rtd,
            struct snd_pcm_hw_params *params)
{
	printk(KERN_ERR "snd_slice_params_fixup called\n");
	/* force 32 bit */
	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

/* machine stream operations */
static struct snd_soc_ops snd_slice_ops = {
	.hw_params = snd_slice_hw_params,
};

/* Widgets */
static const struct snd_soc_dapm_widget snd_slice_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker 1", NULL),
	SND_SOC_DAPM_SPK("Speaker 2", NULL),
	SND_SOC_DAPM_MIC("Mic 1", NULL),
	SND_SOC_DAPM_MIC("Mic 2", NULL),
	SND_SOC_DAPM_MIC("LineIn 1", NULL),
	SND_SOC_DAPM_MIC("LineIn 2", NULL),
	SND_SOC_DAPM_SPK("Spdif", NULL),
};

/* Audio Map */
static const struct snd_soc_dapm_route snd_slice_audio_map[] = {
	{"Speaker 1", NULL, "LINEOUTL"},
	{"Speaker 2", NULL, "LINEOUTR"},
	{"MICL", NULL, "Mic 1"},
	{"MICR", NULL, "Mic 2"},
	{"LINEINL", NULL, "LineIn 1"},
	{"LINEINR", NULL, "LineIn 2"},
	{"Spdif", NULL, "SPDIF"},
};

static const struct snd_soc_pcm_stream snd_slice_params = {
         .formats = SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE,
};

SND_SOC_DAILINK_DEFS(hifi,
	DAILINK_COMP_ARRAY(COMP_CPU("bcm2708-i2s.0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("cs4265.1-004e", "cs4265-dai1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("bcm2708-i2s.0")));

static struct snd_soc_dai_link snd_slice_dai[] = {
{
	.name		= "Slice",
	.stream_name	= "Slice HiFi",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				SND_SOC_DAIFMT_CBM_CFM,
	.ops		= &snd_slice_ops,
	.init		= snd_slice_init,
	.be_hw_params_fixup = snd_slice_params_fixup,
	SND_SOC_DAILINK_REG(hifi),
},
};

/* audio machine driver */
static struct snd_soc_card snd_slice = {
	.name         = "snd_slice",
	.dai_link     = snd_slice_dai,
	.num_links    = ARRAY_SIZE(snd_slice_dai),
	.fully_routed = 1,
	.dapm_widgets = snd_slice_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(snd_slice_dapm_widgets),
	.dapm_routes = snd_slice_audio_map,
	.num_dapm_routes = ARRAY_SIZE(snd_slice_audio_map),
};

static int snd_slice_probe(struct platform_device *pdev)
{
	int ret = 0;
	snd_slice.dev = &pdev->dev;

	if (pdev->dev.of_node) {
		struct device_node *i2s_node;
		struct snd_soc_dai_link *dai = &snd_slice_dai[0];
		i2s_node = of_parse_phandle(pdev->dev.of_node,
					    "i2s-controller", 0);

		if (i2s_node) {
			dai->cpus->dai_name = NULL;
			dai->cpus->of_node = i2s_node;
			dai->platforms->name = NULL;
			dai->platforms->of_node = i2s_node;
		}
	}
	else
	{
		printk(KERN_ERR "SLICEAUDIO - ERROR no Device Tree!\n");
	}

	ret = snd_soc_register_card(&snd_slice);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_card() failed: %d\n", ret);
		goto snd_soc_register_card_failed;
	}

	gp0_clock = devm_clk_get(&pdev->dev, "gp0");
	if (IS_ERR(gp0_clock)) {
		pr_err("Failed to get gp0 clock\n");
		return PTR_ERR(gp0_clock);
	}

	ret = clk_set_rate(gp0_clock, 12288000);
	if (ret) {
		pr_err("Failed to set the GP0 clock rate\n");
		return ret;
	}

	ret = clk_prepare_enable(gp0_clock);
	if (ret) {
		pr_err("Failed to turn on gp0 clock: %d\n", ret);
		return ret;
	}

	return 0;

snd_soc_register_card_failed:

	return ret;
}

static int snd_slice_remove(struct platform_device *pdev)
{
	return snd_soc_unregister_card(&snd_slice);
}

static const struct of_device_id slice_of_match[] = {
	{ .compatible = "fiveninjas,slice", },
	{},
};
MODULE_DEVICE_TABLE(of, slice_of_match);

static struct platform_driver snd_slice_driver = {
	.driver = {
		.name   = "snd-slice",
		.owner  = THIS_MODULE,
		.of_match_table = slice_of_match,
	},
	.probe          = snd_slice_probe,
	.remove         = snd_slice_remove,
};

module_platform_driver(snd_slice_driver);

MODULE_AUTHOR("James Adams <james@fiveninjas.com>");
MODULE_DESCRIPTION("ASoC Driver for Slice on-board audio");
MODULE_LICENSE("GPL v2");
