/*
 *  drivers/cpufreq/cpufreq_conservative.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2009 Alexander Clouter <alex@digriz.org.uk>
 *
 *  Device specific logic for Exynos 8895:
 *            (C) 2020 Silvestro Sanfilippo (ace2nutzer@xda-developers.com) <ace2nutzer@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "cpufreq_governor.h"
#include <linux/pm_qos.h>

/* Conservative governor macros */
#define DEF_FREQUENCY_UP_THRESHOLD		(75) /* min 40, max 100 */
#define DOWN_THRESHOLD_MARGIN			(25)
#define DEF_SAMPLING_DOWN_FACTOR		(2)
#define MAX_SAMPLING_DOWN_FACTOR		(10)
#define DEF_BOOST				(1)

/* Cluster 0 little cpu */
#define DEF_FREQUENCY_STEP_CL0_0               (455000)
#define DEF_FREQUENCY_STEP_CL0_1               (598000)
#define DEF_FREQUENCY_STEP_CL0_2               (715000)
#define DEF_FREQUENCY_STEP_CL0_3               (832000)
#define DEF_FREQUENCY_STEP_CL0_4               (949000)
#define DEF_FREQUENCY_STEP_CL0_5               (1053000)
#define DEF_FREQUENCY_STEP_CL0_6               (1248000)
#define DEF_FREQUENCY_STEP_CL0_7               (1456000)
#define DEF_FREQUENCY_STEP_CL0_8               (1690000)
#define DEF_FREQUENCY_STEP_CL0_9               (1794000)
#define DEF_FREQUENCY_STEP_CL0_10              (1898000)
#define DEF_FREQUENCY_STEP_CL0_11              (2002000)

/* Cluster 1 big cpu */
#define DEF_FREQUENCY_STEP_CL1_0               (741000)
#define DEF_FREQUENCY_STEP_CL1_1               (858000)
#define DEF_FREQUENCY_STEP_CL1_2               (962000)
#define DEF_FREQUENCY_STEP_CL1_3               (1066000)
#define DEF_FREQUENCY_STEP_CL1_4               (1170000)
#define DEF_FREQUENCY_STEP_CL1_5               (1261000)
#define DEF_FREQUENCY_STEP_CL1_6               (1469000)
#define DEF_FREQUENCY_STEP_CL1_7               (1703000)
#define DEF_FREQUENCY_STEP_CL1_8               (1807000)
#define DEF_FREQUENCY_STEP_CL1_9               (1937000)
#define DEF_FREQUENCY_STEP_CL1_10              (2002000)
#define DEF_FREQUENCY_STEP_CL1_11              (2158000)
#define DEF_FREQUENCY_STEP_CL1_12              (2314000)
#define DEF_FREQUENCY_STEP_CL1_13              (2496000)
#define DEF_FREQUENCY_STEP_CL1_14              (2574000)
#define DEF_FREQUENCY_STEP_CL1_15              (2652000)
#define DEF_FREQUENCY_STEP_CL1_16              (2704000)
#define DEF_FREQUENCY_STEP_CL1_17              (2808000)

static unsigned int down_threshold = 0;

static DEFINE_PER_CPU(struct cs_cpu_dbs_info_s, cs_cpu_dbs_info);

static int cs_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_CONSERVATIVE
static
#endif
struct cpufreq_governor cpufreq_gov_conservative = {
	.name			= "conservative",
	.governor		= cs_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate *
 * sampling_down_factor, we check, if current idle time is more than 80%
 * (default), then we try to decrease frequency
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of maximum frequency
 */
static void cs_check_cpu(int cpu, unsigned int load)
{
	struct cs_cpu_dbs_info_s *dbs_info = &per_cpu(cs_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.shared->policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int requested_freq = 0;

	/* Check for frequency increase */
	if (load >= cs_tuners->up_threshold) {
		dbs_info->down_skip = 0;

		/* if we are already at full speed then break out early */
		if (policy->cur == policy->max)
			return;

		if (!cs_tuners->boost) {
			/* Little cpu 0 */
			if (cpu == 0) {
				if (policy->cur == DEF_FREQUENCY_STEP_CL0_0)
					requested_freq = DEF_FREQUENCY_STEP_CL0_1;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_1)
					requested_freq = DEF_FREQUENCY_STEP_CL0_2;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_2)
					requested_freq = DEF_FREQUENCY_STEP_CL0_3;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_3)
					requested_freq = DEF_FREQUENCY_STEP_CL0_4;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_4)
					requested_freq = DEF_FREQUENCY_STEP_CL0_5;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_5)
					requested_freq = DEF_FREQUENCY_STEP_CL0_6;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_6)
					requested_freq = DEF_FREQUENCY_STEP_CL0_7;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_7)
					requested_freq = DEF_FREQUENCY_STEP_CL0_8;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_8)
					requested_freq = DEF_FREQUENCY_STEP_CL0_9;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_9)
					requested_freq = DEF_FREQUENCY_STEP_CL0_10;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL0_10)
					requested_freq = DEF_FREQUENCY_STEP_CL0_11;
				else
					return;
			/* Big cpu 4 */
			} else {
				if (policy->cur == DEF_FREQUENCY_STEP_CL1_0)
					requested_freq = DEF_FREQUENCY_STEP_CL1_1;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_1)
					requested_freq = DEF_FREQUENCY_STEP_CL1_2;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_2)
					requested_freq = DEF_FREQUENCY_STEP_CL1_3;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_3)
					requested_freq = DEF_FREQUENCY_STEP_CL1_4;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_4)
					requested_freq = DEF_FREQUENCY_STEP_CL1_5;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_5)
					requested_freq = DEF_FREQUENCY_STEP_CL1_6;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_6)
					requested_freq = DEF_FREQUENCY_STEP_CL1_7;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_7)
					requested_freq = DEF_FREQUENCY_STEP_CL1_8;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_8)
					requested_freq = DEF_FREQUENCY_STEP_CL1_9;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_9)
					requested_freq = DEF_FREQUENCY_STEP_CL1_10;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_10)
					requested_freq = DEF_FREQUENCY_STEP_CL1_11;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_11)
					requested_freq = DEF_FREQUENCY_STEP_CL1_12;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_12)
					requested_freq = DEF_FREQUENCY_STEP_CL1_13;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_13)
					requested_freq = DEF_FREQUENCY_STEP_CL1_14;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_14)
					requested_freq = DEF_FREQUENCY_STEP_CL1_15;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_15)
					requested_freq = DEF_FREQUENCY_STEP_CL1_16;
				else if (policy->cur == DEF_FREQUENCY_STEP_CL1_16)
					requested_freq = DEF_FREQUENCY_STEP_CL1_17;
				else
					return;
			}

			if (requested_freq > policy->max)
				requested_freq = policy->max;

		} else {
			/* Boost */
			/* Little cpu 0 */
			if (cpu == 0) {
				if (policy->cur == DEF_FREQUENCY_STEP_CL0_0) {
					requested_freq = DEF_FREQUENCY_STEP_CL0_1;

					if (requested_freq > policy->max)
						requested_freq = policy->max;

				} else {
					requested_freq = policy->max;
				}
			/* Big cpu 4 */
			} else {
				requested_freq = policy->max;
			}
		}

		__cpufreq_driver_target(policy, requested_freq,
			CPUFREQ_RELATION_H);
		return;
	}

	/*
	 * if we cannot reduce the frequency anymore, break out early
	 */
	if (policy->cur == policy->min)
		return;

	/* if sampling_down_factor is active break out early */
	if (++dbs_info->down_skip < cs_tuners->sampling_down_factor)
		return;
	dbs_info->down_skip = 0;

	/* Check for frequency decrease */
	if (load <= down_threshold) {
		/* Little cpu 0 */
		if (cpu == 0) {
			if (policy->cur == DEF_FREQUENCY_STEP_CL0_11)
				requested_freq = DEF_FREQUENCY_STEP_CL0_10;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_10)
				requested_freq = DEF_FREQUENCY_STEP_CL0_9;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_9)
				requested_freq = DEF_FREQUENCY_STEP_CL0_8;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_8)
				requested_freq = DEF_FREQUENCY_STEP_CL0_7;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_7)
				requested_freq = DEF_FREQUENCY_STEP_CL0_6;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_6)
				requested_freq = DEF_FREQUENCY_STEP_CL0_5;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_5)
				requested_freq = DEF_FREQUENCY_STEP_CL0_4;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_4)
				requested_freq = DEF_FREQUENCY_STEP_CL0_3;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_3)
				requested_freq = DEF_FREQUENCY_STEP_CL0_2;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_2)
				requested_freq = DEF_FREQUENCY_STEP_CL0_1;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL0_1)
				requested_freq = DEF_FREQUENCY_STEP_CL0_0;
			else
				return;
		/* Big cpu 4 */
		} else {
			if (policy->cur == DEF_FREQUENCY_STEP_CL1_17)
				requested_freq = DEF_FREQUENCY_STEP_CL1_16;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_16)
				requested_freq = DEF_FREQUENCY_STEP_CL1_15;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_15)
				requested_freq = DEF_FREQUENCY_STEP_CL1_14;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_14)
				requested_freq = DEF_FREQUENCY_STEP_CL1_13;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_13)
				requested_freq = DEF_FREQUENCY_STEP_CL1_12;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_12)
				requested_freq = DEF_FREQUENCY_STEP_CL1_11;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_11)
				requested_freq = DEF_FREQUENCY_STEP_CL1_10;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_10)
				requested_freq = DEF_FREQUENCY_STEP_CL1_9;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_9)
				requested_freq = DEF_FREQUENCY_STEP_CL1_8;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_8)
				requested_freq = DEF_FREQUENCY_STEP_CL1_7;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_7)
				requested_freq = DEF_FREQUENCY_STEP_CL1_6;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_6)
				requested_freq = DEF_FREQUENCY_STEP_CL1_5;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_5)
				requested_freq = DEF_FREQUENCY_STEP_CL1_4;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_4)
				requested_freq = DEF_FREQUENCY_STEP_CL1_3;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_3)
				requested_freq = DEF_FREQUENCY_STEP_CL1_2;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_2)
				requested_freq = DEF_FREQUENCY_STEP_CL1_1;
			else if (policy->cur == DEF_FREQUENCY_STEP_CL1_1)
				requested_freq = DEF_FREQUENCY_STEP_CL1_0;
			else
				return;
		}

		if (requested_freq < policy->min)
			requested_freq = policy->min;

		__cpufreq_driver_target(policy, requested_freq,
				CPUFREQ_RELATION_L);
	}
}

static unsigned int cs_dbs_timer(struct cpu_dbs_info *cdbs,
				 struct dbs_data *dbs_data, bool modify_all)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;

	if (modify_all)
		dbs_check_cpu(dbs_data, cdbs->shared->policy->cpu);

	return delay_for_sampling_rate(cs_tuners->sampling_rate);
}

static int dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cs_cpu_dbs_info_s *dbs_info =
					&per_cpu(cs_cpu_dbs_info, freq->cpu);
	struct cpufreq_policy *policy = cpufreq_cpu_get_raw(freq->cpu);

	return 0;
}

static struct notifier_block cs_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier,
};

static void update_down_threshold(struct cs_dbs_tuners *cs_tuners)
{
	down_threshold = ((cs_tuners->up_threshold * DEF_FREQUENCY_STEP_CL0_0 / DEF_FREQUENCY_STEP_CL0_1) - DOWN_THRESHOLD_MARGIN);
}

/************************** sysfs interface ************************/
static struct common_dbs_data cs_dbs_cdata;

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	cs_tuners->sampling_down_factor = input;
	return count;
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	cs_tuners->sampling_rate = max(input, dbs_data->min_sampling_rate);
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (input < 40 || input > 100)
		return -EINVAL;

	cs_tuners->up_threshold = input;

	/* update down_threshold */
	update_down_threshold(cs_tuners);

	return count;
}

static ssize_t store_ignore_nice_load(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == cs_tuners->ignore_nice_load) /* nothing to do */
		return count;

	cs_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct cs_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(cs_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
					&dbs_info->cdbs.prev_cpu_wall, 0);
		if (cs_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}

static ssize_t store_boost(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct cs_dbs_tuners *cs_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	cs_tuners->boost = input;
	return count;
}

show_store_one(cs, sampling_rate);
show_store_one(cs, sampling_down_factor);
show_store_one(cs, up_threshold);
show_store_one(cs, ignore_nice_load);
show_store_one(cs, boost);
declare_show_sampling_rate_min(cs);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_rw(boost);
gov_sys_pol_attr_ro(sampling_rate_min);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	&boost_gov_sys.attr,
	NULL
};

static struct attribute_group cs_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "conservative",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	&boost_gov_pol.attr,
	NULL
};

static struct attribute_group cs_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "conservative",
};

/************************** sysfs end ************************/

static int cs_init(struct dbs_data *dbs_data, bool notify)
{
	struct cs_dbs_tuners *tuners;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;
	tuners->boost = DEF_BOOST;

	dbs_data->tuners = tuners;
	dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
		jiffies_to_usecs(10);

	update_down_threshold(tuners);

	if (notify)
		cpufreq_register_notifier(&cs_cpufreq_notifier_block,
					  CPUFREQ_TRANSITION_NOTIFIER);

	return 0;
}

static void cs_exit(struct dbs_data *dbs_data, bool notify)
{
	if (notify)
		cpufreq_unregister_notifier(&cs_cpufreq_notifier_block,
					    CPUFREQ_TRANSITION_NOTIFIER);

	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(cs_cpu_dbs_info);

static struct common_dbs_data cs_dbs_cdata = {
	.governor = GOV_CONSERVATIVE,
	.attr_group_gov_sys = &cs_attr_group_gov_sys,
	.attr_group_gov_pol = &cs_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = cs_dbs_timer,
	.gov_check_cpu = cs_check_cpu,
	.init = cs_init,
	.exit = cs_exit,
	.mutex = __MUTEX_INITIALIZER(cs_dbs_cdata.mutex),
};

static int cs_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	return cpufreq_governor_dbs(policy, &cs_dbs_cdata, event);
}

#ifdef CONFIG_ARCH_EXYNOS
static int cpufreq_conservative_cluster1_min_qos_handler(struct notifier_block *b,
						unsigned long val, void *v)
{
	struct cs_cpu_dbs_info_s *pcpu;
	struct cpufreq_conservative_tunables *tunables;
	unsigned long flags;
	int ret = NOTIFY_OK;
	int cpu = 4; /* policy cpu of cluster 1 */

	pcpu = &per_cpu(cs_cpu_dbs_info, cpu);

	mutex_lock(&cpufreq_governor_lock);

	if (!pcpu->policy || !pcpu->policy->governor_data ||
		!pcpu->policy->governor) {
		ret = NOTIFY_BAD;
		goto exit;
	}

	//trace_cpufreq_conservative_cpu_min_qos(cpu, val, pcpu->policy->cur);

	if (val < pcpu->policy->cur)
		tunables = pcpu->policy->governor_data;

exit:
	mutex_unlock(&cpufreq_governor_lock);
	return ret;
}

static struct notifier_block cpufreq_conservative_cluster1_min_qos_notifier = {
	.notifier_call = cpufreq_conservative_cluster1_min_qos_handler,
};

static int cpufreq_conservative_cluster1_max_qos_handler(struct notifier_block *b,
						unsigned long val, void *v)
{
	struct cs_cpu_dbs_info_s *pcpu;
	struct cpufreq_conservative_tunables *tunables;
	unsigned long flags;
	int ret = NOTIFY_OK;
	int cpu = 4; /* policy cpu of cluster1 */

	pcpu = &per_cpu(cs_cpu_dbs_info, cpu);

	mutex_lock(&cpufreq_governor_lock);

	if (!pcpu->policy || !pcpu->policy->governor_data ||
		!pcpu->policy->governor) {
		ret = NOTIFY_BAD;
		goto exit;
	}

	//trace_cpufreq_conservative_cpu_max_qos(cpu, val, pcpu->policy->cur);

	if (val > pcpu->policy->cur)
		tunables = pcpu->policy->governor_data;

exit:
	mutex_unlock(&cpufreq_governor_lock);
	return ret;
}

static struct notifier_block cpufreq_conservative_cluster1_max_qos_notifier = {
	.notifier_call = cpufreq_conservative_cluster1_max_qos_handler,
};

static int cpufreq_conservative_cluster0_min_qos_handler(struct notifier_block *b,
						unsigned long val, void *v)
{
	struct cs_cpu_dbs_info_s *pcpu;
	struct cpufreq_conservative_tunables *tunables;
	unsigned long flags;
	int ret = NOTIFY_OK;
	int cpu = 0; /* policy cpu of cluster0 */

	pcpu = &per_cpu(cs_cpu_dbs_info, cpu);

	mutex_lock(&cpufreq_governor_lock);

	if (!pcpu->policy || !pcpu->policy->governor_data ||
		!pcpu->policy->governor) {
		ret = NOTIFY_BAD;
		goto exit;
	}

	//trace_cpufreq_conservative_cpu_min_qos(cpu, val, pcpu->policy->cur);

	if (val < pcpu->policy->cur)
		tunables = pcpu->policy->governor_data;

exit:
	mutex_unlock(&cpufreq_governor_lock);
	return ret;
}

static struct notifier_block cpufreq_conservative_cluster0_min_qos_notifier = {
	.notifier_call = cpufreq_conservative_cluster0_min_qos_handler,
};

static int cpufreq_conservative_cluster0_max_qos_handler(struct notifier_block *b,
						unsigned long val, void *v)
{
	struct cs_cpu_dbs_info_s *pcpu;
	struct cpufreq_conservative_tunables *tunables;
	unsigned long flags;
	int ret = NOTIFY_OK;
	int cpu = 0; /* policy cpu of cluster0 */

	pcpu = &per_cpu(cs_cpu_dbs_info, cpu);

	mutex_lock(&cpufreq_governor_lock);

	if (!pcpu->policy ||!pcpu->policy->governor_data ||
		!pcpu->policy->governor) {
		ret = NOTIFY_BAD;
		goto exit;
	}

	//trace_cpufreq_conservative_cpu_max_qos(cpu, val, pcpu->policy->cur);

	if (val > pcpu->policy->cur)
		tunables = pcpu->policy->governor_data;

exit:
	mutex_unlock(&cpufreq_governor_lock);
	return ret;
}

static struct notifier_block cpufreq_conservative_cluster0_max_qos_notifier = {
	.notifier_call = cpufreq_conservative_cluster0_max_qos_handler,
};
#endif

static int __init cpufreq_gov_dbs_init(void)
{
#ifdef CONFIG_ARCH_EXYNOS
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MIN, &cpufreq_conservative_cluster1_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MAX, &cpufreq_conservative_cluster1_max_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_FREQ_MIN, &cpufreq_conservative_cluster0_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CLUSTER0_FREQ_MAX, &cpufreq_conservative_cluster0_max_qos_notifier);
#endif

	return cpufreq_register_governor(&cpufreq_gov_conservative);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_conservative);
}

MODULE_AUTHOR("Alexander Clouter <alex@digriz.org.uk>");
MODULE_DESCRIPTION("'cpufreq_conservative' - A dynamic cpufreq governor for "
		"Low Latency Frequency Transition capable processors "
		"optimised for use in a battery environment");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_CONSERVATIVE
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
