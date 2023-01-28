/*
 *  drivers/cpufreq/cpufreq_ondemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include "cpufreq_governor.h"

#if IS_ENABLED(CONFIG_A2N)
#include <linux/a2n.h>
#endif

extern unsigned int cpu4_dvfs_limit;

/* On-demand governor macros */
#define DEF_FREQUENCY_UP_THRESHOLD		(95)
#define DOWN_THRESHOLD_MARGIN			(25)
#define DEF_SAMPLING_DOWN_FACTOR		(50)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MIN_FREQUENCY_UP_THRESHOLD		(45)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)
#define DEF_BOOST				(0)
#define IO_IS_BUSY				(0)

/* Cluster 0 little cpu */
#define DEF_FREQUENCY_STEP_CL0_0               (832000)
#define DEF_FREQUENCY_STEP_CL0_1               (1053000)
#define DEF_FREQUENCY_STEP_CL0_2               (1248000)
#define DEF_FREQUENCY_STEP_CL0_3               (1456000)
#define DEF_FREQUENCY_STEP_CL0_4               (1690000)
#define DEF_FREQUENCY_STEP_CL0_5               (1794000)
#define DEF_FREQUENCY_STEP_CL0_6               (1898000)
#define DEF_FREQUENCY_STEP_CL0_7               (2002000)

/* Cluster 1 big cpu */
#define DEF_FREQUENCY_STEP_CL1_0               (741000)
#define DEF_FREQUENCY_STEP_CL1_1               (962000)
#define DEF_FREQUENCY_STEP_CL1_2               (1170000)
#define DEF_FREQUENCY_STEP_CL1_3               (1469000)
#define DEF_FREQUENCY_STEP_CL1_4               (1703000)
#define DEF_FREQUENCY_STEP_CL1_5               (1937000)
#define DEF_FREQUENCY_STEP_CL1_6               (2158000)
#define DEF_FREQUENCY_STEP_CL1_7               (2314000)
#define DEF_FREQUENCY_STEP_CL1_8               (2496000)
#define DEF_FREQUENCY_STEP_CL1_9               (2652000)
#define DEF_FREQUENCY_STEP_CL1_10              (2704000)
#define DEF_FREQUENCY_STEP_CL1_11              (2808000)

static unsigned int down_threshold = 0;

#ifdef CONFIG_CPU_FREQ_SUSPEND
static unsigned int up_threshold_suspend = 95;
static bool boost_suspend = false;

static unsigned int up_threshold_resume = MICRO_FREQUENCY_UP_THRESHOLD;
static bool boost_resume = DEF_BOOST;
#endif

static DEFINE_PER_CPU(struct od_cpu_dbs_info_s, od_cpu_dbs_info);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
static struct cpufreq_governor cpufreq_gov_ondemand;
#endif

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Else, we adjust the frequency
 * proportional to load.
 */
static void od_check_cpu(int cpu, unsigned int load)
{
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.shared->policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int requested_freq = 0;

	/* Check for frequency increase */
	if (load >= od_tuners->up_threshold) {

		/* if we are already at full speed then break out early */
		if (policy->cur == policy->max)
			return;

		if ((cpu) && (policy->cur == cpu4_dvfs_limit))
			return;

		if (!od_tuners->boost) {
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
				else
					requested_freq = policy->max;
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
				else
					requested_freq = policy->max;
			}
			if (requested_freq > policy->max)
				requested_freq = policy->max;
		} else {
			/* Boost */
			requested_freq = policy->max;
		}

		/* If switching to max speed, apply sampling_down_factor */
		if ((requested_freq == policy->max) || (requested_freq == cpu4_dvfs_limit))
			dbs_info->rate_mult =
				od_tuners->sampling_down_factor;

		__cpufreq_driver_target(policy, requested_freq,
			CPUFREQ_RELATION_H);

		return;
	}


	/* No longer fully busy, reset rate_mult */
	dbs_info->rate_mult = 1;

	/*
	 * if we cannot reduce the frequency anymore, break out early
	 */
	if (policy->cur == policy->min)
		return;

	/* Check for frequency decrease */
	if (load < down_threshold) {
		/* Little cpu 0 */
		if (cpu == 0) {
			if (policy->cur == DEF_FREQUENCY_STEP_CL0_7)
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
			else
				requested_freq = policy->min;
		/* Big cpu 4 */
		} else {
			if (policy->cur == DEF_FREQUENCY_STEP_CL1_11)
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
			else
				requested_freq = policy->min;
		}

		if (requested_freq < policy->min)
			requested_freq = policy->min;

		__cpufreq_driver_target(policy, requested_freq,
				CPUFREQ_RELATION_L);

		return;
	}
}

static unsigned int od_dbs_timer(struct cpu_dbs_info *cdbs,
				 struct dbs_data *dbs_data, bool modify_all)
{
	struct cpufreq_policy *policy = cdbs->shared->policy;
	unsigned int cpu = policy->cpu;
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info,
			cpu);
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	int delay = 0, sample_type = dbs_info->sample_type;

	if (!modify_all)
		goto max_delay;

	/* Common NORMAL_SAMPLE setup */
	dbs_info->sample_type = OD_NORMAL_SAMPLE;
	if (sample_type == OD_SUB_SAMPLE) {
		delay = dbs_info->freq_lo_jiffies;
		__cpufreq_driver_target(policy, dbs_info->freq_lo,
					CPUFREQ_RELATION_H);
	} else {
		dbs_check_cpu(dbs_data, cpu);
		if (dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			dbs_info->sample_type = OD_SUB_SAMPLE;
			delay = dbs_info->freq_hi_jiffies;
		}
	}

max_delay:
	if (!delay)
		delay = delay_for_sampling_rate(od_tuners->sampling_rate
				* dbs_info->rate_mult);

	return delay;
}

static void update_down_threshold(struct od_dbs_tuners *od_tuners)
{
	down_threshold = ((od_tuners->up_threshold * DEF_FREQUENCY_STEP_CL0_0 / DEF_FREQUENCY_STEP_CL0_1) - DOWN_THRESHOLD_MARGIN);
	pr_info("[%s] for CPU - new value: %u\n",__func__, down_threshold);
}

/************************** sysfs interface ************************/
static struct common_dbs_data od_dbs_cdata;

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updating
 * dbs_tuners_int.sampling_rate might not be appropriate. For example, if the
 * original sampling_rate was 1 second and the requested new sampling rate is 10
 * ms because the user needs immediate reaction from ondemand governor, but not
 * sure if higher frequency will be required or not, then, the governor may
 * change the sampling rate too late; up to 1 second later. Thus, if we are
 * reducing the sampling rate, we need to make the new value effective
 * immediately.
 */
static void update_sampling_rate(struct dbs_data *dbs_data,
		unsigned int new_rate)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	int cpu;

	od_tuners->sampling_rate = new_rate = max(new_rate,
			dbs_data->min_sampling_rate);

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct od_cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (policy->governor != &cpufreq_gov_ondemand) {
			cpufreq_cpu_put(policy);
			continue;
		}
		dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
		cpufreq_cpu_put(policy);

		if (!delayed_work_pending(&dbs_info->cdbs.dwork))
			continue;

		next_sampling = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->cdbs.dwork.timer.expires;

		if (time_before(next_sampling, appointed_at)) {
			cancel_delayed_work_sync(&dbs_info->cdbs.dwork);

			gov_queue_work(dbs_data, policy,
				       usecs_to_jiffies(new_rate), true);

		}
	}
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		goto err;

	update_sampling_rate(dbs_data, input);
	return count;

err:
	pr_err("[%s] invalid cmd\n",__func__);
	return -EINVAL;
}

static ssize_t store_io_is_busy(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	od_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info,
									j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, od_tuners->io_is_busy);
	}
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		goto err;
	}
	od_tuners->up_threshold = input;

#ifdef CONFIG_CPU_FREQ_SUSPEND
	up_threshold_resume = input;
#endif

	/* update down_threshold */
	update_down_threshold(od_tuners);
	return count;

err:
	pr_err("[%s] invalid cmd\n",__func__);
	return -EINVAL;
}

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	od_tuners->sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(od_cpu_dbs_info,
				j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice_load(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == od_tuners->ignore_nice_load) { /* nothing to do */
		return count;
	}
	od_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(od_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, od_tuners->io_is_busy);
		if (od_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_boost(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct od_dbs_tuners *od_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		goto err;

	if (input > 1)
		input = 1;

	od_tuners->boost = input;

#ifdef CONFIG_CPU_FREQ_SUSPEND
	boost_resume = input;
#endif

	return count;

err:
	pr_err("[%s] invalid cmd\n",__func__);
	return -EINVAL;
}

show_store_one(od, sampling_rate);
show_store_one(od, io_is_busy);
show_store_one(od, up_threshold);
show_store_one(od, sampling_down_factor);
show_store_one(od, ignore_nice_load);
declare_show_sampling_rate_min(od);
show_store_one(od, boost);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(io_is_busy);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_ro(sampling_rate_min);
gov_sys_pol_attr_rw(boost);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
	&boost_gov_sys.attr,
	NULL
};

static struct attribute_group od_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "ondemand",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
	&boost_gov_pol.attr,
	NULL
};

static struct attribute_group od_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "ondemand",
};

/************************** sysfs end ************************/

static int od_init(struct dbs_data *dbs_data, bool notify)
{
	struct od_dbs_tuners *tuners;
	u64 idle_time;
	int cpu;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		tuners->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;

#ifdef CONFIG_CPU_FREQ_SUSPEND
		tuners->up_threshold = up_threshold_resume;
#endif
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		dbs_data->min_sampling_rate = jiffies_to_usecs(10);
	} else {
		tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;

#ifdef CONFIG_CPU_FREQ_SUSPEND
		tuners->up_threshold = up_threshold_resume;
#endif
		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = jiffies_to_usecs(10);
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;
	tuners->io_is_busy = IO_IS_BUSY;
	tuners->boost = DEF_BOOST;

#ifdef CONFIG_CPU_FREQ_SUSPEND
	tuners->boost = boost_resume;
#endif

	dbs_data->tuners = tuners;

	update_down_threshold(tuners);

	return 0;
}

static void od_exit(struct dbs_data *dbs_data, bool notify)
{
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(od_cpu_dbs_info);

static struct common_dbs_data od_dbs_cdata = {
	.governor = GOV_ONDEMAND,
	.attr_group_gov_sys = &od_attr_group_gov_sys,
	.attr_group_gov_pol = &od_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = od_dbs_timer,
	.gov_check_cpu = od_check_cpu,
	.init = od_init,
	.exit = od_exit,
	.mutex = __MUTEX_INITIALIZER(od_dbs_cdata.mutex),
};

static int od_cpufreq_governor_dbs(struct cpufreq_policy *policy,
		unsigned int event)
{
	return cpufreq_governor_dbs(policy, &od_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_ondemand = {
	.name			= "ondemand",
	.governor		= od_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

#ifdef CONFIG_CPU_FREQ_SUSPEND
void update_gov_tunables(bool is_suspend)
{
	int cpu;
	struct od_dbs_tuners *od_tuners_lit, *od_tuners_big;
	struct od_cpu_dbs_info_s *dbs_info;
	struct cpufreq_policy *policy;
	struct dbs_data *dbs_data;

	for_each_cpu(cpu, &hmp_slow_cpu_mask) {
		if (cpu_online(cpu)) {
			dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
			policy = dbs_info->cdbs.shared->policy;
			dbs_data = policy->governor_data;
			od_tuners_lit = dbs_data->tuners;
				if (is_suspend) {
					od_tuners_lit->up_threshold = up_threshold_suspend;
					od_tuners_lit->boost = boost_suspend;
				} else {
					/* resumed */
					od_tuners_lit->up_threshold = up_threshold_resume;
					od_tuners_lit->boost = boost_resume;
				}
			break;
		}
	}

	for_each_cpu(cpu, &hmp_fast_cpu_mask) {
		if (cpu_online(cpu)) {
			dbs_info = &per_cpu(od_cpu_dbs_info, cpu);
			policy = dbs_info->cdbs.shared->policy;
			dbs_data = policy->governor_data;
			od_tuners_big = dbs_data->tuners;
			if (is_suspend) {
				od_tuners_big->up_threshold = up_threshold_suspend;
				od_tuners_big->boost = boost_suspend;
			} else {
				/* resumed */
				od_tuners_big->up_threshold = up_threshold_resume;
				od_tuners_big->boost = boost_resume;
			}
			break;
		}
	}
}
#endif

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_ondemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_ondemand);
}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_ondemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
