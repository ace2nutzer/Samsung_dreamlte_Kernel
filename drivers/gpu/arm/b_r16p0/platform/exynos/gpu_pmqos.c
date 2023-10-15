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

#if defined(CONFIG_HMP_VARIABLE_SCALE)
extern int set_hmp_boost(int enable);
#endif

struct pm_qos_request exynos5_g3d_mif_min_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster0_min_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster1_min_qos;

extern struct kbase_device *pkbdev;
static bool gpu_pmqos_ongoing = false;

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
		break;
	case GPU_CONTROL_PM_QOS_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> set\n", __func__);
			return -ENOENT;
		}
		KBASE_DEBUG_ASSERT(platform->step >= 0);
#ifdef CONFIG_MALI_VK_BOOST /* VK JOB Boost */
		mutex_lock(&platform->gpu_sched_hmp_lock);
		mutex_lock(&platform->gpu_vk_boost_lock);
		if ((platform->ctx_need_qos || platform->ctx_vk_need_qos || (pkbdev->pm.backend.metrics.is_full_compute_util)) && (!gpu_pmqos_ongoing)) {
#if defined(CONFIG_HMP_VARIABLE_SCALE)
			set_hmp_boost(1);
			set_hmp_aggressive_up_migration(true);
			set_hmp_aggressive_yield(true);
#endif
			if (platform->cl_boost) {
				gpu_dvfs_boost_lock(GPU_DVFS_BOOST_SET);
				pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->gpu_vk_boost_mif_min_clk_lock);
				if (platform->cl_boost == 2) {
					pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, platform->table[platform->step].cpu_little_min_freq);
					pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->table[platform->step].cpu_big_min_freq);
				}
			} else {
				pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
			}
			gpu_pmqos_ongoing = true;
		}
		mutex_unlock(&platform->gpu_vk_boost_lock);
		mutex_unlock(&platform->gpu_sched_hmp_lock);
#endif
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> reset\n", __func__);
			return -ENOENT;
		}
		mutex_lock(&platform->gpu_sched_hmp_lock);
		mutex_lock(&platform->gpu_vk_boost_lock);
		if (!platform->ctx_need_qos && !platform->ctx_vk_need_qos && (!pkbdev->pm.backend.metrics.is_full_compute_util) && gpu_pmqos_ongoing) {
			gpu_dvfs_boost_lock(GPU_DVFS_BOOST_UNSET);
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
			pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, 0);
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
#ifdef CONFIG_HMP_VARIABLE_SCALE
			/* unset hmp boost */
			set_hmp_boost(0);
			set_hmp_aggressive_up_migration(false);
			set_hmp_aggressive_yield(false);
#endif
			gpu_pmqos_ongoing = false;
		}
		mutex_unlock(&platform->gpu_vk_boost_lock);
		mutex_unlock(&platform->gpu_sched_hmp_lock);
		break;
	case GPU_CONTROL_PM_QOS_EGL_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_set\n", __func__);
			return -ENOENT;
		}
		break;
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_set\n", __func__);
			return -ENOENT;
		}
		break;
	default:
		break;
	}

	return 0;
}
#endif
