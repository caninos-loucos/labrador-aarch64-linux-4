/*
 * Copyright (c) 2020 LSI-TEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <asm/system_misc.h>
#include <linux/mfd/core.h>
#include <linux/mfd/atc260x/atc260x.h>

#define CANINOS_KEY_POLLING_DELAY_MS (100)

static struct atc260x_dev *pmic = NULL;
static struct input_dev *baseboard_keys_dev = NULL;
static struct delayed_work key_polling_work;
static int userkey_gpio = -1; /* negative gpio number is always invalid */
static bool userkey_active_low = false;
static void atc260x_poweroff(void);

static int caninos_userkey_gpio_get_value(void)
{
	int debounce = 5;
	int value;
	
	if (gpio_is_valid(userkey_gpio)) {
		return -EINVAL;
	}
	
	for (;;)
	{
		value = gpio_get_value(userkey_gpio);
		
		if (value < 0) {
			return -EINVAL;
		}
		else if (value > 0) {
			value = (userkey_active_low) ? 0 : 1;
		}
		else {
			value = (userkey_active_low) ? 1 : 0;
		}
		
		debounce--;
		
		if (debounce > 0) {
			usleep_range(1000, 1500);
		}
		else {
			break;
		}
	}
	return value;
}

static void caninos_key_polling_worker(struct work_struct *work)
{
	const unsigned long delay = msecs_to_jiffies(CANINOS_KEY_POLLING_DELAY_MS);
	int ret;
	
	ret = atc260x_reg_read(pmic, ATC2603C_PMU_SYS_CTL2);
	
	if (ret < 0) {
		goto quit_worker;
	}
	
	/* ONOFF short press */
	if (ret & BIT(14))
	{
		ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, ret);
		
		if (ret >= 0)
		{
			input_report_key(baseboard_keys_dev, KEY_POWER, 1);
			input_sync(baseboard_keys_dev);
			input_report_key(baseboard_keys_dev, KEY_POWER, 0);
			input_sync(baseboard_keys_dev);
		}
	}
	
	/* ONOFF long press */
	if (ret & BIT(13))
	{
		ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, ret);
		
		if (ret >= 0)
		{
			/* do a hard shutdown (never returns!)*/
			atc260x_poweroff();
		}
	}
	
	ret = caninos_userkey_gpio_get_value();
	
	if (ret > 0)
	{
		input_report_key(baseboard_keys_dev, KEY_PROG1, 1);
		input_sync(baseboard_keys_dev);
		input_report_key(baseboard_keys_dev, KEY_PROG1, 0);
		input_sync(baseboard_keys_dev);
	}
	
quit_worker:
	queue_delayed_work(system_long_wq, &key_polling_work, delay);
}

static void caninos_userkey_gpio_probe_and_request(struct device *dev)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	
	userkey_gpio = of_get_named_gpio_flags(np, "userkey-gpios", 0, &flags);
	
	if (gpio_is_valid(userkey_gpio))
	{
		if (devm_gpio_request(dev, userkey_gpio, "userkey_gpio")) {
			userkey_gpio = -1;
		}
		else
		{
			gpio_direction_input(userkey_gpio);
			userkey_active_low = (flags == OF_GPIO_ACTIVE_LOW);
		}
	}
}

static int atc260x_create_input_device(void)
{
	const unsigned long delay = msecs_to_jiffies(CANINOS_KEY_POLLING_DELAY_MS);
	int ret;
	
	ret = atc260x_reg_read(pmic, ATC2603C_PMU_SYS_CTL2);
	
	if (ret < 0) {
		return ret;
	}
	
	/* clear pending key interrupts */
	ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, ret | 0x6000);
	
	if (ret < 0) {
		return ret;
	}
	
	ret = atc260x_reg_read(pmic, ATC2603C_PMU_SYS_CTL2);
	
	if (ret < 0) {
		return ret;
	}
	
	/* enable ONOFF interrupts */
	ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, ret | 0x1000);
	
	if (ret < 0) {
		return ret;
	}
	
	baseboard_keys_dev = input_allocate_device();
	
	if (!baseboard_keys_dev) {
		return -ENOMEM;
	}
	
	/*
	This driver uses the Event Interface (Documentation/input/input.txt)
	A user should read the file /dev/input/eventX in C, Python, ...
	For every key press the following binary structure will be returned:
	
	struct input_event {
		struct timeval time;
		unsigned short type;
		unsigned short code;
		unsigned int value;
	};
	*/
	
	/* power key */
	input_set_capability(baseboard_keys_dev, EV_KEY, KEY_POWER);
	
	/* general purpose user configurable key */
	input_set_capability(baseboard_keys_dev, EV_KEY, KEY_PROG1);
	
	ret = input_register_device(baseboard_keys_dev);
	
	if (ret)
	{
		input_free_device(baseboard_keys_dev);
		baseboard_keys_dev = NULL;
		return ret;
	}
	
	INIT_DELAYED_WORK(&key_polling_work, caninos_key_polling_worker);
	queue_delayed_work(system_long_wq, &key_polling_work, delay);
	return 0;
}

static void atc260x_destroy_input_device(void)
{
	if (baseboard_keys_dev)
	{
		cancel_delayed_work_sync(&key_polling_work);
		input_unregister_device(baseboard_keys_dev);
		baseboard_keys_dev = NULL;
	}
}

static int atc260x_poweroff_setup(void)
{
	// set ATC2603C_PMU_SYS_CTL0 value
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL0, 0x304B);
	
	// set ATC2603C_PMU_SYS_CTL1 value
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL1, 0xF);
	
	// set ATC2603C_PMU_SYS_CTL2 value
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, 0x6680);
	
	// set ATC2603C_PMU_SYS_CTL3 value
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL3, 0x80);
	
	// set ATC2603C_PMU_SYS_CTL5 value
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL5, 0x00);
	
	return 0;
}

static void atc260x_poweroff(void)
{
	int ret;
	
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, 0x6680);
	
	ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL1, 0xE);
	
	if (ret < 0) {
		pr_err("system poweroff failed.\n");
	}
	
	for(;;) { /* must never return */
		cpu_relax();
	}
}

static void atc260x_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	int ret;
	
	atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL2, 0x6680);

	ret = atc260x_reg_write(pmic, ATC2603C_PMU_SYS_CTL0, 0x344B);
	
	if (ret < 0) {
		pr_err("system restart failed.\n");
	}
	
	for(;;) { /* must never return */
		cpu_relax();
	}
}

static int atc2603c_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;
	
	pmic = dev_get_drvdata(dev->parent);
	
	if (!pmic) {
		return -EINVAL;
	}
	
	atc260x_poweroff_setup();
	
	pm_power_off = atc260x_poweroff;
	arm_pm_restart = atc260x_restart;
	
	caninos_userkey_gpio_probe_and_request(dev);
	
	ret = atc260x_create_input_device();
	
	if (ret) {
		dev_err(dev, "could not create baseboard input device.\n");
	}
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static int atc2603c_platform_remove(struct platform_device *pdev)
{
	atc260x_destroy_input_device();
	return 0;
}

static const struct of_device_id atc2603c_poweroff_of_match[] = {
	{.compatible = "actions,atc2603c-poweroff",},
	{}
};
MODULE_DEVICE_TABLE(of, atc2603c_poweroff_of_match);

static struct platform_driver atc2603c_platform_driver = {
	.probe = atc2603c_platform_probe,
	.remove = atc2603c_platform_remove,
	.driver = {
		.name = "atc2603c-poweroff",
		.owner = THIS_MODULE,
		.of_match_table = atc2603c_poweroff_of_match,
	},
};

module_platform_driver(atc2603c_platform_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("ATC2603C poweroff driver");
MODULE_LICENSE("GPL");

