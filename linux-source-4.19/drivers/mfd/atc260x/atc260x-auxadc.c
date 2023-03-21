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
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/suspend.h>
#include <linux/kobject.h>
#include <linux/timekeeping.h>
#include <linux/mfd/core.h>
#include <linux/mfd/atc260x/atc260x.h>

static struct atc260x_dev *pmic = NULL;

static ssize_t adc_show(struct kobject *kobj, struct kobj_attribute *attr,
                        char *buffer)
{
    int reg, mult, shift, offset, ret;
    ktime_t timestamp;
    const char *unit;
    s64 usec, val;
    
    if (strcmp(attr->attr.name, "adc0") == 0)
    {
    	reg    = ATC2603C_PMU_AUXADC0;
    	mult   = 1;
    	shift  = 0;
    	offset = 0;
    	unit   = "/1024";
    }
    else if (strcmp(attr->attr.name, "adc1") == 0)
    {
    	reg    = ATC2603C_PMU_AUXADC1;
    	mult   = 1;
    	shift  = 0;
    	offset = 0;
    	unit   = "/1024";
    }
    else if (strcmp(attr->attr.name, "adc2") == 0)
    {
    	reg    = ATC2603C_PMU_AUXADC2;
    	mult   = 1;
    	shift  = 0;
    	offset = 0;
    	unit   = "/1024";
    }
    else if (strcmp(attr->attr.name, "ictemp") == 0)
    {
    	reg    = ATC2603C_PMU_ICTEMPADC;
    	mult   = 399155;
    	shift  = 11;
    	offset = -44899;
    	unit   = "mCelsius";
    }
    else if (strcmp(attr->attr.name, "batv") == 0)
    {
    	reg    = ATC2603C_PMU_BATVADC;
    	mult   = 375;
    	shift  = 6;
    	offset = 0;
    	unit   = "mV";
    }
    else if (strcmp(attr->attr.name, "bati") == 0)
    {
    	reg    = ATC2603C_PMU_BATIADC;
    	mult   = 1500;
    	shift  = 10;
    	offset = 0;
    	unit   = "mA";
    }
    else if (strcmp(attr->attr.name, "vbusv") == 0)
    {
    	reg    = ATC2603C_PMU_VBUSVADC;
    	mult   = 1875;
    	shift  = 8;
    	offset = 0;
    	unit   = "mV";
    }
    else if (strcmp(attr->attr.name, "vbusi") == 0)
    {
    	reg    = ATC2603C_PMU_VBUSIADC;
    	mult   = 1500;
    	shift  = 10;
    	offset = 0;
    	unit   = "mA";
    }
    else if (strcmp(attr->attr.name, "syspwrv") == 0)
    {
    	reg    = ATC2603C_PMU_SYSPWRADC;
    	mult   = 1875;
    	shift  = 8;
    	offset = 0;
    	unit   = "mV";
    }
    else if (strcmp(attr->attr.name, "wallv") == 0)
    {   
        reg    = ATC2603A_PMU_WALLVADC;
    	mult   = 1875;
    	shift  = 8;
    	offset = 0;
    	unit   = "mV";
    }
    else if (strcmp(attr->attr.name, "walli") == 0)
    {
        reg    = ATC2603A_PMU_WALLIADC;
    	mult   = 1500;
    	shift  = 10;
    	offset = 0;
    	unit   = "mA";
    }
    else if (strcmp(attr->attr.name, "chgi") == 0)
    {
        reg    = ATC2603A_PMU_CHGIADC;
    	mult   = 2000;
    	shift  = 10;
    	offset = 0;
    	unit   = "mA";
    }
    else {
    	return -EINVAL;
    }
    
    timestamp = ktime_get();
    ret = atc260x_reg_read(pmic, reg);
    
    if (ret < 0) {
    	return ret;
    }
    
    usec = ktime_to_us(timestamp);
    
    val = (s64)(ret & 0x3FF);
    val = ((val * mult) >> shift) + offset;
    
    return sprintf(buffer, "%lld %lld %s\n", usec, val, unit);
}

static ssize_t adc_store(struct kobject *kobj, struct kobj_attribute *attr,
                         const char *buffer, size_t count)
{
    return count;
}

static struct kobj_attribute adc0_attr = 
    __ATTR(adc0, 0664, adc_show, adc_store);
    
static struct kobj_attribute adc1_attr =
    __ATTR(adc1, 0664, adc_show, adc_store);
    
static struct kobj_attribute adc2_attr =
    __ATTR(adc2, 0664, adc_show, adc_store);
    
static struct kobj_attribute ictemp_attr =
    __ATTR(ictemp, 0664, adc_show, adc_store);

static struct kobj_attribute batv_attr =
    __ATTR(batv, 0664, adc_show, adc_store);

static struct kobj_attribute bati_attr =
    __ATTR(bati, 0664, adc_show, adc_store);

static struct kobj_attribute vbusv_attr =
    __ATTR(vbusv, 0664, adc_show, adc_store);

static struct kobj_attribute vbusi_attr =
    __ATTR(vbusi, 0664, adc_show, adc_store);

static struct kobj_attribute syspwrv_attr =
    __ATTR(syspwrv, 0664, adc_show, adc_store);

static struct kobj_attribute wallv_attr =
    __ATTR(wallv, 0664, adc_show, adc_store);

static struct kobj_attribute walli_attr =
    __ATTR(walli, 0664, adc_show, adc_store);

static struct kobj_attribute chgi_attr =
    __ATTR(chgi, 0664, adc_show, adc_store);

static struct attribute *adc_attrs[] = {
    &adc0_attr.attr,
    &adc1_attr.attr,
    &adc2_attr.attr,
    &ictemp_attr.attr,
    &batv_attr.attr,
    &bati_attr.attr,
    &vbusv_attr.attr,
    &vbusi_attr.attr,
    &syspwrv_attr.attr,
    &wallv_attr.attr,
    &walli_attr.attr,
    &chgi_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = adc_attrs,
};

static struct kobject *adc_kobj = NULL;

static int atc2603c_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret, val;
	
	pmic = dev_get_drvdata(dev->parent);
	
	if (!pmic) {
		return -EINVAL;
	}
	
	ret = atc260x_reg_read(pmic, ATC2603C_PMU_AUXADC_CTL1);
    
    if (ret < 0) {
    	return ret;
    }
    
    val = ret & 0xFFFFU;
    
    // sets pmic reference resistor to 20mOhms
    val &= ~BIT(4);
    // disable ADC comp offset trimming (not needed at chip rev. b)
    val &= ~BIT(3);
    // use highest ADC clock
    val |= BIT(0);
    // set ADC input range to 0-3V
    val |= BIT(1);
    // use internal clock for ADC
    val &= ~BIT(11);
    // enable and setup coulomb meter
    val &= 0xFC1FU;
    val |= 0x01E0U;
    
    ret = atc260x_reg_write(pmic, ATC2603C_PMU_AUXADC_CTL1, val);
    
    if (ret < 0) {
    	return ret;
    }
    
    // start all adcs
    ret = atc260x_reg_write(pmic, ATC2603C_PMU_AUXADC_CTL0, 0xFFFFU);
    
    if (ret < 0) {
    	return ret;
    }
    
    // create directory auxadc at /sys/kernel/
	adc_kobj = kobject_create_and_add("auxadc", kernel_kobj);
	
	if (!adc_kobj) {
		return -ENOMEM;
	}
	
	ret = sysfs_create_group(adc_kobj, &attr_group);
	
	if(ret)
	{
	    kobject_put(adc_kobj);
	    adc_kobj = NULL;
	}
	
	dev_info(dev, "probe finished\n");
	return ret;
}

static int atc2603c_platform_remove(struct platform_device *pdev)
{
	if (adc_kobj)
	{
		kobject_put(adc_kobj);
		adc_kobj = NULL;
	}
	
    pmic = NULL;
	return 0;
}

static const struct of_device_id atc2603c_auxadc_of_match[] = {
	{.compatible = "actions,atc2603c-auxadc",},
	{}
};
MODULE_DEVICE_TABLE(of, atc2603c_auxadc_of_match);

static struct platform_driver atc2603c_platform_driver = {
	.probe = atc2603c_platform_probe,
	.remove = atc2603c_platform_remove,
	.driver = {
		.name = "atc2603c-auxadc",
		.owner = THIS_MODULE,
		.of_match_table = atc2603c_auxadc_of_match,
	},
};

module_platform_driver(atc2603c_platform_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("ATC2603C auxadc driver");
MODULE_LICENSE("GPL");

