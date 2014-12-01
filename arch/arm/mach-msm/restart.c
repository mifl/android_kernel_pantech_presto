/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/pm8xxx/misc.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/irqs.h>
#ifdef CONFIG_PANTECH // FEATURE_SKY_PWR_ONOFF_REASON_CNT
#include "sky_sys_reset.h"
#endif /* CONFIG_PANTECH */
#ifdef CONFIG_MACH_MSM8X60_PRESTO  //ls4 p13156 lks because of oled reset
#include <mach/gpio.h>
#endif /* CONFIG_MACH_MSM8X60_PRESTO */
#include <mach/scm.h>
#include "msm_watchdog.h"
#include "timer.h"

#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C

#define PSHOLD_CTL_SU (MSM_TLMM_BASE + 0x820)

#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
#define RESTART_REASON_ADDR_IN_HERE 0x65C
#else /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
#define RESTART_REASON_ADDR 0x65C
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
#define DLOAD_MODE_ADDR     0x0

#define SCM_IO_DISABLE_PMIC_ARBITER	1

static int restart_mode;
void *restart_reason;

int pmic_reset_irq;
static void __iomem *msm_tmr0_base;
// paiksun...
#ifdef CONFIG_PANTECH
#define NORMAL_RESET_MAGIC_NUM 0xbaabcddc
#endif /* CONFIG_PANTECH */

#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
extern void pantech_errlog_display_put_log(const char *log, int size);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */

#ifdef CONFIG_MSM_DLOAD_MODE
static int in_panic;
static void *dload_mode_addr;

/* Download mode master kill-switch */
static int dload_set(const char *val, struct kernel_param *kp);
static int download_mode = 1;
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);

static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
    printk(KERN_ERR "%s in_panic=1 \n",__func__);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

#ifdef CONFIG_PANTECH
#define set_dload_mode(x) do {} while (0)
#else /* CONFIG_PANTECH */
static void set_dload_mode(int on)
{
	if (dload_mode_addr) {
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
		mb();
	}
}
#endif /* CONFIG_PANTECH */

static int dload_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

#ifndef CONFIG_PANTECH_ERR_CRASH_LOGGING
	set_dload_mode(download_mode);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */

	return 0;
}
#else
#define set_dload_mode(x) do {} while (0)
#endif

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);

static void __msm_power_off(int lower_pshold)
{
#ifdef CONFIG_SKY_CHARGING  // p14682 kobj 110620 add ef33s new source
#if 1
    printk(KERN_ERR "msm_power_off ioremap_nocache\n");
    writel(0x00, restart_reason);
    printk(KERN_ERR "msm_power_off writel\n");
    writel(0x00, restart_reason+4);
#endif
#endif /* CONFIG_SKY_CHARGING */
#ifdef CONFIG_MACH_MSM8X60_PRESTO  //ls4 p13156 lks because of oled reset
    gpio_set_value(157 , 0);
#endif /* CONFIG_MACH_MSM8X60_PRESTO */
	printk(KERN_CRIT "Powering off the SoC\n");
#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(0);
#endif
	pm8xxx_reset_pwr_off(0);

	if (lower_pshold) {
		__raw_writel(0, PSHOLD_CTL_SU);
		mdelay(10000);
		printk(KERN_ERR "Powering off has failed\n");
	}
	return;
}

#ifdef CONFIG_PANTECH_PWR_ONOFF_REASON_CNT
int sky_reset_reason=SYS_RESET_REASON_UNKNOWN;
#endif /* CONFIG_PANTECH_PWR_ONOFF_REASON_CNT */

static void msm_power_off(void)
{
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
    printk(KERN_ERR "%s \n",__func__);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
	/* MSM initiated power off, lower ps_hold */
	__msm_power_off(1);
}

static void cpu_power_off(void *data)
{
	int rc;

	pr_err("PMIC Initiated shutdown %s cpu=%d\n", __func__,
						smp_processor_id());
	if (smp_processor_id() == 0) {
		/*
		 * PMIC initiated power off, do not lower ps_hold, pmic will
		 * shut msm down
		 */
		__msm_power_off(0);

		pet_watchdog();
		pr_err("Calling scm to disable arbiter\n");
		/* call secure manager to disable arbiter and never return */
		rc = scm_call_atomic1(SCM_SVC_PWR,
						SCM_IO_DISABLE_PMIC_ARBITER, 1);

		pr_err("SCM returned even when asked to busy loop rc=%d\n", rc);
		pr_err("waiting on pmic to shut msm down\n");
	}

	preempt_disable();
	while (1)
		;
}

static irqreturn_t resout_irq_handler(int irq, void *dev_id)
{
	pr_warn("%s PMIC Initiated shutdown\n", __func__);
	oops_in_progress = 1;
	smp_call_function_many(cpu_online_mask, cpu_power_off, NULL, 0);
	if (smp_processor_id() == 0)
		cpu_power_off(NULL);
	preempt_disable();
	while (1)
		;
	return IRQ_HANDLED;
}

void arch_reset(char mode, const char *cmd)
{
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
    char dispbuf[512];
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */

#ifdef CONFIG_MSM_DLOAD_MODE
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
    printk(KERN_ERR "%s \n",__func__);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */

	/* This looks like a normal reboot at this point. */
	set_dload_mode(0);

#ifndef CONFIG_PANTECH_ERR_CRASH_LOGGING
    /* Write download mode flags if we're panic'ing */
    if (in_panic || restart_mode == RESTART_DLOAD)
        set_dload_mode(1);
#endif /* CONFIG_SW_RESET */

#ifndef CONFIG_PANTECH /* NOT Used in PANTECH */
	/* Write download mode flags if we're panic'ing */
	set_dload_mode(in_panic);

	/* Write download mode flags if restart_mode says so */
	if (restart_mode == RESTART_DLOAD)
		set_dload_mode(1);

	/* Kill download mode if master-kill switch is set */
	if (!download_mode)
		set_dload_mode(0);
#endif /* CONFIG_PANTECH // Not Used in PANTECH */
#ifndef CONFIG_PANTECH /* NOT Used in PANTECH */
    if (!in_panic && restart_mode == RESTART_NORMAL)
#endif /* CONFIG_PANTECH // NOT Used in PANTECH */
		set_dload_mode(0);
#endif

	printk(KERN_NOTICE "Going down for restart now\n");

	pm8xxx_reset_pwr_off(1);
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
    printk(KERN_ERR "%s cmd: %s \n",__func__,cmd);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */

	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			__raw_writel(0x77665500, restart_reason);
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
            printk(KERN_ERR "Powering off bootloader\n");
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
		} else if (!strncmp(cmd, "recovery", 8)) {
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
            printk(KERN_ERR "Powering off recovery\n");
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
			__raw_writel(0x77665502, restart_reason);
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
        } else if (!strncmp(cmd, "androidpanic", 12)) {
            printk(KERN_ERR "allydrop android panic!!!!in_panic:%x\n",in_panic);
#ifdef CONFIG_PANTECH_PWR_ONOFF_REASON_CNT
            sky_reset_reason=SYS_RESET_REASON_ANDROID;
            __raw_writel(sky_reset_reason, restart_reason);
            __raw_writel(NORMAL_RESET_MAGIC_NUM, restart_reason+4);
#endif /* CONFIG_PANTECH_PWR_ONOFF_REASON_CNT */
            strcpy(dispbuf,"\n\n     [ANDROID FRAMEWORK ERROR]\n\n");
            strcat(dispbuf,"\n\n     Rebooting cause of Crash\n\n");
            strcat(dispbuf,"\n\n     Press Power key for reboot\n\n");
            strcat(dispbuf,"\n\n     Wait a minute for saving logs until rebooting \n\n");  
            pantech_errlog_display_put_log(dispbuf, strlen(dispbuf));
            panic("android framework error\n"); 
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			code = simple_strtoul(cmd + 4, NULL, 16) & 0xff;
			__raw_writel(0x6f656d00 | code, restart_reason);
		} else {
#ifdef CONFIG_PANTECH_PWR_ONOFF_REASON_CNT
            if(in_panic){
                printk(KERN_ERR "%s in_panic sky_reset_reason :%x \n",__func__,sky_reset_reason);
                __raw_writel(sky_reset_reason, restart_reason);
            }
            else
#endif /* CONFIG_PANTECH_PWR_ONOFF_REASON_CNT */
            {
            printk(KERN_ERR "Powering off Default\n");
			__raw_writel(0x77665501, restart_reason);
			}
		}
	}
#ifdef CONFIG_PANTECH_WDOG_WORKAROUND
    else {
#ifdef CONFIG_PANTECH_PWR_ONOFF_REASON_CNT
        if(in_panic) {
            printk(KERN_ERR "%s in_panic sky_reset_reason :%x \n",__func__,sky_reset_reason);
            __raw_writel(sky_reset_reason, restart_reason);
        } else
#endif /* CONFIG_PANTECH_PWR_ONOFF_REASON_CNT */
        {
            __raw_writel(0x77665501, restart_reason);
        }
    }
    writel(NORMAL_RESET_MAGIC_NUM, restart_reason+4);
#endif /* CONFIG_PANTECH_WDOG_WORKAROUND */

	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	if (!(machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()
#ifdef CONFIG_MACH_MSM8X60_PRESTO
	      || machine_is_msm8x60_presto()
#endif
	)) {
		mb();
		__raw_writel(0, PSHOLD_CTL_SU); /* Actually reset the chip */
		mdelay(5000);
		pr_notice("PS_HOLD didn't work, falling back to watchdog\n");
	}

#ifdef CONFIG_MACH_MSM8X60_PRESTO   //ls4 p13156 lks because of oled reset 
// LCD 한번 리셋해 주기 위해 리셋 안 해주면 reset 후 화면이 깨어지는 경우가 있음.
    gpio_set_value(157 , 0);
#endif /* CONFIG_MACH_MSM8X60_PRESTO */
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5*0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);

	mdelay(10000);
	printk(KERN_ERR "Restarting has failed\n");
}

static int __init msm_restart_init(void)
{
	int rc;

#ifdef CONFIG_MSM_DLOAD_MODE
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	dload_mode_addr = MSM_IMEM_BASE + DLOAD_MODE_ADDR;

#ifndef CONFIG_PANTECH_ERR_CRASH_LOGGING
	/* Reset detection is switched on below.*/
	set_dload_mode(1);
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
#endif
	msm_tmr0_base = msm_timer_get_timer0_base();
#ifdef CONFIG_PANTECH_ERR_CRASH_LOGGING
	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR_IN_HERE;
#else /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR;
#endif /* CONFIG_PANTECH_ERR_CRASH_LOGGING */
	pm_power_off = msm_power_off;

	if (pmic_reset_irq != 0) {
		rc = request_any_context_irq(pmic_reset_irq,
					resout_irq_handler, IRQF_TRIGGER_HIGH,
					"restart_from_pmic", NULL);
		if (rc < 0)
			pr_err("pmic restart irq fail rc = %d\n", rc);
	} else {
		pr_warn("no pmic restart interrupt specified\n");
	}

	return 0;
}

late_initcall(msm_restart_init);
