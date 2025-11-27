// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <beeper.h>
#include <dm/uclass-internal.h>

#define LED_TOGGLE LEDST_COUNT

static const char *const state_label[] = {
	[BEEPER_STATUS_OFF]	= "off",
	[BEEPER_STATUS_ON]	= "on",
	[BEEPER_STATUS_TOGGLE]	= "toggle",
};

enum beeper_state_t get_beeper_cmd(char *var)
{
	int i;

	for (i = 0; i < BEEPER_STATUS_COUNT; i++) {
		if (!strncmp(var, state_label[i], strlen(var)))
			return i;
	}

	return -1;
}

static int show_beeper_state(struct udevice *dev)
{
	int ret;

	ret = beeper_get_state(dev);
	if (ret >= BEEPER_STATUS_COUNT)
		ret = -EINVAL;
	if (ret >= 0)
		printf("%s\n", state_label[ret]);

	return ret;
}

static int list_beepers(void)
{
	struct udevice *dev;
	int ret;

	for (uclass_find_first_device(UCLASS_BEEPER, &dev);
	     dev;
	     uclass_find_next_device(&dev)) {
		struct beeper_uc_plat *plat = dev_get_uclass_plat(dev);

		if (!plat->label)
			continue;
		printf("%-15s ", plat->label);
		if (device_active(dev)) {
			ret = show_beeper_state(dev);
			if (ret < 0)
				printf("Error %d\n", ret);
		} else {
			printf("<inactive>\n");
		}
	}

	return 0;
}

int do_beeper(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	enum beeper_state_t cmd;
	const char *beeper_label;
	struct udevice *dev;
	int ret;

	/* Validate arguments */
	if (argc < 2)
		return CMD_RET_USAGE;
	beeper_label = argv[1];
	if (strncmp(beeper_label, "list", 4) == 0)
		return list_beepers();

	cmd = argc > 2 ? get_beeper_cmd(argv[2]) : BEEPER_STATUS_COUNT;
	ret = beeper_get_by_label(beeper_label, &dev);
	if (ret) {
		printf("Beeper '%s' not found (err=%d)\n", beeper_label, ret);
		return CMD_RET_FAILURE;
	}
	switch (cmd) {
	case BEEPER_STATUS_OFF:
	case BEEPER_STATUS_ON:
	case BEEPER_STATUS_TOGGLE:
		ret = beeper_set_state(dev, cmd);
		break;
	case BEEPER_STATUS_COUNT:
		printf("Beeper '%s': ", beeper_label);
		ret = show_beeper_state(dev);
		break;
	}
	if (ret < 0) {
		printf("Beeper '%s' operation failed (err=%d)\n", beeper_label, ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

U_BOOT_CMD(
	beeper, 4, 1, do_beeper,
	"manage beepers",
	"<beeper_label> on|off|toggle \tChange beeper state\n"
	"beeper <beeper_label>\tGet beeper state\n"
	"beeper list\t\tshow a list of beepers"
);
