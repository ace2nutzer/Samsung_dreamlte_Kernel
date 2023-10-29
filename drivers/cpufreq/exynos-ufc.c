/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/reboot.h>
#include <linux/thermal.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <soc/samsung/exynos-cpu_hotplug.h>

#include "exynos-acme.h"

#if IS_ENABLED(CONFIG_A2N)
#include <linux/a2n.h>
#endif

#if defined(CONFIG_EXYNOS_THERMAL)
#include "exynos_tmu.h"
#endif

/*********************************************************************
 *                          SYSFS INTERFACES                         *
 *********************************************************************/

/* custom DVFS */
static unsigned int user_cpu_dvfs_max_temp = 60; /* °C */
static unsigned int cpu_dvfs_max_temp = 0;
static unsigned int cpu_dvfs_peak_temp = 0;
static int cpu_temp = 0;
static unsigned int cpu_dvfs_sleep_time = 6; /* ms */
static unsigned int cpu4_dvfs_limit = 0;
static unsigned int cpu0_dvfs_limit = 0;
static unsigned int cpu_dvfs_min_temp = 0;
static unsigned int user_cpu0_min_limit = 0;
static unsigned int user_cpu4_min_limit = 0;
static int dvfs_bat_down_threshold = 3300; /* mV */
static int dvfs_bat_up_threshold = 0;
extern int dvfs_bat_vol;
extern int dvfs_bat_peak_vol;

static struct task_struct *cpu_dvfs_thread = NULL;
static struct pm_qos_request cpu_maxlock_cl0;
static struct pm_qos_request cpu_maxlock_cl1;
static struct pm_qos_request cpu_minlock_cl0;
static struct pm_qos_request cpu_minlock_cl1;


#define CPU_DVFS_RANGE_TEMP_MIN		(45)	/* °C */
#define CPU_DVFS_TJMAX			(100)	/* °C */
#define CPU_DVFS_AVOID_SHUTDOWN_TEMP	(110)	/* °C */
#define CPU_DVFS_SHUTDOWN_TEMP		(115)	/* °C */
#define CPU_DVFS_MARGIN_TEMP		(10)	/* °C */
#define CPU_DVFS_STEP_DOWN_TEMP		(5)	/* °C */
#define DVFS_BAT_THRESHOLD_MIN		(3300)	/* mV */
#define DVFS_BAT_THRESHOLD_MAX		(3600)	/* mV */
#define DVFS_BAT_THRESHOLD_MARGIN	(100)	/* mV */
#define CPU_DVFS_DEBUG			(0)

/* Cluster 1 big cpu */
#define FREQ_STEP_CL1_0               (741000)
#define FREQ_STEP_CL1_1               (962000)
#define FREQ_STEP_CL1_2               (1170000)
#define FREQ_STEP_CL1_3               (1469000)
#define FREQ_STEP_CL1_4               (1703000)
#define FREQ_STEP_CL1_5               (1937000)
#define FREQ_STEP_CL1_6               (2158000)
#define FREQ_STEP_CL1_7               (2314000)
#define FREQ_STEP_CL1_8               (2496000)
#define FREQ_STEP_CL1_9               (2652000)
#define FREQ_STEP_CL1_10              (2704000)
#define FREQ_STEP_CL1_11              (2808000)

/* Cluster 0 little cpu */
#define FREQ_STEP_CL0_0               (832000)
#define FREQ_STEP_CL0_1               (1053000)
#define FREQ_STEP_CL0_2               (1248000)
#define FREQ_STEP_CL0_3               (1456000)
#define FREQ_STEP_CL0_4               (1690000)
#define FREQ_STEP_CL0_5               (1794000)
#define FREQ_STEP_CL0_6               (1898000)
#define FREQ_STEP_CL0_7               (2002000)

static DEFINE_MUTEX(poweroff_lock);

/*
 * Log2 of the number of scale size. The frequencies are scaled up or
 * down as the multiple of this number.
 */
#define SCALE_SIZE	2

//static int last_max_limit = -1;
static const char sse_mode = 0; /* force 64-bit mode */

static ssize_t show_cpufreq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	ssize_t count = 0;
	int i, scale = 0;

	list_for_each_entry_reverse(domain, domains, list) {
		for (i = 0; i < domain->table_size; i++) {
			unsigned int freq = domain->freq_table[i].frequency;

			if (freq == CPUFREQ_ENTRY_INVALID)
				continue;

			count += snprintf(&buf[count], 10, "%d ",
					freq >> (scale * SCALE_SIZE));
		}

		scale++;
	}

	count += snprintf(&buf[count - 1], 2, "\n");

	return count - 1;
}

static ssize_t show_user_cpu0_min_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u Khz\n", user_cpu0_min_limit);
}

static ssize_t store_user_cpu0_min_limit(struct kobject *kobj,
				struct attribute *attr, const char *buf,
				size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u", &val)) {
		if (val < 0 || val > 2002000) {
			pr_err("%s: out of range 0 - 2002000 Khz\n", __func__);
			goto err;
		}
	}

	user_cpu0_min_limit = val;
	pm_qos_update_request(&cpu_minlock_cl0, val);
	return count;
err:
	pr_err("%s: invalid cmd\n", __func__);
	return -EINVAL;
}

static ssize_t show_user_cpu4_min_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u Khz\n", user_cpu4_min_limit);
}

static ssize_t store_user_cpu4_min_limit(struct kobject *kobj,
				struct attribute *attr, const char *buf,
				size_t count)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u", &val)) {
		if (val < 0 || val > 2808000) {
			pr_err("%s: out of range 0 - 2808000 Khz\n", __func__);
			goto err;
		}
	}

	user_cpu4_min_limit = val;
	pm_qos_update_request(&cpu_minlock_cl1, val);
	return count;
err:
	pr_err("%s: invalid cmd\n", __func__);
	return -EINVAL;
}

static ssize_t show_cpufreq_min_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	unsigned int pm_qos_min;
	int scale = -1;

	list_for_each_entry_reverse(domain, domains, list) {
		scale++;

		/* get value of minimum PM QoS */
		pm_qos_min = pm_qos_request(domain->pm_qos_min_class);
		if (pm_qos_min > 0) {
			pm_qos_min = min(pm_qos_min, domain->max_freq);
			pm_qos_min = max(pm_qos_min, domain->min_freq);

			/*
			 * To manage frequencies of all domains at once,
			 * scale down frequency as multiple of 4.
			 * ex) domain2 = freq
			 *     domain1 = freq /4
			 *     domain0 = freq /16
			 */
			pm_qos_min = pm_qos_min >> (scale * SCALE_SIZE);
			return snprintf(buf, 10, "%u\n", pm_qos_min);
		}
	}

	/*
	 * If there is no QoS at all domains, it returns minimum
	 * frequency of last domain
	 */
	return snprintf(buf, 10, "%u\n",
		first_domain()->min_freq >> (scale * SCALE_SIZE));
}

#if defined(CONFIG_HMP_VARIABLE_SCALE)
static inline void control_boost(bool enable)
{
	static bool boosted = false;

	if (boosted && !enable) {
		set_hmp_boost(HMP_BOOSTING_DISABLE);
		boosted = false;
	} else if (!boosted && enable) {
		set_hmp_boost(HMP_BOOSTING_ENABLE);
		boosted = true;
	}
}
#endif

static ssize_t store_cpufreq_min_limit(struct kobject *kobj,
				struct attribute *attr, const char *buf,
				size_t count)
{
#if 0
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	int input, scale = -1;
	unsigned int freq;
	unsigned int req_limit_freq;
	bool set_max = false;
	bool set_limit = false;
	int index = 0;
	int ret = 0;
	struct cpumask mask;

	if (sscanf(buf, "%8d", &input) < 1)
		return -EINVAL;

	if (!domains) {
		pr_err("failed to get domains!\n");
		return -ENXIO;
	}

	list_for_each_entry_reverse(domain, domains, list) {
		struct exynos_ufc *ufc, *r_ufc = NULL, *r_ufc_32 = NULL;
		struct cpufreq_policy *policy = NULL;

		cpumask_and(&mask, &domain->cpus, cpu_online_mask);
		if (!cpumask_weight(&mask))
			continue;

		policy = cpufreq_cpu_get_raw(cpumask_any(&mask));
		if (!policy)
			continue;

		ufc = list_entry(&domain->ufc_list, struct exynos_ufc, list);

		list_for_each_entry(ufc, &domain->ufc_list, list) {
			if (ufc->info.ctrl_type == PM_QOS_MIN_LIMIT) {
				if (ufc->info.exe_mode == AARCH64_MODE)
					r_ufc = ufc;
				else
					r_ufc_32 = ufc;
			}
		}

		scale++;

		if (set_limit) {
			req_limit_freq = min(req_limit_freq, domain->max_freq);
			pm_qos_update_request(&domain->user_min_qos_req, req_limit_freq);
			set_limit = false;
			continue;
		}

		if (set_max) {
			unsigned int qos = domain->max_freq;

			if (domain->user_default_qos)
				qos = domain->user_default_qos;

			pm_qos_update_request(&domain->user_min_qos_req, qos);
			continue;
		}

		/* Clear all constraint by cpufreq_min_limit */
		if (input < 0) {
			pm_qos_update_request(&domain->user_min_qos_req, 0);
#if defined(CONFIG_HMP_VARIABLE_SCALE)
			control_boost(0);
#endif
			continue;
		}

		/*
		 * User inputs scaled down frequency. To recover real
		 * frequency, scale up frequency as multiple of 4.
		 * ex) domain2 = freq
		 *     domain1 = freq * 4
		 *     domain0 = freq * 16
		 */
		freq = input << (scale * SCALE_SIZE);

		if (freq < domain->min_freq) {
			pm_qos_update_request(&domain->user_min_qos_req, 0);
			continue;
		}

		if (r_ufc) {
			if (sse_mode && r_ufc_32)
				r_ufc = r_ufc_32;

			ret = cpufreq_frequency_table_target(policy, domain->freq_table,
							freq, CPUFREQ_RELATION_L, &index);
			if (ret) {
				pr_err("target frequency(%d) out of range\n", freq);
				continue;
			}
			req_limit_freq = r_ufc->info.freq_table[index].limit_freq;
			if (req_limit_freq)
				set_limit = true;
		}

		freq = min(freq, domain->max_freq);
		pm_qos_update_request(&domain->user_min_qos_req, freq);
#if defined(CONFIG_HMP_VARIABLE_SCALE)
		control_boost(1);
#endif
		set_max = true;
	}
#endif
	return count;
}

static ssize_t store_cpufreq_min_limit_wo_boost(struct kobject *kobj,
				struct attribute *attr, const char *buf,
				size_t count)
{
#if 0
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	int input, scale = -1;
	unsigned int freq;
	unsigned int req_limit_freq;
	bool set_max = false;
	bool set_limit = false;
	int index = 0;
	int ret = 0;
	struct cpumask mask;

	if (sscanf(buf, "%8d", &input) < 1)
		return -EINVAL;

	if (!domains) {
		pr_err("failed to get domains!\n");
		return -ENXIO;
	}

	list_for_each_entry_reverse(domain, domains, list) {
		struct exynos_ufc *ufc, *r_ufc = NULL, *r_ufc_32 = NULL;
		struct cpufreq_policy *policy = NULL;

		cpumask_and(&mask, &domain->cpus, cpu_online_mask);
		if (!cpumask_weight(&mask))
			continue;

		policy = cpufreq_cpu_get_raw(cpumask_any(&mask));
		if (!policy)
			continue;

		ufc = list_entry(&domain->ufc_list, struct exynos_ufc, list);

		list_for_each_entry(ufc, &domain->ufc_list, list) {
			if (ufc->info.ctrl_type == PM_QOS_MIN_WO_BOOST_LIMIT) {
				if (ufc->info.exe_mode == AARCH64_MODE)
					r_ufc = ufc;
				else
					r_ufc_32 = ufc;
			}
		}

		scale++;

		if (set_limit) {
			req_limit_freq = min(req_limit_freq, domain->max_freq);
			pm_qos_update_request(&domain->user_min_qos_req, req_limit_freq);
			set_limit = false;
			continue;
		}

		if (set_max) {
			unsigned int qos = domain->max_freq;

			if (domain->user_default_qos)
				qos = domain->user_default_qos;

			pm_qos_update_request(&domain->user_min_qos_wo_boost_req, qos);
			continue;
		}

		/* Clear all constraint by cpufreq_min_limit */
		if (input < 0) {
			pm_qos_update_request(&domain->user_min_qos_wo_boost_req, 0);
			continue;
		}

		/*
		 * User inputs scaled down frequency. To recover real
		 * frequency, scale up frequency as multiple of 4.
		 * ex) domain2 = freq
		 *     domain1 = freq * 4
		 *     domain0 = freq * 16
		 */
		freq = input << (scale * SCALE_SIZE);

		if (freq < domain->min_freq) {
			pm_qos_update_request(&domain->user_min_qos_wo_boost_req, 0);
			continue;
		}

		if (r_ufc) {
			if (sse_mode && r_ufc_32)
				r_ufc = r_ufc_32;

			ret = cpufreq_frequency_table_target(policy, domain->freq_table,
							freq, CPUFREQ_RELATION_L, &index);
			if (ret) {
				pr_err("target frequency(%d) out of range\n", freq);
				continue;
			}
			req_limit_freq = r_ufc->info.freq_table[index].limit_freq;
			if (req_limit_freq)
				set_limit = true;
		}

		freq = min(freq, domain->max_freq);
		pm_qos_update_request(&domain->user_min_qos_wo_boost_req, freq);

		set_max = true;
	}
#endif
	return count;
}

static ssize_t show_cpufreq_max_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	unsigned int pm_qos_max;
	int scale = -1;

	if (!domains) {
		pr_err("failed to get domains!\n");
		return -ENXIO;
	}

	list_for_each_entry_reverse(domain, domains, list) {
		scale++;

		/* get value of minimum PM QoS */
		pm_qos_max = pm_qos_request(domain->pm_qos_max_class);
		if (pm_qos_max > 0) {
			pm_qos_max = min(pm_qos_max, domain->max_freq);
			pm_qos_max = max(pm_qos_max, domain->min_freq);

			/*
			 * To manage frequencies of all domains at once,
			 * scale down frequency as multiple of 4.
			 * ex) domain2 = freq
			 *     domain1 = freq /4
			 *     domain0 = freq /16
			 */
			pm_qos_max = pm_qos_max >> (scale * SCALE_SIZE);
			return snprintf(buf, 10, "%u\n", pm_qos_max);
		}
	}

	/*
	 * If there is no QoS at all domains, it returns minimum
	 * frequency of last domain
	 */
	return snprintf(buf, 10, "%u\n",
		first_domain()->min_freq >> (scale * SCALE_SIZE));
}

struct pm_qos_request cpu_online_max_qos_req;
static void enable_domain_cpus(struct exynos_cpufreq_domain *domain)
{
	struct cpumask mask;

	if (domain == first_domain())
		return;

	cpumask_or(&mask, cpu_online_mask, &domain->cpus);
	pm_qos_update_request(&cpu_online_max_qos_req, cpumask_weight(&mask));
}

static void disable_domain_cpus(struct exynos_cpufreq_domain *domain)
{
	struct cpumask mask;

	if (domain == first_domain())
		return;

	cpumask_andnot(&mask, cpu_online_mask, &domain->cpus);
	pm_qos_update_request(&cpu_online_max_qos_req, cpumask_weight(&mask));
}

#if 0
static void cpufreq_max_limit_update(unsigned int input_freq)
{
	struct list_head *domains = get_domain_list();
	struct exynos_cpufreq_domain *domain;
	int scale = -1;
	unsigned int freq;
	bool set_max = false;
	unsigned int req_limit_freq;
	bool set_limit = false;
	int index = 0;
	int ret = 0;
	struct cpumask mask;

	list_for_each_entry_reverse(domain, domains, list) {
		struct exynos_ufc *ufc, *r_ufc = NULL, *r_ufc_32 = NULL;
		struct cpufreq_policy *policy = NULL;

		cpumask_and(&mask, &domain->cpus, cpu_online_mask);
		if (cpumask_weight(&mask))
			policy = cpufreq_cpu_get_raw(cpumask_any(&mask));

		ufc = list_entry(&domain->ufc_list, struct exynos_ufc, list);

		list_for_each_entry(ufc, &domain->ufc_list, list) {
			if (ufc->info.ctrl_type == PM_QOS_MAX_LIMIT) {
				if (ufc->info.exe_mode == AARCH64_MODE)
					r_ufc = ufc;
				else
					r_ufc_32 = ufc;
			}
		}

		scale++;

		if (set_limit) {
			req_limit_freq = max(req_limit_freq, domain->min_freq);
			pm_qos_update_request(&domain->user_max_qos_req,
					req_limit_freq);
			set_limit = false;
			continue;
		}

		if (set_max) {
			pm_qos_update_request(&domain->user_max_qos_req,
					domain->max_freq);
			continue;
		}

		/* Clear all constraint by cpufreq_max_limit */
		if (input_freq < 0) {
			enable_domain_cpus(domain);
			pm_qos_update_request(&domain->user_max_qos_req,
						domain->max_freq);
			continue;
		}

		/*
		 * User inputs scaled down frequency. To recover real
		 * frequency, scale up frequency as multiple of 4.
		 * ex) domain2 = freq
		 *     domain1 = freq * 4
		 *     domain0 = freq * 16
		 */
		freq = input_freq << (scale * SCALE_SIZE);

		if (policy && r_ufc) {
			if (sse_mode && r_ufc_32)
				r_ufc = r_ufc_32;

			ret = cpufreq_frequency_table_target(policy, domain->freq_table,
							freq, CPUFREQ_RELATION_L, &index);
			if (ret) {
				pr_err("target frequency(%d) out of range\n", freq);
				continue;
			}

			req_limit_freq = r_ufc->info.freq_table[index].limit_freq;
			if (req_limit_freq)
				set_limit = true;
		}

		if (freq < domain->min_freq) {
			set_limit = false;
			pm_qos_update_request(&domain->user_max_qos_req, 0);
			disable_domain_cpus(domain);
			continue;
		}

		enable_domain_cpus(domain);

		freq = max(freq, domain->min_freq);
		pm_qos_update_request(&domain->user_max_qos_req, freq);

		set_max = true;
	}
}
#endif

static ssize_t store_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
/*
	int input;

	if (sscanf(buf, "%8d", &input) < 1)
		return -EINVAL;

	last_max_limit = input;
	cpufreq_max_limit_update(input);
*/
	return count;
}

static ssize_t show_execution_mode_change(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", sse_mode);
}

static ssize_t store_execution_mode_change(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
/*
	int input;
	int prev_mode;

	if (sscanf(buf, "%8d", &input) < 1)
		return -EINVAL;

	prev_mode = sse_mode;
	sse_mode = !!input;

	if (prev_mode != sse_mode) {
		if (last_max_limit != -1)
			cpufreq_max_limit_update(last_max_limit);
	}
*/
	return count;
}

static ssize_t show_cpu_dvfs_max_temp(struct kobject *kobj, struct attribute *attr, char *buf)
{
	sprintf(buf, "%s[cpu_temp]\t%d °C\n",buf, cpu_temp);
	sprintf(buf, "%s[peak_temp]\t%u °C\n",buf, cpu_dvfs_peak_temp);
	sprintf(buf, "%s[user_max_temp]\t%u °C\n",buf, user_cpu_dvfs_max_temp);
	sprintf(buf, "%s[cal_max_temp]\t%u °C\n",buf, cpu_dvfs_max_temp);
	sprintf(buf, "%s[tjmax]\t\t%d °C\n",buf, (int)CPU_DVFS_TJMAX);
	sprintf(buf, "%s[dvfs_avoid_shutdown_temp]\t%d °C\n",buf, (int)CPU_DVFS_AVOID_SHUTDOWN_TEMP);
	sprintf(buf, "%s[dvfs_shutdown_temp]\t%d °C\n",buf, (int)CPU_DVFS_SHUTDOWN_TEMP);
	sprintf(buf, "%s[cpu4_max_freq]\t%u KHz\n",buf, cpu4_max_freq);
	sprintf(buf, "%s[cpu4_dvfs_limit]\t%u KHz\n",buf, cpu4_dvfs_limit);
	sprintf(buf, "%s[cpu0_max_freq]\t%u KHz\n",buf, cpu0_max_freq);
	sprintf(buf, "%s[cpu0_dvfs_limit]\t%u KHz\n",buf, cpu0_dvfs_limit);
	sprintf(buf, "%s[dvfs_bat_down_threshold]\t%d mV\n",buf, dvfs_bat_down_threshold);
	sprintf(buf, "%s[dvfs_bat_peak_vol]\t%d mV\n",buf, dvfs_bat_peak_vol);

	return strlen(buf);
}

static ssize_t store_cpu_dvfs_max_temp(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	unsigned int tmp = 0;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	if (sscanf(buf, "%u", &tmp)) {
		if (tmp < CPU_DVFS_RANGE_TEMP_MIN || tmp > CPU_DVFS_TJMAX) {
			pr_err("%s: CPU DVFS: out of range %d - %d\n", __func__ , (int)CPU_DVFS_RANGE_TEMP_MIN , (int)CPU_DVFS_TJMAX);
			goto err;
		}
		goto out;
	}
err:
	pr_err("%s: CPU DVFS: invalid cmd\n", __func__);
	return -EINVAL;
out:
	user_cpu_dvfs_max_temp = tmp;
	sanitize_cpu_dvfs(false);
	return count;
}

static ssize_t show_dvfs_bat_down_threshold(struct kobject *kobj, struct attribute *attr, char *buf)
{
	sprintf(buf, "%s[dvfs_bat_down_threshold]\t%d mV\n",buf, dvfs_bat_down_threshold);
	return strlen(buf);
}

static ssize_t store_dvfs_bat_down_threshold(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	int tmp = 0;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	if (sscanf(buf, "%d", &tmp)) {
		if (tmp < DVFS_BAT_THRESHOLD_MIN || tmp > DVFS_BAT_THRESHOLD_MAX) {
			pr_err("%s: CPU DVFS: out of range %d - %d\n", __func__ , (int)DVFS_BAT_THRESHOLD_MIN , (int)DVFS_BAT_THRESHOLD_MAX);
			goto err;
		}
		goto out;
	}
err:
	pr_err("%s: CPU DVFS: invalid cmd\n", __func__);
	return -EINVAL;
out:
	dvfs_bat_down_threshold = tmp;
	sanitize_cpu_dvfs(false);
	return count;
}

static ssize_t show_cpu_dvfs_peak_temp(struct kobject *kobj, struct attribute *attr, char *buf)
{
	sprintf(buf, "%s[peak_temp]\t%u °C\n",buf, cpu_dvfs_peak_temp);
	return strlen(buf);
}

static ssize_t show_dvfs_bat_peak_vol(struct kobject *kobj, struct attribute *attr, char *buf)
{
	sprintf(buf, "%s[dvfs_bat_peak_vol]\t%d mV\n",buf, dvfs_bat_peak_vol);
	return strlen(buf);
}

static ssize_t store_cpu_lit_volt(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	const char id = 3; /* dvfs_cpucl1 */
	unsigned int rate = 0, volt = 0;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	if (sscanf(buf, "%u %u", &rate, &volt) == 2) {
		if ((volt < 450000) || (volt > 1350000))
			goto err;
		update_fvmap(id, rate, volt);
		pr_info("%s: CPU DVFS: update dvfs_cpucl1 - rate: %u kHz - volt: %u uV\n", __func__, rate, volt);
		return count;
	}

err:
	return -EINVAL;
}

static ssize_t store_cpu_big_volt(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	const char id = 2; /* dvfs_cpucl0 */
	unsigned int rate = 0, volt = 0;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	if (sscanf(buf, "%u %u", &rate, &volt) == 2) {
		if ((volt < 450000) || (volt > 1350000))
			goto err;
		update_fvmap(id, rate, volt);
		pr_info("%s: CPU DVFS: update dvfs_cpucl0 - rate: %u kHz - volt: %u uV\n", __func__, rate, volt);
		return count;
	}

err:
	return -EINVAL;
}

static ssize_t store_update_dvfs_table(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	unsigned int id = 0, rate = 0, volt = 0;

#if IS_ENABLED(CONFIG_A2N)
	if (!a2n_allow) {
		pr_err("[%s] a2n: unprivileged access !\n",__func__);
		goto err;
	}
#endif

	if (sscanf(buf, "%u %u %u", &id, &rate, &volt) == 3) {
		update_fvmap(id, rate, volt);
		pr_info("%s: CPU DVFS update id: %u - rate: %u kHz - volt: %u uV\n", __func__, id, rate, volt);
		return count;
	}

err:
	return -EINVAL;
}

static ssize_t store_print_dvfs_table(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	/* print custom DVFS table */
	if (sysfs_streq(buf, "true") || sysfs_streq(buf, "1")) {
		print_fvmap();
		return count;
	}

	return -EINVAL;
}

static inline void set_cpu_dvfs_limit(unsigned int cpu, unsigned int freq)
{
	if (cpu == 4) {
		if (freq > cpu4_max_freq)
			freq = cpu4_max_freq;

		if (cpu4_dvfs_limit == freq)
			return;

		pm_qos_update_request(&cpu_maxlock_cl1, freq);
		cpu4_dvfs_limit = freq;
	} else {
		if (freq > cpu0_max_freq)
			freq = cpu0_max_freq;

		if (cpu0_dvfs_limit == freq)
			return;

		pm_qos_update_request(&cpu_maxlock_cl0, freq);
		cpu0_dvfs_limit = freq;
	}
}

inline void sanitize_cpu_dvfs(bool sanitize)
{
	if (!sanitize) {
		cpu_dvfs_max_temp = user_cpu_dvfs_max_temp;
		cpu_dvfs_peak_temp = 0;
		set_cpu_dvfs_limit(4, cpu4_max_freq);
		set_cpu_dvfs_limit(0, cpu0_max_freq);
		dvfs_bat_up_threshold = (dvfs_bat_down_threshold + DVFS_BAT_THRESHOLD_MARGIN);
	} else {
		cpu_dvfs_max_temp -= CPU_DVFS_STEP_DOWN_TEMP;
	}
	cpu_dvfs_min_temp = (cpu_dvfs_max_temp - CPU_DVFS_MARGIN_TEMP);
}

static inline int cpu_dvfs_check_thread(void *nothing)
{
	unsigned int big_freq = 0, lit_freq = 0;
	static unsigned int prev_temp = 0;
	static int prev_bat_vol = 0;

	while (!kthread_should_stop()) {
		if (!cpu4_max_freq || !cpu0_max_freq) {
			pr_warn("%s: CPU DVFS: cpufreq driver not ready! - Trying again after 500 ms ...\n", __func__);
			msleep(500);
			continue;
		}
		if (cpu_tmu_data == NULL) {
			pr_warn("%s: CPU DVFS: cpu_tmu_data is not ready! - Trying again after 500 ms ...\n", __func__);
			msleep(500);
			continue;
		}
		break;
	}

	sanitize_cpu_dvfs(false);
	cpu4_dvfs_limit = cpu4_max_freq;
	cpu0_dvfs_limit = cpu0_max_freq;
	pr_info("%s: CPU DVFS: kthread started successfully.\n", __func__);

	while (!kthread_should_stop()) {

		cpu_temp = cpu_tmu_data->tmu_read(cpu_tmu_data);

		if ((cpu_temp == prev_temp) && (dvfs_bat_vol == prev_bat_vol)) {
			msleep(cpu_dvfs_sleep_time);
			continue;
		}

		if (cpu_temp > cpu_dvfs_peak_temp) {
			cpu_dvfs_peak_temp = cpu_temp;
#if CPU_DVFS_DEBUG
			pr_info("%s: CPU DVFS: peak_temp: %d C\n", __func__, cpu_dvfs_peak_temp);
#endif
		}

		if (cpu_temp >= CPU_DVFS_SHUTDOWN_TEMP) {
			pr_err("%s: CPU DVFS: CPU_DVFS_SHUTDOWN_TEMP %u C reached! - CUUR_TEMP: %d C ! - cal cpu_dvfs_max_temp: %u C - cpu4_dvfs_limit: %u KHz\n", 
					__func__ , CPU_DVFS_SHUTDOWN_TEMP, cpu_temp, cpu_dvfs_max_temp, cpu4_dvfs_limit);
			sanitize_cpu_dvfs(true);
			big_freq = FREQ_STEP_CL1_0;
			set_cpu_dvfs_limit(4, big_freq);
			pr_err("%s: CPU DVFS: shutting down ...\n", __func__);
			mutex_lock(&poweroff_lock);
			/*
			 * Queue a backup emergency shutdown in the event of
			 * orderly_poweroff failure
			 */
			thermal_emergency_poweroff();
			orderly_poweroff(true);
			mutex_unlock(&poweroff_lock);
			break;
		}

		if (cpu_temp >= CPU_DVFS_AVOID_SHUTDOWN_TEMP) {
			pr_warn("%s: CPU DVFS: CPU_DVFS_AVOID_SHUTDOWN_TEMP %u C reached! - CURR_TEMP: %d C ! - cal cpu_dvfs_max_temp: %u C, calibrating to: %u C ... - cpu4_dvfs_limit: %u KHz\n", 
					__func__ , CPU_DVFS_AVOID_SHUTDOWN_TEMP, cpu_temp, cpu_dvfs_max_temp, (cpu_dvfs_max_temp - CPU_DVFS_STEP_DOWN_TEMP), cpu4_dvfs_limit);
			sanitize_cpu_dvfs(true);
			big_freq = FREQ_STEP_CL1_5;
			goto out;
		}

		if ((cpu_temp >= cpu_dvfs_max_temp) || (dvfs_bat_vol < dvfs_bat_down_threshold)) {
			if (cpu4_dvfs_limit >= FREQ_STEP_CL1_8)
				big_freq = FREQ_STEP_CL1_7;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_7)
				big_freq = FREQ_STEP_CL1_6;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_6)
				big_freq = FREQ_STEP_CL1_5;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_5)
				big_freq = FREQ_STEP_CL1_4;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_4)
				big_freq = FREQ_STEP_CL1_3;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_3)
				big_freq = FREQ_STEP_CL1_2;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_2)
				big_freq = FREQ_STEP_CL1_1;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_1)
				big_freq = FREQ_STEP_CL1_0;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_7)
				lit_freq = FREQ_STEP_CL0_6;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_6)
				lit_freq = FREQ_STEP_CL0_5;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_5)
				lit_freq = FREQ_STEP_CL0_4;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_4)
				lit_freq = FREQ_STEP_CL0_3;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_3)
				lit_freq = FREQ_STEP_CL0_2;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_2)
				lit_freq = FREQ_STEP_CL0_1;
			else
				lit_freq = FREQ_STEP_CL0_0;

		} else if ((cpu_temp <= cpu_dvfs_min_temp) && (dvfs_bat_vol > dvfs_bat_up_threshold)) {
			if (cpu4_dvfs_limit == FREQ_STEP_CL1_0)
				big_freq = FREQ_STEP_CL1_1;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_1)
				big_freq = FREQ_STEP_CL1_2;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_2)
				big_freq = FREQ_STEP_CL1_3;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_3)
				big_freq = FREQ_STEP_CL1_4;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_4)
				big_freq = FREQ_STEP_CL1_5;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_5)
				big_freq = FREQ_STEP_CL1_6;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_6)
				big_freq = FREQ_STEP_CL1_7;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_7)
				big_freq = FREQ_STEP_CL1_8;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_8)
				big_freq = FREQ_STEP_CL1_9;
			else if (cpu4_dvfs_limit == FREQ_STEP_CL1_9)
				big_freq = FREQ_STEP_CL1_10;
			else
				big_freq = FREQ_STEP_CL1_11;

			if (cpu0_dvfs_limit == FREQ_STEP_CL0_0)
				lit_freq = FREQ_STEP_CL0_1;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_1)
				lit_freq = FREQ_STEP_CL0_2;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_2)
				lit_freq = FREQ_STEP_CL0_3;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_3)
				lit_freq = FREQ_STEP_CL0_4;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_4)
				lit_freq = FREQ_STEP_CL0_5;
			else if (cpu0_dvfs_limit == FREQ_STEP_CL0_5)
				lit_freq = FREQ_STEP_CL0_6;
			else
				lit_freq = FREQ_STEP_CL0_7;
		}
out:
		prev_bat_vol = dvfs_bat_vol;
		prev_temp = cpu_temp;
		if (big_freq)
			set_cpu_dvfs_limit(4, big_freq);
		if (lit_freq)
			set_cpu_dvfs_limit(0, lit_freq);
		msleep(cpu_dvfs_sleep_time);
		continue;
	}

	return 0;
}

static struct global_attr cpufreq_table =
__ATTR(cpufreq_table, 0444, show_cpufreq_table, NULL);
static struct global_attr cpufreq_min_limit =
__ATTR(cpufreq_min_limit, 0644,
		show_cpufreq_min_limit, store_cpufreq_min_limit);
static struct global_attr cpufreq_min_limit_wo_boost =
__ATTR(cpufreq_min_limit_wo_boost, 0644,
		show_cpufreq_min_limit, store_cpufreq_min_limit_wo_boost);
static struct global_attr cpu0_min_limit =
__ATTR(user_cpu0_min_limit, 0644,
		show_user_cpu0_min_limit, store_user_cpu0_min_limit);
static struct global_attr cpu4_min_limit =
__ATTR(user_cpu4_min_limit, 0644,
		show_user_cpu4_min_limit, store_user_cpu4_min_limit);
static struct global_attr cpufreq_max_limit =
__ATTR(cpufreq_max_limit, 0644,
		show_cpufreq_max_limit, store_cpufreq_max_limit);
static struct global_attr execution_mode_change =
__ATTR(execution_mode_change, 0644,
		show_execution_mode_change, store_execution_mode_change);
static struct global_attr sysfs_cpu_dvfs_max_temp =
__ATTR(cpu_dvfs_max_temp, 0644,
		show_cpu_dvfs_max_temp, store_cpu_dvfs_max_temp);
static struct global_attr sysfs_dvfs_bat_down_threshold =
__ATTR(dvfs_bat_down_threshold, 0644,
		show_dvfs_bat_down_threshold, store_dvfs_bat_down_threshold);
static struct global_attr sysfs_cpu_dvfs_peak_temp =
__ATTR(cpu_dvfs_peak_temp, 0444,
		show_cpu_dvfs_peak_temp, NULL);
static struct global_attr sysfs_dvfs_bat_peak_vol =
__ATTR(dvfs_bat_peak_vol, 0444,
		show_dvfs_bat_peak_vol, NULL);
static struct global_attr sysfs_cpu_lit_volt =
__ATTR(cpu_lit_volt, 0600,
		NULL, store_cpu_lit_volt);
static struct global_attr sysfs_cpu_big_volt =
__ATTR(cpu_big_volt, 0600,
		NULL, store_cpu_big_volt);
static struct global_attr sysfs_print_dvfs_table =
__ATTR(print_dvfs_table, 0600,
		NULL, store_print_dvfs_table);
static struct global_attr sysfs_update_dvfs_table =
__ATTR(update_dvfs_table, 0600,
		NULL, store_update_dvfs_table);

static void init_sysfs(void)
{
	if (sysfs_create_file(power_kobj, &cpufreq_table.attr))
		pr_err("failed to create cpufreq_table node\n");

	if (sysfs_create_file(power_kobj, &cpufreq_min_limit.attr))
		pr_err("failed to create cpufreq_min_limit node\n");

	if (sysfs_create_file(power_kobj, &cpufreq_min_limit_wo_boost.attr))
		pr_err("failed to create cpufreq_min_limit_wo_boost node\n");

	if (sysfs_create_file(power_kobj, &cpu0_min_limit.attr))
		pr_err("failed to create user_cpu0_min_limit node\n");

	if (sysfs_create_file(power_kobj, &cpu4_min_limit.attr))
		pr_err("failed to create user_cpu4_min_limit node\n");

	if (sysfs_create_file(power_kobj, &cpufreq_max_limit.attr))
		pr_err("failed to create cpufreq_max_limit node\n");

	if (sysfs_create_file(power_kobj, &execution_mode_change.attr))
		pr_err("failed to create execution_mode_change node\n");

	if (sysfs_create_file(power_kobj, &sysfs_cpu_dvfs_max_temp.attr))
		pr_err("CPU DVFS: failed to create cpu_dvfs_max_temp node\n");

	if (sysfs_create_file(power_kobj, &sysfs_dvfs_bat_down_threshold.attr))
		pr_err("CPU DVFS: failed to create dvfs_bat_down_threshold node\n");

	if (sysfs_create_file(power_kobj, &sysfs_cpu_dvfs_peak_temp.attr))
		pr_err("CPU DVFS: failed to create cpu_dvfs_peak_temp node\n");

	if (sysfs_create_file(power_kobj, &sysfs_dvfs_bat_peak_vol.attr))
		pr_err("CPU DVFS: failed to create dvfs_bat_peak_vol node\n");

	if (sysfs_create_file(power_kobj, &sysfs_cpu_lit_volt.attr))
		pr_err("CPU DVFS: failed to create cpu_lit_volt node\n");

	if (sysfs_create_file(power_kobj, &sysfs_cpu_big_volt.attr))
		pr_err("CPU DVFS: failed to create cpu_big_volt node\n");

	if (sysfs_create_file(power_kobj, &sysfs_print_dvfs_table.attr))
		pr_err("CPU DVFS: failed to create print_dvfs_table node\n");

	if (sysfs_create_file(power_kobj, &sysfs_update_dvfs_table.attr))
		pr_err("CPU DVFS: failed to create update_dvfs_table node\n");
}

static int parse_ufc_ctrl_info(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	unsigned int val;

	if (!of_property_read_u32(dn, "user-default-qos", &val))
		domain->user_default_qos = val;

	return 0;
}

static void init_pm_qos(struct exynos_cpufreq_domain *domain)
{
	pm_qos_add_request(&domain->user_min_qos_req,
			domain->pm_qos_min_class, domain->min_freq);

	pm_qos_add_request(&domain->user_min_qos_wo_boost_req,
			domain->pm_qos_min_class, domain->min_freq);
}

int ufc_domain_init(struct exynos_cpufreq_domain *domain)
{
	struct device_node *dn, *child;
	struct cpumask mask;
	const char *buf;

	dn = of_find_node_by_name(NULL, "cpufreq-ufc");

	while ((dn = of_find_node_by_type(dn, "cpufreq-userctrl"))) {
		of_property_read_string(dn, "shared-cpus", &buf);
		cpulist_parse(buf, &mask);
		if (cpumask_intersects(&mask, &domain->cpus)) {
			pr_info("found!\n");
			break;
		}
	}

	for_each_child_of_node(dn, child) {
		struct exynos_ufc *ufc;

		ufc = kzalloc(sizeof(struct exynos_ufc), GFP_KERNEL);
		if (!ufc)
			return -ENOMEM;

		ufc->info.freq_table = kzalloc(sizeof(struct exynos_ufc_freq)
				* domain->table_size, GFP_KERNEL);

		if (!ufc->info.freq_table) {
			kfree(ufc);
			return -ENOMEM;
		}

		list_add_tail(&ufc->list, &domain->ufc_list);
	}

	return 0;
}

static int init_ufc_table_dt(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	struct device_node *child;
	struct exynos_ufc_freq *table;
	struct exynos_ufc *ufc;
	int size, index, c_index;
	int ret;

	ufc = list_entry(&domain->ufc_list, struct exynos_ufc, list);

	pr_info("Initialize ufc table for Domain %d\n", domain->id);

	for_each_child_of_node(dn, child) {

		ufc = list_next_entry(ufc, list);

		if (of_property_read_u32(child, "ctrl-type", &ufc->info.ctrl_type))
			continue;

		if (of_property_read_u32(child, "execution-mode", &ufc->info.exe_mode))
			continue;

		size = of_property_count_u32_elems(child, "table");
		if (size < 0)
			return size;

		table = kzalloc(sizeof(struct exynos_ufc_freq) * size / 2, GFP_KERNEL);
		if (!table)
			return -ENOMEM;

		ret = of_property_read_u32_array(child, "table", (unsigned int *)table, size);
		if (ret)
			return -EINVAL;

		pr_info("Register UFC Type-%d Execution Mode-%d for Domain %d\n",
				ufc->info.ctrl_type, ufc->info.exe_mode, domain->id);

		for (index = 0; index < domain->table_size; index++) {
			unsigned int freq = domain->freq_table[index].frequency;

			if (freq == CPUFREQ_ENTRY_INVALID)
				continue;

			for (c_index = 0; c_index < size / 2; c_index++) {
				if (freq <= table[c_index].master_freq)
					ufc->info.freq_table[index].limit_freq = table[c_index].limit_freq;

				if (freq >= table[c_index].master_freq)
					break;
			}
			pr_info("Master_freq : %u kHz - limit_freq : %u kHz\n",
					ufc->info.freq_table[index].master_freq,
					ufc->info.freq_table[index].limit_freq);
		}
		kfree(table);
	}

	return 0;
}

static int __init exynos_ufc_init(void)
{
	struct device_node *dn = NULL;
	const char *buf;
	struct exynos_cpufreq_domain *domain;
	int ret = 0;

	pm_qos_add_request(&cpu_online_max_qos_req,
			PM_QOS_CPU_ONLINE_MAX,
			PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);

	pm_qos_add_request(&cpu_maxlock_cl0,
			PM_QOS_CLUSTER0_FREQ_MAX,
			PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);

	pm_qos_add_request(&cpu_maxlock_cl1,
			PM_QOS_CLUSTER1_FREQ_MAX,
			PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);

	pm_qos_add_request(&cpu_minlock_cl0,
			PM_QOS_CLUSTER0_FREQ_MIN,
			PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE);

	pm_qos_add_request(&cpu_minlock_cl1,
			PM_QOS_CLUSTER1_FREQ_MIN,
			PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE);

	while ((dn = of_find_node_by_type(dn, "cpufreq-userctrl"))) {
		struct cpumask shared_mask;

		ret = of_property_read_string(dn, "shared-cpus", &buf);
		if (ret) {
			pr_err("failed to get shared-cpus for ufc\n");
			goto exit;
		}

		cpulist_parse(buf, &shared_mask);
		domain = find_domain_cpumask(&shared_mask);
		if (!domain) {
			pr_err("Can't found domain for ufc!\n");
			goto exit;
		}

		/* Initialize user control information from dt */
		ret = parse_ufc_ctrl_info(domain, dn);
		if (ret) {
			pr_err("failed to get ufc ctrl info\n");
			goto exit;
		}

		/* Parse user frequency ctrl table info from dt */
		ret = init_ufc_table_dt(domain, dn);
		if (ret) {
			pr_err("failed to parse frequency table for ufc ctrl\n");
			goto exit;
		}
		/* Initialize PM QoS */
		init_pm_qos(domain);
		pr_info("Complete to initialize domain%d\n", domain->id);
	}

	init_sysfs();

	mutex_init(&poweroff_lock);

	cpu_dvfs_thread = kthread_run(cpu_dvfs_check_thread, NULL, "cpu_dvfsd");
	if (IS_ERR(cpu_dvfs_thread)) {
		pr_err("%s: CPU DVFS: failed to create and start kthread.", __func__);
		ret = -EINVAL;
		goto exit;
	}

#ifdef CONFIG_SCHED_HMP_CUSTOM
	set_cpus_allowed_ptr(cpu_dvfs_thread, &hmp_slow_cpu_mask);
#else
	set_cpus_allowed_ptr(cpu_dvfs_thread, cpu_all_mask);
#endif

	set_user_nice(cpu_dvfs_thread, MIN_NICE);

	pr_info("Initialized Exynos UFC(User-Frequency-Ctrl) driver\n");

	return 0;

exit:
	mutex_destroy(&poweroff_lock);
	return ret;
}
late_initcall(exynos_ufc_init);
