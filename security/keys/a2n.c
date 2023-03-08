/*
 * Copyright (C) 2021 ace2nutzer <ace2nutzer@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

static unsigned int a2n = 0;
bool a2n_allow = false;
extern unsigned int bootmode;
extern unsigned int lpcharge;

static int set_a2n_allow(const char *buf, struct kernel_param *kp)
{
	unsigned int temp = 0;

	sscanf(buf, "%u", &temp);

	if ((temp == 1) && ((bootmode == 2) || (lpcharge))) {
		a2n_allow = true;
		pr_info("[%s] a2n: welcome !\n",__func__);
		return 0;
	}

	if ((temp == a2n) && (a2n_allow))
		return 0;

	if (temp == a2n) {
		a2n_allow = true;
		pr_info("[%s] a2n: welcome !\n",__func__);
	} else if (temp == 0) {
		a2n_allow = false;
		pr_info("[%s] a2n: bye bye !\n",__func__);
	} else {
		pr_warn("[%s] a2n: wrong input !\n",__func__);
		return -EINVAL;
	}

	return 0;
}
module_param_call(a2n_allow, set_a2n_allow, param_get_bool, &a2n_allow, 0644);

static int __init a2n_init(void)
{
	a2n = 1;
	a2n_allow = false;
	pr_info("%s: initialized\n", __func__);
	return 0;
}
module_init(a2n_init);

static void __exit a2n_exit(void)
{
	a2n = 0;
	a2n_allow = false;
	pr_info("%s: bye bye\n", __func__);
}
module_exit(a2n_exit);

MODULE_AUTHOR("ace2nutzer <ace2nutzer@gmail.com>");
MODULE_DESCRIPTION("A2N module");
MODULE_LICENSE("GPL v2");
