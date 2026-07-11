/*
 * Copyright (C) 2021 <ace2nutzer @ xda>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/a2n.h>

static unsigned int a2n = 0;
bool a2n_allow = false;

static int set_a2n_allow(const char *buf, struct kernel_param *kp)
{
	unsigned int temp = 0;

	sscanf(buf, "%u", &temp);

	if ((temp == 1) && ((bootmode == 2) || (lpcharge))) {
		a2n_allow = true;
		return 0;
	}

	if ((temp == a2n) && (a2n_allow))
		return 0;

	if (temp == a2n)
		a2n_allow = true;
	else if (temp == 0)
		a2n_allow = false;
	else
		return -EINVAL;

	return 0;
}
module_param_call(a2n_allow, set_a2n_allow, param_get_bool, &a2n_allow, 0644);

static int __init a2n_init(void)
{
	a2n = 1;
	a2n_allow = false;
	return 0;
}
module_init(a2n_init);

static void __exit a2n_exit(void)
{
	a2n_allow = false;
}
module_exit(a2n_exit);

MODULE_AUTHOR("<ace2nutzer @ xda>");
MODULE_DESCRIPTION("A2N module");
MODULE_LICENSE("GPL v2");
