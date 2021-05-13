/*
 * Copyright (C) 2021 ace2nutzer <ace2nutzer@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

unsigned int a2n = 0;
bool a2n_allow = false;

static int __init a2n_init(void)
{
	a2n = 1;
	pr_info("%s: success\n", __func__);
	return 0;
}
static void __exit a2n_exit(void)
{
	a2n = 0;
	a2n_allow = false;
	pr_info("%s: bye bye\n", __func__);
}

module_init(a2n_init);
module_exit(a2n_exit);

MODULE_AUTHOR("ace2nutzer <ace2nutzer@gmail.com>");
MODULE_DESCRIPTION("A2N module");
MODULE_LICENSE("GPL v2");
