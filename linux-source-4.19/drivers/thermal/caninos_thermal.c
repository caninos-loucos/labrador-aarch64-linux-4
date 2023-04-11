// SPDX-License-Identifier: GPL-2.0
/*
 * Thermal Sensor Driver for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include "caninos_thermal.h"

static int caninos_get_mode(struct thermal_zone_device *thermal,
                            enum thermal_device_mode *mode)
{
	*mode = THERMAL_DEVICE_ENABLED;
	return 0;
}

static int caninos_get_temp(struct thermal_zone_device *thermal, int *temp)
{
	struct caninos_tmu_sensor *sensor = thermal->devdata;
	struct caninos_tmu_data *data = dev_get_drvdata(sensor->dev);
	
	mutex_lock(&data->lock);
	*temp = sensor->temperature;
	mutex_unlock(&data->lock);
	return 0;
}

static struct thermal_zone_device_ops caninos_tmu_ops = {
	.get_temp = caninos_get_temp,
	.get_mode = caninos_get_mode,
};

static int caninos_tsensdata_to_mcelsius_k5(unsigned int tsens_data)
{
	s32 tmp1, tmp2;

	tsens_data &= 0x7ff;

	/* T = (838.45*5.068/(1024*12/count+7.894)-162 */
	
	tmp1 = 83845*5068;
	tmp2 = 1024*12*100;
	tmp2 = tmp2/(tsens_data+1)+789;
	tmp1 = tmp1/tmp2;
	tmp1 -= 162000;

	return (int) tmp1;
}

static int caninos_tsensdata_to_mcelsius_k7(u32 code)
{
#ifdef CONFIG_ARM64
	s64 temp1, temp2;
	
	code &= 0x3FF;
	
	/* T=5.6534D2/10000 + 1.1513D - 447.1272 */
	
	temp1 = code;
	temp2 = temp1 * temp1;
	temp1 = (11513 * temp1) - 4471272;
	temp2 = (56534 * temp2) / 10000;
	temp2 = (temp1 - temp2) / 10;
	return (int)temp2;
#else
	return THERMAL_TEMP_INVALID;
#endif
}

static int read_raw_thermal_sensor_k5(struct caninos_tmu_data *data)
{
	u32 tmp = readl(data->base);
	int temperature = THERMAL_TEMP_INVALID;
	int retry = 5;

	void __iomem *out = data->base + 4;

	tmp = tmp | 0x1; //enable tsensor
	writel(tmp, data->base);
	
	while(retry > 0)
	{
		usleep_range(1500, 2000);
		tmp = readl(out);
		
		if(tmp & (1 << 25))
		{
			temperature = caninos_tsensdata_to_mcelsius_k5(tmp);
			break;
		}
		retry--;
	}
	tmp = readl(data->base);
	tmp &= ~(1); // disable tsensor
	writel(tmp, data->base);

	return temperature;
}

static int read_raw_thermal_sensor_k7(int id, struct caninos_tmu_data *data)
{
	int temperature = THERMAL_TEMP_INVALID;
	int retry = 5;
	u32 tmp;
	
	tmp = readl(data->base) & 0xCF840FFF;
	
	switch(id)
	{
	case CANINOS_TMU_CPU:
		tmp |= (0x1 << 20) | (0x1 << 19) | (0x20 << 12);
		break;
		
	case CANINOS_TMU_GPU:
		tmp |= (0x1 << 21) | (0x1 << 28) | (0x1 << 19)  | (0x21 << 12);
		break;
		
	case CANINOS_TMU_CORELOGIC:
		tmp |= (0x1 << 22) | (0x1 << 29) | (0x1 << 19) | (0x20 << 12);
		break;
		
	default:
		return temperature;
	}
	
	writel(tmp, data->base);
	
	while(retry > 0)
	{
		usleep_range(1500, 2000);
		tmp = readl(data->base);
		
		if(tmp & (0x1 << 11))
		{
			temperature = caninos_tsensdata_to_mcelsius_k7(tmp);
			break;
		}
		retry--;
	}
	
	tmp = readl(data->base) & 0xCF840FFF;
	writel(tmp, data->base);
	return temperature;
}

static int read_thermal_sensor(int id, struct caninos_tmu_data *data)
{
	int temp_array[5];
	int temperature;
	int aux, i, j;
	
	if ((id < 0) || (id >= data->sensor_count)) {
		return THERMAL_TEMP_INVALID;
	}
	
	/* fill temperature array */
	for (i = 0; i < 4; i++)
	{
		switch(data->model){
			case TMU_MODEL_K5:
				temp_array[i] = read_raw_thermal_sensor_k5(data);
				break;
			case TMU_MODEL_K7:
				temp_array[i] = read_raw_thermal_sensor_k7(id, data);
				break;
		}
		
		if (temp_array[i] == THERMAL_TEMP_INVALID) {
			return THERMAL_TEMP_INVALID;
		}
	}
	
	/* sort temperature array */
	for (i = 0; i < 4; i++)
	{
		for (j = i + 1; j < 5; j++)
		{
			if (temp_array[j] < temp_array[i])
			{
				aux = temp_array[i];
				temp_array[i] = temp_array[j];
				temp_array[j] = aux;
			}
		}
	}
	
	/* discard min & max, then take their average */
	for (temperature = 0, i = 1; i < 4; i++) {
		temperature += temp_array[i];
	}
	return (temperature / 3);
}

static void caninos_thermal_polling_worker(struct work_struct *work)
{
	const unsigned long delay = msecs_to_jiffies(CANINOS_TMU_ACTIVE_INTERVAL);
	struct caninos_tmu_data *data;
	int i;
	int temperature[CANINOS_TMU_SENSOR_COUNT_K7];
		
	data = container_of(work, struct caninos_tmu_data, work.work);

	for (i = 0; i < data->sensor_count; i++) {
		temperature[i] = read_thermal_sensor(i, data);
	}
	
	mutex_lock(&data->lock);
	
	for (i = 0; i < data->sensor_count; i++) {
		data->sensor[i].temperature = temperature[i];
	}
	
	mutex_unlock(&data->lock);
	
	for (i = 0; i < data->sensor_count; i++)
	{
		struct thermal_zone_device *zone = data->sensor[i].zone;
		thermal_zone_device_update(zone, THERMAL_EVENT_UNSPECIFIED);
	}
	
	queue_delayed_work(system_freezable_wq, &data->work, delay);
}

static int caninos_tmu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	const char *names[] = {"cpu-temp", "gpu-temp", "corelogic-temp"};
	struct caninos_tmu_data *data;
	unsigned long delay;
	int ret, i;
	
	/*
	Sample Usage:
	-> cat /sys/class/thermal/thermal_zone0/type
	cpu-temp
	-> cat /sys/class/thermal/thermal_zone0/temp
	430146
	*/
	
	if (!np) {
		dev_err(&pdev->dev, "missing device OF node\n");
		return -EINVAL;
	}
	
	match = of_match_device(pdev->dev.driver->of_match_table, &pdev->dev);
	
	if (!match || !match->data)
	{
		dev_err(&pdev->dev, "could not get hardware specific data\n");
		return -EINVAL;
	}
	
	
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	
	if (!data)
	{
		dev_err(&pdev->dev, "could not allocate driver structure\n");
		return -ENOMEM;
	}

	data->model = (enum caninos_tmu_model)(match->data);
	switch(data->model)
	{
		case TMU_MODEL_K5:
			data->sensor_count = CANINOS_TMU_SENSOR_COUNT_K5;
			break;
		case TMU_MODEL_K7:
			data->sensor_count = CANINOS_TMU_SENSOR_COUNT_K7;
			break;
		default:
			dev_err(&pdev->dev, "could not get model\n");
			return -EINVAL;
	}
	
	data->tmu_clk = devm_clk_get(&pdev->dev, "thermal_sensor");
	
	if (IS_ERR(data->tmu_clk))
	{
		dev_err(&pdev->dev, "could not get device clock\n");
		return PTR_ERR(data->tmu_clk);
	}
	
	data->base = of_iomap(pdev->dev.of_node, 0);
	
	if (IS_ERR(data->base))
	{
		dev_err(&pdev->dev, "could not map device registers\n");
		return PTR_ERR(data->base);
	}
	
	clk_set_rate(data->tmu_clk, 24000000);
	clk_prepare_enable(data->tmu_clk);
	
	INIT_DELAYED_WORK(&data->work, caninos_thermal_polling_worker);
	mutex_init(&data->lock);
	
	platform_set_drvdata(pdev, data);
	
	for (i = 0; i < data->sensor_count; i++)
	{
		data->sensor[i].id = i;
		data->sensor[i].dev = &pdev->dev;
		data->sensor[i].temperature = THERMAL_TEMP_INVALID;
		
		data->sensor[i].zone = thermal_zone_device_register
			(names[i], 0, 0, &data->sensor[i], &caninos_tmu_ops, NULL,
			 CANINOS_TMU_IDLE_INTERVAL, 0);
		
		if (IS_ERR(data->sensor[i].zone))
		{
			dev_err(&pdev->dev, "could not register thermal zone device %s\n",
			        names[i]);
			ret = PTR_ERR(data->sensor[i].zone);
			goto err;
		}
	}
	
	delay = msecs_to_jiffies(CANINOS_TMU_ACTIVE_INTERVAL);
	queue_delayed_work(system_freezable_wq, &data->work, delay);

	dev_info(&pdev->dev, "probe finished");

	return 0;	
err:
	while (--i >= 0) {
		thermal_zone_device_unregister(data->sensor[i].zone);
	}
	iounmap(data->base);
	mutex_destroy(&data->lock);
	clk_disable_unprepare(data->tmu_clk);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int caninos_tmu_remove(struct platform_device *pdev)
{
	struct caninos_tmu_data *data = platform_get_drvdata(pdev);
	int i;

	
	cancel_delayed_work_sync(&data->work);
	
	for (i = 0; i < data->sensor_count; i++) {
		thermal_zone_device_unregister(data->sensor[i].zone);
	}
	
	iounmap(data->base);
	mutex_destroy(&data->lock);
	clk_disable_unprepare(data->tmu_clk);
	return 0;
}

static const struct of_device_id caninos_tmu_match[] = {
	{ .compatible = "caninos,k7-tmu", .data = (void *)TMU_MODEL_K7 },
	{ .compatible = "caninos,k5-tmu", .data = (void *)TMU_MODEL_K5 },
	{ }
};

MODULE_DEVICE_TABLE(of, caninos_tmu_match);

static struct platform_driver caninos_tmu_driver = {
	.probe = caninos_tmu_probe,
	.remove = caninos_tmu_remove,
	.driver = {
		.name = "caninos-tmu",
		.of_match_table = caninos_tmu_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(caninos_tmu_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Labrador SBCs Thermal Sensor Driver");
MODULE_LICENSE("GPL v2");

