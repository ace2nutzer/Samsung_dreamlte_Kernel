/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#include <linux/cpufreq.h>

#define EXYNOS_UFC_TYPE_NAME_LEN	16

enum exynos_ufc_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MIN_WO_BOOST_LIMIT,
	PM_QOS_MAX_LIMIT,
	TYPE_END,
};

enum exynos_ufc_execution_mode {
	AARCH64_MODE = 0,
	AARCH32_MODE,
	MODE_END,
};

struct exynos_ufc_freq {
	u32			master_freq;
	u32			limit_freq;
};

struct exynos_ufc_info {
	struct list_head	node;
	int			ctrl_type;
	int			exe_mode;

	struct exynos_ufc_freq *freq_table;
};

extern void update_fvmap(int id, int rate, int volt);
extern void print_fvmap(void);

/* GPU stuff */
#include "../gpu/arm/b_r16p0/platform/exynos/mali_kbase_platform.h"
#include "../gpu/arm/b_r16p0/platform/exynos/gpu_dvfs_handler.h"
extern struct exynos_context *platform;
extern int dvfs_get_dev_vol(void);
extern int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock);

