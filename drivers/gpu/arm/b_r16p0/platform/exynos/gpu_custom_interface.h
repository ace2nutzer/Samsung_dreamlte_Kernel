/* drivers/gpu/arm/.../platform/gpu_custom_interface.h
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
 * @file gpu_custom_interface.h
 * DVFS
 */

#ifndef _GPU_CUSTOM_INTERFACE_H_
#define _GPU_CUSTOM_INTERFACE_H_

int gpu_pmqos_dvfs_min_lock(int level);
#ifdef CONFIG_MALI_DEBUG_SYS
int gpu_create_sysfs_file(struct device *dev);
void gpu_remove_sysfs_file(struct device *dev);
#endif /* CONFIG_MALI_DEBUG_SYS */

extern void kbasep_trace_format_msg(struct kbase_trace *trace_msg, char *buffer, int len);

extern void update_fvmap(int id, int rate, int volt);
extern struct kbase_device *pkbdev;
extern bool is_suspend;
extern unsigned int dvfs_sleep_time_us;

/* DVFS device low voltage handler */
extern int dvfs_dev_low_vol_peak;
extern int dvfs_dev_low_vol_trig;
extern unsigned int gpu_dvfs_limit_freq_vol;
extern void sanitize_cpu_gpu_dvfs_vol(void);

#endif /* _GPU_CUSTOM_INTERFACE_H_ */
