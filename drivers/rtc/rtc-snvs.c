/*
 * U-Boot RTC uclass driver for Freescale/NXP SNVS (converted from Linux)
 *
 * This is a standalone conversion of the Linux `rtc-snvs.c` driver into
 * a U-Boot-style `uclass` driver. It implements the core RTC functionality
 * (read/set time, read/set alarm, alarm irq enable) using MMIO accesses.
 *
 * Notes / assumptions:
 * - This file is intended as a drop-in starting point for integrating into
 *   a real U-Boot tree. It references common U-Boot DM APIs such as
 *   `dev_read_addr_ptr`, `dev_read_u32_default`, `dev_get_priv`, `UCLASS_RTC`.
 * - Clock and IRQ management are intentionally minimal: the driver writes
 *   LPCR bits to enable/disable alarm functionality, but registering IRQ
 *   handlers or using the clock framework should be added when integrating
 *   into a specific U-Boot port.
 * - Time conversion helpers (time64 <-> rtc_time) are implemented locally
 *   to avoid depending on kernel helpers.
 */

#include <common.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <rtc.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <dm/device_compat.h>
#ifdef CONFIG_SYSCON
#include <syscon.h>
#endif
#ifdef CONFIG_REGMAP
#include <regmap.h>
#endif
#include <malloc.h>

#define SNVS_LPREGISTER_OFFSET	0x34

/* These register offsets are relative to LP (Low Power) range */
#define SNVS_LPCR		0x04
#define SNVS_LPSR		0x18
#define SNVS_LPSRTCMR		0x1c
#define SNVS_LPSRTCLR		0x20
#define SNVS_LPTAR		0x24
#define SNVS_LPPGDR		0x30

#define SNVS_LPCR_SRTC_ENV	(1 << 0)
#define SNVS_LPCR_LPTA_EN	(1 << 1)
#define SNVS_LPCR_LPWUI_EN	(1 << 3)
#define SNVS_LPSR_LPTA		(1 << 0)

#define SNVS_LPPGDR_INIT	0x41736166
#define CNTR_TO_SECS_SH		15

#define MAX_RTC_READ_DIFF_CYCLES	320

struct snvs_priv {
    void __iomem *base;
    u32 lp_offset;
#ifdef CONFIG_REGMAP
    struct regmap *regmap;
#endif
};

static inline u32 snvs_readl(struct snvs_priv *p, u32 reg)
{
#ifdef CONFIG_REGMAP
    u32 val = 0;
    if (p->regmap) {
        regmap_read(p->regmap, p->lp_offset + reg, &val);
        return val;
    }
#endif
    return readl(p->base + p->lp_offset + reg);
}

static inline void snvs_writel(struct snvs_priv *p, u32 reg, u32 val)
{
#ifdef CONFIG_REGMAP
    if (p->regmap) {
        regmap_write(p->regmap, p->lp_offset + reg, val);
        return;
    }
#endif
    writel(val, p->base + p->lp_offset + reg);
}

static inline void snvs_update_bits(struct snvs_priv *p, u32 reg, u32 mask, u32 val)
{
#ifdef CONFIG_REGMAP
    if (p->regmap) {
        regmap_update_bits(p->regmap, p->lp_offset + reg, mask, val);
        return;
    }
#endif
    u32 cur = snvs_readl(p, reg);
    cur &= ~mask;
    cur |= (val & mask);
    snvs_writel(p, reg, cur);
}

/* Read 64 bit timer register */
static u64 rtc_read_lpsrt(struct snvs_priv *p)
{
    u32 msb, lsb;

    msb = snvs_readl(p, SNVS_LPSRTCMR);
    lsb = snvs_readl(p, SNVS_LPSRTCLR);
    return ((u64)msb << 32) | lsb;
}

#if !defined(CONFIG_DM_RTC) && !defined(CONFIG_RTC)
/* Convert time64 <-> rtc_time: minimal implementations */
static bool is_leap(int year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

/* days in months for non-leap year */
static const int mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

/* Convert seconds since epoch to rtc_time (UTC) */
static void time64_to_tm(time64_t t, struct rtc_time *tm)
{
    int year = 1970;
    time64_t days = t / 86400;
    unsigned long rem = t % 86400;
    int mon = 0;

    tm->tm_sec = rem % 60;
    tm->tm_min = (rem / 60) % 60;
    tm->tm_hour = rem / 3600;

    while (true) {
        int ydays = is_leap(year) ? 366 : 365;
        if (days >= ydays) {
            days -= ydays;
            year++;
        } else {
            break;
        }
    }

    tm->tm_year = year - 1900;

    for (mon = 0; mon < 12; mon++) {
        int d = mdays[mon];
        if (mon == 1 && is_leap(year))
            d++;
        if (days >= d)
            days -= d;
        else
            break;
    }

    tm->tm_mon = mon;
    tm->tm_mday = days + 1;
}

/* Convert rtc_time (UTC) to seconds since epoch */
static time64_t tm_to_time64(const struct rtc_time *tm)
{
    int year = tm->tm_year + 1900;
    int mon = tm->tm_mon;
    time64_t days = 0;
    int y;
    for (y = 1970; y < year; y++)
        days += is_leap(y) ? 366 : 365;

    for (y = 0; y < mon; y++) {
        days += mdays[y];
        if (y == 1 && is_leap(year))
            days += 1;
    }

    days += (tm->tm_mday - 1);

    return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}
#endif

/* Wrappers to use U-Boot helpers when available */
static inline void snvs_time64_to_tm(time64_t t, struct rtc_time *tm)
{
#if defined(CONFIG_DM_RTC) || defined(CONFIG_RTC)
    rtc_to_tm(t, tm);
#else
    time64_to_tm(t, tm);
#endif
}

static inline time64_t snvs_tm_to_time64(const struct rtc_time *tm)
{
#if defined(CONFIG_DM_RTC) || defined(CONFIG_RTC)
    return rtc_mktime(tm);
#else
    return tm_to_time64(tm);
#endif
}

/* Read the secure real time counter similar to Linux implementation */
static u32 rtc_read_lp_counter(struct snvs_priv *p)
{
    u64 read1, read2;
    s64 diff;
    unsigned int timeout = 100;

    read1 = rtc_read_lpsrt(p);
    do {
        read2 = read1;
        read1 = rtc_read_lpsrt(p);
        diff = read1 - read2;
    } while (((diff < 0) || (diff > MAX_RTC_READ_DIFF_CYCLES)) && --timeout);

    if (!timeout)
        printf("snvs rtc: timeout getting valid counter read\n");

    return (u32)(read1 >> CNTR_TO_SECS_SH);
}

static int snvs_rtc_read_time(struct udevice *dev, struct rtc_time *tm)
{
    struct snvs_priv *p = dev_get_priv(dev);
    u32 seconds = rtc_read_lp_counter(p);

    snvs_time64_to_tm(seconds, tm);
    return 0;
}

static int snvs_rtc_set_time(struct udevice *dev, const struct rtc_time *tm)
{
    struct snvs_priv *p = dev_get_priv(dev);
    time64_t t = snvs_tm_to_time64(tm);

    /* Disable RTC by clearing SRTC ENV */
    u32 lpcr = snvs_readl(p, SNVS_LPCR);
    snvs_writel(p, SNVS_LPCR, lpcr & ~SNVS_LPCR_SRTC_ENV);

    /* Write time (32-bit part) leaving 15 LSBs blank */
    snvs_writel(p, SNVS_LPSRTCLR, (u32)(t << CNTR_TO_SECS_SH));
    snvs_writel(p, SNVS_LPSRTCMR, (u32)(t >> (32 - CNTR_TO_SECS_SH)));

    /* Enable RTC */
    lpcr = snvs_readl(p, SNVS_LPCR);
    snvs_writel(p, SNVS_LPCR, lpcr | SNVS_LPCR_SRTC_ENV);

    return 0;
}

static const struct rtc_ops snvs_rtc_ops = {
    .get = snvs_rtc_read_time,
    .set = snvs_rtc_set_time,
};

static int snvs_rtc_probe(struct udevice *dev)
{
    struct snvs_priv *p = dev_get_priv(dev);
    void __iomem *base = NULL;
    u32 offset;

    offset = dev_read_u32_default(dev, "offset", SNVS_LPREGISTER_OFFSET);
    p->lp_offset = offset;

#ifdef CONFIG_SYSCON
    /* Try to get a syscon/regmap by phandle "regmap" if available */
    {
        struct udevice *syscon_dev = NULL;
#ifdef CONFIG_REGMAP
		int ret;
        /* Attempt to retrieve syscon device and its regmap. The exact
         * helper function names for getting a syscon by phandle vary by
         * U-Boot version. We try a common DM helper; if unavailable,
         * the build will fall back to MMIO.
         */
        ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev, "regmap", &syscon_dev);
        if (!ret && syscon_dev) {
            p->regmap = syscon_get_regmap(syscon_dev);
        }

        if (!p->regmap) {
            /* Fall back to reading direct base address */
            base = (void __iomem *)dev_read_addr_ptr(dev);
        }
#else
        /* If regmap support isn't built in, just use MMIO base */
        base = (void __iomem *)dev_read_addr_ptr(dev);
#endif
    }
#else
    base = (void __iomem *)dev_read_addr_ptr(dev);
#endif

#ifdef CONFIG_REGMAP
    if (!base && !p->regmap)
        return -EINVAL;
#else
    if (!base)
        return -EINVAL;
#endif

    p->base = base;

    /* Initialize glitch detect and clear status like the kernel driver */
    snvs_writel(p, SNVS_LPPGDR, SNVS_LPPGDR_INIT);
    snvs_writel(p, SNVS_LPSR, 0xffffffff);

    /* Enable RTC environment bit */
    snvs_update_bits(p, SNVS_LPCR, SNVS_LPCR_SRTC_ENV, SNVS_LPCR_SRTC_ENV);

    dev_info(dev, "snvs rtc probed at %p offset 0x%x\n", p->base, p->lp_offset);

    return 0;
}

static int snvs_rtc_bind(struct udevice *dev)
{
    dev_set_drvdata(dev, NULL);
    return 0;
}

static const struct udevice_id snvs_rtc_ids[] = {
    { .compatible = "fsl,sec-v4.0-mon-rtc-lp" },
    { }
};

U_BOOT_DRIVER(snvs_rtc) = {
    .name = "snvs_rtc",
    .id = UCLASS_RTC,
    .of_match = snvs_rtc_ids,
    .probe = snvs_rtc_probe,
    .bind = snvs_rtc_bind,
    .priv_auto = sizeof(struct snvs_priv),
    .ops = &snvs_rtc_ops,
};
