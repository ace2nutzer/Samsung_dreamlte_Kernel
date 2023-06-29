/* linux/drivers/devfreq/exynos/exynos8895_bus_mif.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS8895 SoC MIF devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/bts.h>
#include <linux/apm-exynos.h>
#include <soc/samsung/asv-exynos.h>
#include <linux/mcu_ipc.h>
#include <linux/mfd/samsung/core.h>
#include <soc/samsung/cal-if.h>
#include "../governor.h"

#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/ect_parser.h>
#include "../../soc/samsung/acpm/acpm.h"
#include "../../soc/samsung/acpm/acpm_ipc.h"
#include "exynos_ppmu.h"

static struct exynos_devfreq_data *_data = NULL;

static int exynos8895_devfreq_mif_cmu_dump(struct exynos_devfreq_data *data)
{
	mutex_lock(&data->devfreq->lock);
	cal_vclk_dbg_info(data->dfs_id);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}
#ifdef CONFIG_PM_DEVFREQ
static int exynos8895_devfreq_mif_resume(struct exynos_devfreq_data *data)
{
	if (pm_qos_request_active(&data->default_pm_qos_min))
		pm_qos_update_request(&data->default_pm_qos_min,
				data->default_qos);

	pr_info("%s: set freq to: %u\n", __func__, data->default_qos);

	return 0;
}

static int exynos8895_devfreq_mif_suspend(struct exynos_devfreq_data *data)
{
	if (pm_qos_request_active(&data->default_pm_qos_min))
		pm_qos_update_request(&data->default_pm_qos_min,
				data->devfreq_profile.suspend_freq);

	pr_info("%s: set freq to: %lu\n", __func__, data->devfreq_profile.suspend_freq);

	return 0;
}

void set_devfreq_mif_pm_qos(bool is_suspend)
{
	if (_data == NULL) {
		pr_err("%s: _data is NULL !!\n", __func__);
		return;
	}

	if (is_suspend)
		exynos8895_devfreq_mif_suspend(_data);
	else
		exynos8895_devfreq_mif_resume(_data);
}
#endif

static int exynos8895_devfreq_mif_reboot(struct exynos_devfreq_data *data)
{
	if (pm_qos_request_active(&data->default_pm_qos_max))
		pm_qos_update_request(&data->default_pm_qos_max,
				data->reboot_freq);

	return 0;
}

static int exynos8895_devfreq_mif_get_freq(struct device *dev, u32 *cur_freq,
		struct clk *clk, struct exynos_devfreq_data *data)
{
	*cur_freq = (u32)cal_dfs_get_rate(data->dfs_id);
	if (*cur_freq == 0) {
		dev_err(dev, "failed get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos8895_devfreq_mif_set_freq(struct device *dev, u32 new_freq,
		struct clk *clk, struct exynos_devfreq_data *data)
{
	if (cal_dfs_set_rate(data->dfs_id, (unsigned long)new_freq)) {
		dev_err(dev, "failed set frequency to CAL (%uKhz)\n",
				new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos8895_devfreq_mif_init_freq_table(struct exynos_devfreq_data *data)
{
	u32 max_freq = 0, min_freq = 0, cur_freq = 0;
	unsigned long tmp_max = 0, tmp_min = 0;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i = 0, ret = 0;

	max_freq = cal_dfs_get_max_freq(data->dfs_id);
	if (!max_freq) {
		dev_err(data->dev, "failed get max frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "max_freq: %u Khz, get_max_freq: %u Khz\n",
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
		dev_err(data->dev, "failed get min frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "min_freq: %u Khz, get_min_freq: %u Khz\n",
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

	dev_info(data->dev, "min_freq: %u Khz, max_freq: %u Khz\n",
			data->min_freq, data->max_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(data->dev, data->opp_list[i].freq);
	}

	data->boot_freq = data->max_freq;
	data->devfreq_profile.initial_freq = data->max_freq;

	ret = cal_dfs_set_rate(data->dfs_id, data->boot_freq);
	if (!ret)
		bts_update_scen(BS_MIF_CHANGE, data->boot_freq);
	else
		dev_err(data->dev, "failed to set boot_freq %u Khz to CAL\n", data->boot_freq);
	cur_freq = cal_dfs_get_rate(data->dfs_id);
	dev_info(data->dev, "cur_freq: %u Khz - boot_freq: %u Khz - min_freq: %u Khz - max_freq: %u Khz\n",
			cur_freq, data->boot_freq, data->min_freq, data->max_freq);

	return 0;
}

static int exynos8895_devfreq_mif_um_register(struct exynos_devfreq_data *data)
{
#ifdef CONFIG_EXYNOS_WD_DVFS
	int i;
	if (data->use_get_dev) {
		for (i = 0; i < data->um_data.um_count; i++)
			exynos_init_ppmu(data->um_data.va_base[i],
					 data->um_data.mask_v[i],
					 data->um_data.mask_a[i]);
		for (i = 0; i < data->um_data.um_count; i++)
			exynos_start_ppmu(data->um_data.va_base[i]);
	}
#endif
	return 0;
}

static int exynos8895_devfreq_mif_um_unregister(struct exynos_devfreq_data *data)
{
#ifdef CONFIG_EXYNOS_WD_DVFS
	int i;
	if (data->use_get_dev) {
		for (i = 0; i < data->um_data.um_count; i++)
			exynos_exit_ppmu(data->um_data.va_base[i]);
	}
#endif
	return 0;
}

static int exynos8895_devfreq_mif_get_status(struct exynos_devfreq_data *data)
{
#ifdef CONFIG_EXYNOS_WD_DVFS
	int i;
	struct ppmu_data ppmu = { 0, };
	u64 max = 0;

	for (i = 0; i < data->um_data.um_count; i++)
		exynos_reset_ppmu(data->um_data.va_base[i],
				  data->um_data.channel[i]);

	for (i = 0; i < data->um_data.um_count; i++) {
		exynos_read_ppmu(&ppmu, data->um_data.va_base[i],
				 data->um_data.channel[i]);
		if (!i)
			data->um_data.val_ccnt = ppmu.ccnt;
		if (max < ppmu.pmcnt0)
			max = ppmu.pmcnt0;
		if (max < ppmu.pmcnt1)
			max = ppmu.pmcnt1;
	}
	data->um_data.val_pmcnt = max;
#endif
	return 0;
}

static int exynos8895_devfreq_mif_set_freq_prepare(struct exynos_devfreq_data *data)
{
	if (data->new_freq < data->old_freq)
		bts_update_scen(BS_MIF_CHANGE, data->new_freq);
	return 0;
}

static int exynos8895_devfreq_mif_set_freq_post(struct exynos_devfreq_data *data)
{
	if (data->new_freq > data->old_freq)
		bts_update_scen(BS_MIF_CHANGE, data->new_freq);
	return 0;
}

static int exynos8895_devfreq_mif_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.um_register = exynos8895_devfreq_mif_um_register;
	data->ops.um_unregister = exynos8895_devfreq_mif_um_unregister;
	data->ops.get_dev_status = exynos8895_devfreq_mif_get_status;
	data->ops.get_freq = exynos8895_devfreq_mif_get_freq;
	data->ops.set_freq = exynos8895_devfreq_mif_set_freq;
	data->ops.init_freq_table = exynos8895_devfreq_mif_init_freq_table;
	data->ops.suspend = exynos8895_devfreq_mif_suspend;
	data->ops.reboot = exynos8895_devfreq_mif_reboot;
	data->ops.resume = exynos8895_devfreq_mif_resume;
	data->ops.cmu_dump = exynos8895_devfreq_mif_cmu_dump;
	data->ops.set_freq_prepare = exynos8895_devfreq_mif_set_freq_prepare;
	data->ops.set_freq_post = exynos8895_devfreq_mif_set_freq_post;

	_data = data;

	return 0;
}

static int __init exynos8895_devfreq_mif_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_MIF,
				exynos8895_devfreq_mif_init_prepare))
		return -EINVAL;

	return 0;
}
fs_initcall(exynos8895_devfreq_mif_initcall);
