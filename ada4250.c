// SPDX-License-Identifier: GPL-2.0+
/*
 * ADA4250 driver
 *
 * Copyright 2021 Analog Devices Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>
#include <linux/regmap.h>
#include <linux/bitfield.h>

/* ADA4250 Register Map */
#define ADA4250_REG_GAIN_MUX        0x00
#define ADA4250_REG_REFBUF_EN       0x01
#define ADA4250_REG_RESET           0x02
#define ADA4250_REG_SNSR_CAL_VAL    0x04
#define ADA4250_REG_SNSR_CAL_CNFG   0x05
#define ADA4250_REG_DIE_REV         0x18
#define ADA4250_REG_CHIP_ID1        0x19
#define ADA4250_REG_CHIP_ID2        0x1a

/* ADA4250_REG_GAIN_MUX Map */
#define ADA4250_GAIN_MUX_MSK        GENMASK(2, 0)
#define ADA4250_GAIN_MUX(x)         FIELD_PREP(ADA4250_GAIN_MUX_MSK, x)

/* ADA4250_REG_REFBUF Map */
#define ADA4250_REFBUF_MSK          BIT(0)
#define ADA4250_REFBUF(x)           FIELD_PREP(ADA4250_REFBUF_MSK, x)

/* ADA4250_REG_RESET Map */
#define ADA4250_RESET_MSK           BIT(0)
#define ADA4250_RESET(x)            FIELD_PREP(ADA4250_RESET_MSK, x)

/* ADA4250_REG_SNSR_CAL_VAL Map */
#define ADA4250_SNSR_CAL_VAL_MSK    GENMASK(7, 0)
#define ADA4250_SNSR_CAL_VAL(x)     FIELD_PREP(ADA4250_SNSR_CAL_VAL_MSK, x)

/* ADA4250_REG_SNSR_CAL_CNFG Bit Definition */
#define ADA4250_BIAS_SET_MSK        GENMASK(1, 0)
#define ADA4250_BIAS_SET(x)         FIELD_PREP(ADA4250_SNSR_CAL_VAL_MSK, x)
#define ADA4250_RANGE_SET_MSK       GENMASK(3, 2)
#define ADA4250_RANGE_SET(x)        FIELD_PREP(ADA4250_SNSR_CAL_VAL_MSK, x)

enum supported_parts {
	ADA4250,
};

enum ada4250_bias {
	ADA4250_BIAS_DISABLE,
	ADA4250_BIAS_BANDGAP_REF,
	ADA4250_BIAS_AVDD
};

struct ada4250 {
	struct regmap		*regmap;
	enum ada4250_bias	bias;
};

static const struct regmap_config ada4250_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.read_flag_mask = BIT(7),
	.max_register = 0x1A,
};

static int ada4250_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long info)
{
	struct ada4250 *dev = iio_priv(indio_dev);
	int ret;

	switch (info) {
	case IIO_CHAN_INFO_HARDWAREGAIN:
		ret = regmap_read(dev->regmap, ADA4250_REG_GAIN_MUX, val);
		if (ret < 0)
			return ret;

		*val = ADA4250_GAIN_MUX((u8)(*val));

		return IIO_VAL_INT;
	}

	return 0;
}

static int ada4250_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int val, int val2, long info)
{
	struct ada4250 *dev = iio_priv(indio_dev);

	switch (info) {
	case IIO_CHAN_INFO_HARDWAREGAIN:
		return regmap_write(dev->regmap, ADA4250_REG_GAIN_MUX,
			ADA4250_GAIN_MUX((u8)val));
	}

	return 0;
}

static int ada4250_reg_access(struct iio_dev *indio_dev,
	unsigned int reg,
	unsigned int write_val,
	unsigned int *read_val)
{
	struct ada4250 *dev = iio_priv(indio_dev);

	if (read_val)
		return regmap_read(dev->regmap, reg, read_val);
	else
		return regmap_write(dev->regmap, reg, write_val);
}

static const struct iio_info ada4250_info = {
	.read_raw = ada4250_read_raw,
	.write_raw = ada4250_write_raw,
	.debugfs_reg_access = &ada4250_reg_access,
};

#define ADA4250_CHAN(_channel) {				\
	.type = IIO_VOLTAGE,					\
	.output = 1,						\
	.indexed = 1,						\
	.channel = _channel,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_HARDWAREGAIN) | \
		BIT(IIO_CHAN_INFO_OFFSET), 			\
}

static const struct iio_chan_spec ada4250_channels[] = {
	ADA4250_CHAN(0),
};

static int ada4250_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct regmap *regmap;
	struct ada4250 *dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*dev));
	if (!indio_dev)
		return -ENOMEM;

	regmap = devm_regmap_init_spi(spi, &ada4250_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	spi_set_drvdata(spi, indio_dev);

	dev = iio_priv(indio_dev);
	dev->regmap = regmap;

	indio_dev->dev.parent = &spi->dev;
	indio_dev->info = &ada4250_info;
	indio_dev->name = spi->dev.of_node->name;
	indio_dev->channels = ada4250_channels;
	indio_dev->num_channels = ARRAY_SIZE(ada4250_channels);

	ret = iio_device_register(indio_dev);

	dev_info(&spi->dev, "%s probed\n", indio_dev->name);

	return ret;
}

static int ada4250_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);

	iio_device_unregister(indio_dev);

	return 0;
}

static const struct spi_device_id ada4250_id[] = {
	{ "ada4250", ADA4250 },
	{}
};
MODULE_DEVICE_TABLE(spi, ada4250_id);

static const struct of_device_id ada4250_of_match[] = {
	{ .compatible = "adi,ada4250" },
	{},
};
MODULE_DEVICE_TABLE(of, ada4250_of_match);

static struct spi_driver ada4250_driver = {
	.driver = {
			.name = "ada4250",
			.of_match_table = of_match_ptr(ada4250_of_match),
		},
	.probe = ada4250_probe,
	.remove = ada4250_remove,
	.id_table = ada4250_id,
};
module_spi_driver(ada4250_driver);

MODULE_AUTHOR("Antoniu Miclaus <antoniu.miclaus@analog.com");
MODULE_DESCRIPTION("Analog Devices ADA4250");
MODULE_LICENSE("GPL v2");
