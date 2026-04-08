/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __BEEPER_H__
#define __BEEPER_H__

struct udevice;

enum beeper_state_t {
	BEEPER_STATUS_OFF = 0,
	BEEPER_STATUS_ON = 1,
	BEEPER_STATUS_TOGGLE,
	BEEPER_STATUS_COUNT,
};

/**
 * struct beeper_uc_plat - Platform data the uclass stores about each device
 *
 * @label:	Beeper label
 * @default_state:	Beeper default state
 */
struct beeper_uc_plat {
	const char *label;
	enum beeper_state_t default_state;
};

struct beeper_ops {
	/**
	 * set_state() - set the state of a beeper
	 *
	 * @dev:	BEEPER device to change
	 * @state:	BEEPER state to set
	 * @return 0 if OK, -ve on error
	 */
	int (*set_state)(struct udevice *dev, enum beeper_state_t state);
	/**
	 * beeper_get_state() - get the state of a beeper
	 *
	 * @dev:	BEEPER device to change
	 * @return Beeper state beeper_state_t, or -ve on error
	 */
	enum beeper_state_t (*get_state)(struct udevice *dev);
};

#define beeper_get_ops(dev)	((struct beeper_ops *)(dev)->driver->ops)

/**
 * beeper_get_by_label() - Find an beeper device by label
 *
 * @label:	Beeper label to look up
 * @devp:	Returns the associated device, if found
 * Return: 0 if found, -ENODEV if not found, other -ve on error
 */
int beeper_get_by_label(const char *label, struct udevice **devp);
/**
 * beeper_set_state() - set the state of a beeper
 *
 * @dev:	Beeper device to change
 * @state:	Beeper state to set
 * Return: 0 if OK, -ve on error
 */
int beeper_set_state(struct udevice *dev, enum beeper_state_t state);

/**
 * beeper_get_state() - get the state of a beeper
 *
 * @dev:	Beeper device to change
 * Return: Beeper state beeper_state_t, or -ve on error
 */
enum beeper_state_t beeper_get_state(struct udevice *dev);

/**
 * beeper_set_period() - set the blink period of a beeper
 *
 * @dev:	Beeper device to change
 * @period_ms:	Beeper blink period in milliseconds
 * Return: 0 if OK, -ve on error
 */
int beeper_set_period(struct udevice *dev, int period_ms);
/**
 * beeper_bind_generic() - bind children of parent to given driver
 *
 * @parent:      Top-level beeper device
 * @driver_name: Driver for handling individual child nodes
 */
int beeper_bind_generic(struct udevice *parent, const char *driver_name);

#endif
