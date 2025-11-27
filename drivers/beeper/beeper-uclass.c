// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BEEPER

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <beeper.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <dm/uclass-internal.h>

int beeper_bind_generic(struct udevice *parent, const char *driver_name)
{
	struct udevice *dev;
	ofnode node;
	int ret;

	dev_for_each_subnode(node, parent) {
		ret = device_bind_driver_to_node(parent, driver_name,
						 ofnode_get_name(node),
						 node, &dev);
		if (ret)
			return ret;
	}

	return 0;
}

int beeper_get_by_label(const char *label, struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_BEEPER, &uc);
	if (ret)
		return ret;
	uclass_foreach_dev(dev, uc) {
		struct beeper_uc_plat *uc_plat = dev_get_uclass_plat(dev);
		/* Ignore the top-level BEEPER node */
		if (uc_plat->label && !strcmp(label, uc_plat->label))
			return uclass_get_device_tail(dev, 0, devp);
	}

	return -ENODEV;
}

int beeper_set_state(struct udevice *dev, enum beeper_state_t state)
{
	struct beeper_ops *ops = beeper_get_ops(dev);

	if (!ops->set_state)
		return -ENOSYS;

	return ops->set_state(dev, state);
}

enum beeper_state_t beeper_get_state(struct udevice *dev)
{
	struct beeper_ops *ops = beeper_get_ops(dev);

	if (!ops->get_state)
		return -ENOSYS;

	return ops->get_state(dev);
}

static int beeper_post_bind(struct udevice *dev)
{
	struct beeper_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	const char *default_state;

	if (!uc_plat->label)
		uc_plat->label = dev_read_string(dev, "label");

	if (!uc_plat->label && !dev_read_string(dev, "compatible"))
		uc_plat->label = ofnode_get_name(dev_ofnode(dev));

	uc_plat->default_state = BEEPER_STATUS_COUNT;

	default_state = dev_read_string(dev, "default-state");
	if (!default_state)
		return 0;

	if (!strncmp(default_state, "on", 2))
		uc_plat->default_state = BEEPER_STATUS_ON;
	else if (!strncmp(default_state, "off", 3))
		uc_plat->default_state = BEEPER_STATUS_OFF;
	else
		return 0;

	/*
	 * In case the BEEPER has default-state DT property, trigger
	 * probe() to configure its default state during startup.
	 */
	dev_or_flags(dev, DM_FLAG_PROBE_AFTER_BIND);

	return 0;
}

static int beeper_post_probe(struct udevice *dev)
{
	struct beeper_uc_plat *uc_plat = dev_get_uclass_plat(dev);

	if (uc_plat->default_state == BEEPER_STATUS_ON ||
	    uc_plat->default_state == BEEPER_STATUS_OFF)
		beeper_set_state(dev, uc_plat->default_state);
	return 0;
}

UCLASS_DRIVER(beeper) = {
	.id		= UCLASS_BEEPER,
	.name		= "beeper",
	.per_device_plat_auto	= sizeof(struct beeper_uc_plat),
	.post_bind	= beeper_post_bind,
	.post_probe	= beeper_post_probe,
};
