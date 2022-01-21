/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_MM_H
#define _LINUX_SCHED_MM_H

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/gfp.h>

/*
 * This has to be called after a get_task_mm()/mmget_not_zero()
 * followed by taking the mmap_sem for writing before modifying the
 * vmas or anything the coredump pretends not to change from under it.
 *
 * NOTE: find_extend_vma() called from GUP context is the only place
 * that can modify the "mm" (notably the vm_start/end) under mmap_sem
 * for reading and outside the context of the process, so it is also
 * the only case that holds the mmap_sem for reading that must call
 * this function. Generally if the mmap_sem is hold for reading
 * there's no need of this check after get_task_mm()/mmget_not_zero().
 *
 * This function can be obsoleted and the check can be removed, after
 * the coredump code will hold the mmap_sem for writing before
 * invoking the ->core_dump methods.
 */
static inline bool mmget_still_valid(struct mm_struct *mm)
{
	return likely(!mm->core_state);
}

#endif /* _LINUX_SCHED_MM_H */
