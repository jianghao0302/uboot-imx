// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <beeper.h>
#include <log.h>
#include <malloc.h>
#include <asm/gpio.h>


struct beeper_gpio_priv {
	struct gpio_desc gpio;
};

static int gpio_beeper_set_state(struct udevice *dev, enum beeper_state_t state)
{
	struct beeper_gpio_priv *priv = dev_get_priv(dev);
	int ret;

	if (!dm_gpio_is_valid(&priv->gpio))
		return -EREMOTEIO;
	switch (state) {
	case BEEPER_STATUS_OFF:
	case BEEPER_STATUS_ON:
		break;
	case BEEPER_STATUS_TOGGLE:
		ret = dm_gpio_get_value(&priv->gpio);
		if (ret < 0)
			return ret;
		state = !ret;
		break;
	default:
		return -ENOSYS;
	}

	return dm_gpio_set_value(&priv->gpio, state);
}

static enum beeper_state_t gpio_beeper_get_state(struct udevice *dev)
{
	struct beeper_gpio_priv *priv = dev_get_priv(dev);
	int ret;

	if (!dm_gpio_is_valid(&priv->gpio))
		return -EREMOTEIO;
	ret = dm_gpio_get_value(&priv->gpio);
	if (ret < 0)
		return ret;

	return ret ? BEEPER_STATUS_ON : BEEPER_STATUS_OFF;
}

static int beeper_gpio_probe(struct udevice *dev)
{
	struct beeper_gpio_priv *priv = dev_get_priv(dev);

	return gpio_request_by_name(dev, "gpios", 0, &priv->gpio, GPIOD_IS_OUT);
}

static int beeper_gpio_remove(struct udevice *dev)
{
	/*
	 * The GPIO driver may have already been removed. We will need to
	 * address this more generally.
	 */
#ifndef CONFIG_SANDBOX
	struct beeper_gpio_priv *priv = dev_get_priv(dev);

	if (dm_gpio_is_valid(&priv->gpio))
		dm_gpio_free(dev, &priv->gpio);
#endif

	return 0;
}

static int beeper_gpio_bind(struct udevice *parent)
{
	return beeper_bind_generic(parent, "gpio_beeper");
}

static const struct beeper_ops gpio_beeper_ops = {
	.set_state	= gpio_beeper_set_state,
	.get_state	= gpio_beeper_get_state,
};

U_BOOT_DRIVER(beeper_gpio) = {
	.name	= "gpio_beeper",
	.id	= UCLASS_BEEPER,
	.ops	= &gpio_beeper_ops,
	.priv_auto	= sizeof(struct beeper_gpio_priv),
	.probe	= beeper_gpio_probe,
	.remove	= beeper_gpio_remove,
};

static const struct udevice_id beeper_gpio_ids[] = {
	{ .compatible = "gpio-beeper" },
	{ }
};

U_BOOT_DRIVER(beeper_gpio_wrap) = {
	.name	= "gpio_beeper_wrap",
	.id	= UCLASS_NOP,
	.of_match = beeper_gpio_ids,
	.bind	= beeper_gpio_bind,
};
