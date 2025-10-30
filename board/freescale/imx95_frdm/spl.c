// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 NXP
 */

#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/arch/mu.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/sections.h>
#include <hang.h>
#include <init.h>
#include <spl.h>
#include <asm/mach-imx/ele_api.h>
#include <asm/gpio.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	switch (boot_dev_spl) {
	case SD1_BOOT:
	case MMC1_BOOT:
		return BOOT_DEVICE_MMC1;
	case SD2_BOOT:
	case MMC2_BOOT:
		return BOOT_DEVICE_MMC2;
	case USB_BOOT:
		return BOOT_DEVICE_BOARD;
	case QSPI_BOOT:
		return BOOT_DEVICE_SPI;
	default:
		return BOOT_DEVICE_NONE;
	}
}

void spl_board_init(void)
{
	int ret;

	puts("Normal Boot\n");

	ret = ele_start_rng();
	if (ret)
		printf("Fail to start RNG: %d\n", ret);
}

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

#ifdef CONFIG_SPL_RECOVER_DATA_SECTION
	if (IS_ENABLED(CONFIG_SPL_BUILD))
		spl_save_restore_data();
#endif

	timer_init();

	/* Need dm_init() to run before any SCMI calls can be made. */
	spl_early_init();

	/* Need enable SCMI drivers and ELE driver before enabling console */
	ret = imx9_probe_mu();
	if (ret)
		hang(); /* if MU not probed, nothing can output, just hang here */

	arch_cpu_init();

	board_early_init_f();

	preloader_console_init();

	debug("SOC: 0x%x\n", gd->arch.soc_rev);
	debug("LC: 0x%x\n", gd->arch.lifecycle);

	get_reset_reason(true, false);

	disable_smmuv3();

	/* Will set ARM freq to max rate */
	clock_init_late();

	board_init_r(NULL, 0);
}

#ifdef CONFIG_ANDROID_SUPPORT
int board_get_emmc_id(void)
{
	return 0;
}
#endif
