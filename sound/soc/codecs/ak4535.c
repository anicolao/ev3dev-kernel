/*
 * ak4535.c  --  AK4535 ALSA Soc Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>

#include "ak4535.h"

#define AK4535_VERSION "0.3"

/* codec private data */
struct ak4535_priv {
	unsigned int sysclk;
	enum snd_soc_control_type control_type;
};

/*
 * ak4535 register cache
 */
static const u8 ak4535_reg[AK4535_CACHEREGNUM] = {
	0x00, 0x80, 0x00, 0x03,
	0x02, 0x00, 0x11, 0x01,
	0x00, 0x40, 0x36, 0x10,
	0x00, 0x00, 0x57, 0x00,
};

static const char *ak4535_mono_gain[] = {"+6dB", "-17dB"};
static const char *ak4535_mono_out[] = {"(L + R)/2", "Hi-Z"};
static const char *ak4535_hp_out[] = {"Stereo", "Mono"};
static const char *ak4535_deemp[] = {"44.1kHz", "Off", "48kHz", "32kHz"};
static const char *ak4535_mic_select[] = {"Internal", "External"};

static const struct soc_enum ak4535_enum[] = {
	SOC_ENUM_SINGLE(AK4535_SIG1, 7, 2, ak4535_mono_gain),
	SOC_ENUM_SINGLE(AK4535_SIG1, 6, 2, ak4535_mono_out),
	SOC_ENUM_SINGLE(AK4535_MODE2, 2, 2, ak4535_hp_out),
	SOC_ENUM_SINGLE(AK4535_DAC, 0, 4, ak4535_deemp),
	SOC_ENUM_SINGLE(AK4535_MIC, 1, 2, ak4535_mic_select),
};

static const struct snd_kcontrol_new ak4535_snd_controls[] = {
	SOC_SINGLE("ALC2 Switch", AK4535_SIG1, 1, 1, 0),
	SOC_ENUM("Mono 1 Output", ak4535_enum[1]),
	SOC_ENUM("Mono 1 Gain", ak4535_enum[0]),
	SOC_ENUM("Headphone Output", ak4535_enum[2]),
	SOC_ENUM("Playback Deemphasis", ak4535_enum[3]),
	SOC_SINGLE("Bass Volume", AK4535_DAC, 2, 3, 0),
	SOC_SINGLE("Mic Boost (+20dB) Switch", AK4535_MIC, 0, 1, 0),
	SOC_ENUM("Mic Select", ak4535_enum[4]),
	SOC_SINGLE("ALC Operation Time", AK4535_TIMER, 0, 3, 0),
	SOC_SINGLE("ALC Recovery Time", AK4535_TIMER, 2, 3, 0),
	SOC_SINGLE("ALC ZC Time", AK4535_TIMER, 4, 3, 0),
	SOC_SINGLE("ALC 1 Switch", AK4535_ALC1, 5, 1, 0),
	SOC_SINGLE("ALC 2 Switch", AK4535_ALC1, 6, 1, 0),
	SOC_SINGLE("ALC Volume", AK4535_ALC2, 0, 127, 0),
	SOC_SINGLE("Capture Volume", AK4535_PGA, 0, 127, 0),
	SOC_SINGLE("Left Playback Volume", AK4535_LATT, 0, 127, 1),
	SOC_SINGLE("Right Playback Volume", AK4535_RATT, 0, 127, 1),
	SOC_SINGLE("AUX Bypass Volume", AK4535_VOL, 0, 15, 0),
	SOC_SINGLE("Mic Sidetone Volume", AK4535_VOL, 4, 7, 0),
};

/* Mono 1 Mixer */
static const struct snd_kcontrol_new ak4535_mono1_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Sidetone Switch", AK4535_SIG1, 4, 1, 0),
	SOC_DAPM_SINGLE("Mono Playback Switch", AK4535_SIG1, 5, 1, 0),
};

/* Stereo Mixer */
static const struct snd_kcontrol_new ak4535_stereo_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Sidetone Switch", AK4535_SIG2, 4, 1, 0),
	SOC_DAPM_SINGLE("Playback Switch", AK4535_SIG2, 7, 1, 0),
	SOC_DAPM_SINGLE("Aux Bypass Switch", AK4535_SIG2, 5, 1, 0),
};

/* Input Mixer */
static const struct snd_kcontrol_new ak4535_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Capture Switch", AK4535_MIC, 2, 1, 0),
	SOC_DAPM_SINGLE("Aux Capture Switch", AK4535_MIC, 5, 1, 0),
};

/* Input mux */
static const struct snd_kcontrol_new ak4535_input_mux_control =
	SOC_DAPM_ENUM("Input Select", ak4535_enum[4]);

/* HP L switch */
static const struct snd_kcontrol_new ak4535_hpl_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 1, 1, 1);

/* HP R switch */
static const struct snd_kcontrol_new ak4535_hpr_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 0, 1, 1);

/* mono 2 switch */
static const struct snd_kcontrol_new ak4535_mono2_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG1, 0, 1, 0);

/* Line out switch */
static const struct snd_kcontrol_new ak4535_line_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 6, 1, 0);

/* ak4535 dapm widgets */
static const struct snd_soc_dapm_widget ak4535_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Stereo Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_stereo_mixer_controls[0],
		ARRAY_SIZE(ak4535_stereo_mixer_controls)),
	SND_SOC_DAPM_MIXER("Mono1 Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_mono1_mixer_controls[0],
		ARRAY_SIZE(ak4535_mono1_mixer_controls)),
	SND_SOC_DAPM_MIXER("Input Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_input_mixer_controls[0],
		ARRAY_SIZE(ak4535_input_mixer_controls)),
	SND_SOC_DAPM_MUX("Input Mux", SND_SOC_NOPM, 0, 0,
		&ak4535_input_mux_control),
	SND_SOC_DAPM_DAC("DAC", "Playback", AK4535_PM2, 0, 0),
	SND_SOC_DAPM_SWITCH("Mono 2 Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_mono2_control),
	/* speaker powersave bit */
	SND_SOC_DAPM_PGA("Speaker Enable", AK4535_MODE2, 0, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("Line Out Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_line_control),
	SND_SOC_DAPM_SWITCH("Left HP Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_hpl_control),
	SND_SOC_DAPM_SWITCH("Right HP Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_hpr_control),
	SND_SOC_DAPM_OUTPUT("LOUT"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("ROUT"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPP"),
	SND_SOC_DAPM_OUTPUT("SPN"),
	SND_SOC_DAPM_OUTPUT("MOUT1"),
	SND_SOC_DAPM_OUTPUT("MOUT2"),
	SND_SOC_DAPM_OUTPUT("MICOUT"),
	SND_SOC_DAPM_ADC("ADC", "Capture", AK4535_PM1, 0, 0),
	SND_SOC_DAPM_PGA("Spk Amp", AK4535_PM2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP R Amp", AK4535_PM2, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP L Amp", AK4535_PM2, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic", AK4535_PM1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Line Out", AK4535_PM1, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono Out", AK4535_PM1, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AUX In", AK4535_PM1, 2, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Int Bias", AK4535_MIC, 3, 0),
	SND_SOC_DAPM_MICBIAS("Mic Ext Bias", AK4535_MIC, 4, 0),
	SND_SOC_DAPM_INPUT("MICIN"),
	SND_SOC_DAPM_INPUT("MICEXT"),
	SND_SOC_DAPM_INPUT("AUX"),
	SND_SOC_DAPM_INPUT("MIN"),
	SND_SOC_DAPM_INPUT("AIN"),
};

static const struct snd_soc_dapm_route ak4535_audio_map[] = {
	/*stereo mixer */
	{"Stereo Mixer", "Playback Switch", "DAC"},
	{"Stereo Mixer", "Mic Sidetone Switch", "Mic"},
	{"Stereo Mixer", "Aux Bypass Switch", "AUX In"},

	/* mono1 mixer */
	{"Mono1 Mixer", "Mic Sidetone Switch", "Mic"},
	{"Mono1 Mixer", "Mono Playback Switch", "DAC"},

	/* Mic */
	{"Mic", NULL, "AIN"},
	{"Input Mux", "Internal", "Mic Int Bias"},
	{"Input Mux", "External", "Mic Ext Bias"},
	{"Mic Int Bias", NULL, "MICIN"},
	{"Mic Ext Bias", NULL, "MICEXT"},
	{"MICOUT", NULL, "Input Mux"},

	/* line out */
	{"LOUT", NULL, "Line Out Enable"},
	{"ROUT", NULL, "Line Out Enable"},
	{"Line Out Enable", "Switch", "Line Out"},
	{"Line Out", NULL, "Stereo Mixer"},

	/* mono1 out */
	{"MOUT1", NULL, "Mono Out"},
	{"Mono Out", NULL, "Mono1 Mixer"},

	/* left HP */
	{"HPL", NULL, "Left HP Enable"},
	{"Left HP Enable", "Switch", "HP L Amp"},
	{"HP L Amp", NULL, "Stereo Mixer"},

	/* right HP */
	{"HPR", NULL, "Right HP Enable"},
	{"Right HP Enable", "Switch", "HP R Amp"},
	{"HP R Amp", NULL, "Stereo Mixer"},

	/* speaker */
	{"SPP", NULL, "Speaker Enable"},
	{"SPN", NULL, "Speaker Enable"},
	{"Speaker Enable", "Switch", "Spk Amp"},
	{"Spk Amp", NULL, "MIN"},

	/* mono 2 */
	{"MOUT2", NULL, "Mono 2 Enable"},
	{"Mono 2 Enable", "Switch", "Stereo Mixer"},

	/* Aux In */
	{"Aux In", NULL, "AUX"},

	/* ADC */
	{"ADC", NULL, "Input Mixer"},
	{"Input Mixer", "Mic Capture Switch", "Mic"},
	{"Input Mixer", "Aux Capture Switch", "Aux In"},
};

static int ak4535_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct ak4535_priv *ak4535 = snd_soc_codec_get_drvdata(codec);

	ak4535->sysclk = freq;
	return 0;
}

static int ak4535_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct ak4535_priv *ak4535 = snd_soc_codec_get_drvdata(codec);
	u8 mode2 = snd_soc_read(codec, AK4535_MODE2) & ~(0x3 << 5);
	int rate = params_rate(params), fs = 256;

	if (rate)
		fs = ak4535->sysclk / rate;

	/* set fs */
	switch (fs) {
	case 1024:
		mode2 |= (0x2 << 5);
		break;
	case 512:
		mode2 |= (0x1 << 5);
		break;
	case 256:
		break;
	}

	/* set rate */
	snd_soc_write(codec, AK4535_MODE2, mode2);
	return 0;
}

static int ak4535_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 mode1 = 0;

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		mode1 = 0x0002;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		mode1 = 0x0001;
		break;
	default:
		return -EINVAL;
	}

	/* use 32 fs for BCLK to save power */
	mode1 |= 0x4;

	snd_soc_write(codec, AK4535_MODE1, mode1);
	return 0;
}

static int ak4535_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, AK4535_DAC);
	if (!mute)
		snd_soc_write(codec, AK4535_DAC, mute_reg & ~0x20);
	else
		snd_soc_write(codec, AK4535_DAC, mute_reg | 0x20);
	return 0;
}

static int ak4535_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		snd_soc_update_bits(codec, AK4535_DAC, 0x20, 0);
		break;
	case SND_SOC_BIAS_PREPARE:
		snd_soc_update_bits(codec, AK4535_DAC, 0x20, 0x20);
		break;
	case SND_SOC_BIAS_STANDBY:
		snd_soc_update_bits(codec, AK4535_PM1, 0x80, 0x80);
		snd_soc_update_bits(codec, AK4535_PM2, 0x80, 0);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, AK4535_PM1, 0x80, 0);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

#define AK4535_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

static const struct snd_soc_dai_ops ak4535_dai_ops = {
	.hw_params	= ak4535_hw_params,
	.set_fmt	= ak4535_set_dai_fmt,
	.digital_mute	= ak4535_mute,
	.set_sysclk	= ak4535_set_dai_sysclk,
};

static struct snd_soc_dai_driver ak4535_dai = {
	.name = "ak4535-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AK4535_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AK4535_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &ak4535_dai_ops,
};

static int ak4535_suspend(struct snd_soc_codec *codec)
{
	ak4535_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int ak4535_resume(struct snd_soc_codec *codec)
{
	snd_soc_cache_sync(codec);
	ak4535_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

static int ak4535_probe(struct snd_soc_codec *codec)
{
	struct ak4535_priv *ak4535 = snd_soc_codec_get_drvdata(codec);
	int ret;

	printk(KERN_INFO "AK4535 Audio Codec %s", AK4535_VERSION);

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, ak4535->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	/* power on device */
	ak4535_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_add_controls(codec, ak4535_snd_controls,
				ARRAY_SIZE(ak4535_snd_controls));
	return 0;
}

/* power down chip */
static int ak4535_remove(struct snd_soc_codec *codec)
{
	ak4535_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_ak4535 = {
	.probe =	ak4535_probe,
	.remove =	ak4535_remove,
	.suspend =	ak4535_suspend,
	.resume =	ak4535_resume,
	.set_bias_level = ak4535_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(ak4535_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = ak4535_reg,
	.dapm_widgets = ak4535_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4535_dapm_widgets),
	.dapm_routes = ak4535_audio_map,
	.num_dapm_routes = ARRAY_SIZE(ak4535_audio_map),
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static __devinit int ak4535_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct ak4535_priv *ak4535;
	int ret;

	ak4535 = devm_kzalloc(&i2c->dev, sizeof(struct ak4535_priv),
			      GFP_KERNEL);
	if (ak4535 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ak4535);
	ak4535->control_type = SND_SOC_I2C;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak4535, &ak4535_dai, 1);
	return ret;
}

static __devexit int ak4535_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id ak4535_i2c_id[] = {
	{ "ak4535", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4535_i2c_id);

static struct i2c_driver ak4535_i2c_driver = {
	.driver = {
		.name = "ak4535-codec",
		.owner = THIS_MODULE,
	},
	.probe =    ak4535_i2c_probe,
	.remove =   __devexit_p(ak4535_i2c_remove),
	.id_table = ak4535_i2c_id,
};
#endif

static int __init ak4535_modinit(void)
{
	int ret = 0;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&ak4535_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register AK4535 I2C driver: %d\n",
		       ret);
	}
#endif
	return ret;
}
module_init(ak4535_modinit);

static void __exit ak4535_exit(void)
{
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&ak4535_i2c_driver);
#endif
}
module_exit(ak4535_exit);

MODULE_DESCRIPTION("Soc AK4535 driver");
MODULE_AUTHOR("Richard Purdie");
MODULE_LICENSE("GPL");
