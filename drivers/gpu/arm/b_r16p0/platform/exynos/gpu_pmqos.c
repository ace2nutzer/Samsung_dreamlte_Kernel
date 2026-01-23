/* drivers/gpu/arm/.../platform/gpu_pmqos.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_pmqos.c
 * DVFS
 */

#include <linux/pm_qos.h>

#include <mali_kbase.h>
#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"

extern int set_hmp_boost(int enable);
extern struct kbase_device *pkbdev;

static struct pm_qos_request exynos5_g3d_mif_min_qos;
static struct pm_qos_request exynos5_g3d_cpu_cluster0_min_qos;
static struct pm_qos_request exynos5_g3d_cpu_cluster1_min_qos;

static bool gpu_pmqos_ongoing = false;
static bool mif_boost_ongoing = false;

#ifdef CONFIG_MALI_PM_QOS
int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	int idx = 0;

	DVFS_ASSERT(platform);
	DVFS_ASSERT(pkbdev);

#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	if (platform->gpu_auto_cali_status)
		return 0;
#endif

	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		if (!platform->is_pm_qos_init) {
			pm_qos_add_request(&exynos5_g3d_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
			pm_qos_add_request(&exynos5_g3d_cpu_cluster0_min_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
			pm_qos_add_request(&exynos5_g3d_cpu_cluster1_min_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
			platform->is_pm_qos_init = true;
		}
		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		if (platform->is_pm_qos_init) {
			pm_qos_remove_request(&exynos5_g3d_mif_min_qos);
			pm_qos_remove_request(&exynos5_g3d_cpu_cluster0_min_qos);
			pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_min_qos);
			platform->is_pm_qos_init = false;
		}
		break;
	case GPU_CONTROL_PM_QOS_SET:
	case GPU_CONTROL_PM_QOS_EGL_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> set\n", __func__);
			return -ENOENT;
		}
		KBASE_DEBUG_ASSERT(platform->step >= 0);
		if (!mif_boost_ongoing)
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
		if ((platform->ctx_need_qos || platform->ctx_vk_need_qos || (pkbdev->pm.backend.metrics.is_full_compute_util)) && (!gpu_pmqos_ongoing)) {
			mutex_lock(&platform->gpu_sched_hmp_lock);
			set_hmp_boost(1);
			set_hmp_aggressive_up_migration(true);
			set_hmp_aggressive_yield(true);
			mutex_unlock(&platform->gpu_sched_hmp_lock);
			if (platform->cl_boost) {
				pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->gpu_vk_boost_mif_min_clk_lock);
				mif_boost_ongoing = true;
				if (platform->cl_boost == 2) {
					pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
					pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
				}
			} else {
				mif_boost_ongoing = false;
			}
			gpu_pmqos_ongoing = true;
		}
		break;
	case GPU_CONTROL_PM_QOS_RESET:
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> reset\n", __func__);
			return -ENOENT;
		}
		if (gpu_pmqos_ongoing) {
			pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, 0);
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
			/* unset hmp boost */
			mutex_lock(&platform->gpu_sched_hmp_lock);
			set_hmp_boost(0);
			set_hmp_aggressive_up_migration(false);
			set_hmp_aggressive_yield(false);
			mutex_unlock(&platform->gpu_sched_hmp_lock);
			gpu_pmqos_ongoing = false;
			mif_boost_ongoing = false;
		}
		break;
	default:
		break;
	}

	return 0;
}
#endif
