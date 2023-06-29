/* linux/drivers/devfreq/exynos/exynos8895_bus_disp.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung EXYNOS8895 SoC DISP devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>

#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/cal-if.h>
#include "../governor.h"

static struct exynos_devfreq_data *_data = NULL;

static int exynos8895_devfreq_disp_cmu_dump(struct exynos_devfreq_data *data)
{
	mutex_lock(&data->devfreq->lock);
	cal_vclk_dbg_info(data->dfs_id);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

#ifdef CONFIG_PM_DEVFREQ
static int exynos8895_devfreq_disp_resume(struct exynos_devfreq_data *data)
{
	if (pm_qos_request_active(&data->default_pm_qos_min))
		pm_qos_update_request(&data->default_pm_qos_min,
				data->default_qos);

	pr_info("%s: set freq to: %u\n", __func__, data->default_qos);

	return 0;
}

static int exynos8895_devfreq_disp_suspend(struct exynos_devfreq_data *data)
{
	if (pm_qos_request_active(&data->default_pm_qos_min))
		pm_qos_update_request(&data->default_pm_qos_min,
				data->devfreq_profile.suspend_freq);

	pr_info("%s: set freq to: %lu\n", __func__, data->devfreq_profile.suspend_freq);

	return 0;
}

void set_devfreq_disp_pm_qos(bool is_suspend)
{
	if (_data == NULL) {
		pr_err("%s: _data is NULL !!\n", __func__);
		return;
	}

	if (is_suspend)
		exynos8895_devfreq_disp_suspend(_data);
	else
		exynos8895_devfreq_disp_resume(_data);
}
#endif

static int exynos8895_devfreq_disp_reboot(struct exynos_devfreq_data *data)
{
	data->max_freq = data->reboot_freq;
	data->devfreq->max_freq = data->max_freq;

	mutex_lock(&data->devfreq->lock);
	update_devfreq(data->devfreq);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static int exynos8895_devfreq_disp_get_freq(struct device *dev, u32 *cur_freq,
		struct clk *clk, struct exynos_devfreq_data *data)
{
	*cur_freq = (u32)cal_dfs_get_rate(data->dfs_id);
	if (*cur_freq == 0) {
		dev_err(dev, "failed to get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos8895_devfreq_disp_set_freq(struct device *dev, u32 new_freq, 
		struct clk *clk, struct exynos_devfreq_data *data)
{
	if (cal_dfs_set_rate(data->dfs_id, (unsigned long)new_freq)) {
		dev_err(dev, "failed to set frequency via CAL (%uKhz)\n",
				new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos8895_devfreq_disp_init_freq_table(struct exynos_devfreq_data *data)
{
	u32 max_freq = 0, min_freq = 0, cur_freq = 0;
	unsigned long tmp_max = 0, tmp_min = 0;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i = 0, ret = 0;

	max_freq = cal_dfs_get_max_freq(data->dfs_id);
	if (!max_freq) {
		dev_err(data->dev, "failed to get max frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "max_freq: %uKhz, get_max_freq: %uKhz\n",
			data->max_freq, max_freq);

	if (max_freq < data->max_freq) {
		rcu_read_lock();
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_max = max_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_max, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(data->dev, "not found valid OPP for max_freq\n");
			return PTR_ERR(target_opp);
		}

		data->max_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	/* min ferquency must be equal or under max frequency */
	if (data->min_freq > data->max_freq)
		data->min_freq = data->max_freq;

	min_freq = cal_dfs_get_min_freq(data->dfs_id);
	if (!min_freq) {
		dev_err(data->dev, "failed to get min frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "min_freq: %uKhz, get_min_freq: %uKhz\n",
			data->min_freq, min_freq);

	if (min_freq > data->min_freq) {
		rcu_read_lock();
		flags &= ~DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_min = min_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_min, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(data->dev, "not found valid OPP for min_freq\n");
			return PTR_ERR(target_opp);
		}

		data->min_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	dev_info(data->dev, "min_freq: %uKhz, max_freq: %uKhz\n",
			data->min_freq, data->max_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(data->dev, data->opp_list[i].freq);
	}

	if (data->max_freq < data->boot_freq) {
		data->boot_freq = data->max_freq;
		data->devfreq_profile.initial_freq = data->max_freq;
	}

	ret = cal_dfs_set_rate(data->dfs_id, data->boot_freq);
	if (ret)
		dev_err(data->dev, "failed to set boot_freq %u Khz to CAL\n", data->boot_freq);
	cur_freq = cal_dfs_get_rate(data->dfs_id);
	dev_info(data->dev, "cur_freq: %u Khz - boot_freq: %u Khz - min_freq: %u Khz - max_freq: %u Khz\n",
			cur_freq, data->boot_freq, data->min_freq, data->max_freq);

	return 0;
}

static int exynos8895_devfreq_disp_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.get_freq = exynos8895_devfreq_disp_get_freq;
	data->ops.set_freq = exynos8895_devfreq_disp_set_freq;
	data->ops.init_freq_table = exynos8895_devfreq_disp_init_freq_table;
	data->ops.reboot = exynos8895_devfreq_disp_reboot;
	data->ops.cmu_dump = exynos8895_devfreq_disp_cmu_dump;

	_data = data;

	return 0;
}

static int __init exynos8895_devfreq_disp_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_DISP,
				exynos8895_devfreq_disp_init_prepare))
		return -EINVAL;

	return 0;
}
fs_initcall(exynos8895_devfreq_disp_initcall);
