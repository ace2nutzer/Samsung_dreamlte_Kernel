#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <soc/samsung/cal-if.h>

#include "fvmap.h"
#include "cmucal.h"
#include "vclk.h"
#include "ra.h"
#include "acpm_dvfs.h"

#define FVMAP_SIZE		(SZ_8K)

static void __iomem *fvmap_base = NULL;
static void __iomem *sram_fvmap_base = NULL;

static int init_margin_table[10];

static int set_mif_volt = 0;
static int set_int_volt = 0;
static int set_cpucl0_volt = 0;
static int set_cpucl1_volt = 0;
static int set_g3d_volt = 0;
static int set_intcam_volt = 0;
static int set_cam_volt = 0;
static int set_disp_volt = 0;
static int set_g3dm_volt = 0;
static int set_cp_volt = 0;

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	init_margin_table[0] = set_mif_volt;
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	init_margin_table[1] = set_int_volt;
	return 0;
}
early_param("int", get_int_volt);

static int __init get_cpucl0_volt(char *str)
{
	get_option(&str, &set_cpucl0_volt);
	init_margin_table[2] = set_cpucl0_volt;
	return 0;
}
early_param("big", get_cpucl0_volt);

static int __init get_cpucl1_volt(char *str)
{
	get_option(&str, &set_cpucl1_volt);
	init_margin_table[3] = set_cpucl1_volt;
	return 0;
}
early_param("lit", get_cpucl1_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	init_margin_table[4] = set_g3d_volt;
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_intcam_volt(char *str)
{
	get_option(&str, &set_intcam_volt);
	init_margin_table[5] = set_intcam_volt;
	return 0;
}
early_param("intcam", get_intcam_volt);

static int __init get_cam_volt(char *str)
{
	get_option(&str, &set_cam_volt);
	init_margin_table[6] = set_cam_volt;
	return 0;
}
early_param("cam", get_cam_volt);

static int __init get_disp_volt(char *str)
{
	get_option(&str, &set_disp_volt);
	init_margin_table[7] = set_disp_volt;
	return 0;
}
early_param("disp", get_disp_volt);

static int __init get_g3dm_volt(char *str)
{
	get_option(&str, &set_g3dm_volt);
	init_margin_table[8] = set_g3dm_volt;
	return 0;
}
early_param("g3dm", get_g3dm_volt);

static int __init get_cp_volt(char *str)
{
	get_option(&str, &set_cp_volt);
	init_margin_table[9] = set_cp_volt;
	return 0;
}
early_param("cp", get_cp_volt);

static int fvmap_set_raw_voltage_table(unsigned int id, unsigned int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int num_of_lv;
	int idx, i;

	if (!IS_ACPM_VCLK(id)) {
		pr_warn("%s: dvfs_id: %u is not ACPM_VCLK! - return ...\n", __func__, ACPM_VCLK_TYPE | id);
		return 0;
	}

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++) {
		fv_table->table[i].volt = uV;
		pr_info("%s: dvfs_id: %u - rate %u - volt %u\n", __func__, ACPM_VCLK_TYPE | id, fv_table->table[i].rate, uV);
	}

	return 0;
}

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx = 0, i = 0;
	int num_of_lv = 0;

	if (!IS_ACPM_VCLK(id)) {
		pr_warn("%s: dvfs_id: %u is not ACPM_VCLK! - return ...\n", __func__, ACPM_VCLK_TYPE | id);
		return 0;
	}

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++) {
		table[i] = fv_table->table[i].volt;
		pr_info("%s: dvfs_id: %u - rate %u - volt %u\n", __func__, ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);
	}

	return num_of_lv;

}

static int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	if (!IS_ACPM_VCLK(id)) {
		pr_warn("%s: dvfs_id: %u is not ACPM_VCLK! - return ...\n", __func__, ACPM_VCLK_TYPE | id);
		return 0;
	}

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++) {
		table[i] = fv_table->table[i].volt;
		pr_info("%s: dvfs_id: %u - rate %u - volt %u\n", __func__, ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);
	}

	return 0;
}

void print_fvmap(void)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *rvh;
	struct vclk *vclk;
	int size = 0;
	int i = 0, j = 0;

	pr_info("#################\n");
	pr_info("CUSTOM DVFS TABLE\n");
	pr_info("#################\n");

	fvmap_header = fvmap_base;
	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
		pr_info("dvfs_type : %s - id : %x\n",
				vclk->name, fvmap_header[i].dvfs_type);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);

		rvh = fvmap_base + fvmap_header[i].o_ratevolt;

		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			pr_info("  lv : [%7d], volt = %d uV\n",
					rvh->table[j].rate, rvh->table[j].volt);
		}
	}
}

void update_fvmap(int id, int rate, int volt)
{
	struct fvmap_header *fvmap_header, *raw_header;
	struct rate_volt_header *raw_rvh, *rvh;
	struct vclk *vclk;
	int size = 0;
	int i = 0, j = 0;

	fvmap_header = fvmap_base;
	raw_header = sram_fvmap_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;

		raw_rvh = sram_fvmap_base + fvmap_header[i].o_ratevolt;
		rvh = fvmap_base + fvmap_header[i].o_ratevolt;

		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			/* update voltage table */
			if ((fvmap_header[i].dvfs_type == id) && (raw_rvh->table[j].rate == rate)) {
				raw_rvh->table[j].volt = volt;
				/* copy to fvmap */
				rvh->table[j].rate = raw_rvh->table[j].rate;
				rvh->table[j].volt = raw_rvh->table[j].volt;
				break;
			}
		}
	}
}

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	struct fvmap_header *fvmap_header, *raw_header;
	struct rate_volt_header *raw_rvh, *rvh;
	struct clocks *clks;
	struct pll_header *plls;
	struct vclk *vclk;
	struct cmucal_clk *clk_node;
	unsigned int paddr_offset = 0, fvaddr_offset = 0;
	int size = 0;
	int i = 0, j = 0;
	unsigned int mif_max_freq = 0, int_max_freq = 0, disp_max_freq = 0;

	fvmap_header = map_base;
	raw_header = sram_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	pr_info("################\n");
	pr_info("STOCK DVFS TABLE\n");
	pr_info("################\n");

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].dvfs_type = raw_header[i].dvfs_type;
		fvmap_header[i].num_of_lv = raw_header[i].num_of_lv;
		fvmap_header[i].num_of_members = raw_header[i].num_of_members;
		fvmap_header[i].num_of_pll = raw_header[i].num_of_pll;
		fvmap_header[i].num_of_mux = raw_header[i].num_of_mux;
		fvmap_header[i].num_of_div = raw_header[i].num_of_div;
		fvmap_header[i].gearratio = raw_header[i].gearratio;
		fvmap_header[i].init_lv = raw_header[i].init_lv;
		fvmap_header[i].num_of_gate = raw_header[i].num_of_gate;
		fvmap_header[i].reserved[0] = raw_header[i].reserved[0];
		fvmap_header[i].reserved[1] = raw_header[i].reserved[1];
		fvmap_header[i].block_addr[0] = raw_header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = raw_header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = raw_header[i].block_addr[2];
		fvmap_header[i].o_members = raw_header[i].o_members;
		fvmap_header[i].o_ratevolt = raw_header[i].o_ratevolt;
		fvmap_header[i].o_tables = raw_header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;

		pr_info("dvfs_type : %s - id : %x\n",
				vclk->name, fvmap_header[i].dvfs_type);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);

		raw_rvh = sram_base + fvmap_header[i].o_ratevolt;
		rvh = map_base + fvmap_header[i].o_ratevolt;
		if (init_margin_table[i])
			cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE,
						init_margin_table[i]);

		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			rvh->table[j].rate = raw_rvh->table[j].rate;
			rvh->table[j].volt = raw_rvh->table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV\n",
					rvh->table[j].rate, rvh->table[j].volt);

			/* hardcoded g3d voltages */
			if (strcmp(vclk->name, "dvfs_g3d") == 0) {
				if (raw_rvh->table[j].rate == 683000)
					raw_rvh->table[j].volt = 750000;
				else if (raw_rvh->table[j].rate == 764000)
					raw_rvh->table[j].volt = 800000;
				else if (raw_rvh->table[j].rate == 839000)
					raw_rvh->table[j].volt = 850000;
			}

			/* hardcoded cpucl1 voltages */
			if (strcmp(vclk->name, "dvfs_cpucl1") == 0) {
				vclk->boot_freq = vclk->max_freq;
				if (raw_rvh->table[j].rate == 1794000)
					raw_rvh->table[j].volt = 1100000;
				else if (raw_rvh->table[j].rate == 1898000)
					raw_rvh->table[j].volt = 1150000;
				else if (raw_rvh->table[j].rate == 2002000)
					raw_rvh->table[j].volt = 1250000;
			}

			/* hardcoded cpucl0 voltages */
			if (strcmp(vclk->name, "dvfs_cpucl0") == 0) {
				vclk->boot_freq = vclk->max_freq;
				if (raw_rvh->table[j].rate == 2496000)
					raw_rvh->table[j].volt = 1050000;
				else if (raw_rvh->table[j].rate == 2652000)
					raw_rvh->table[j].volt = 1125000;
				else if (raw_rvh->table[j].rate == 2704000)
					raw_rvh->table[j].volt = 1150000;
				else if (raw_rvh->table[j].rate == 2808000)
					raw_rvh->table[j].volt = 1250000;
			}

			/* patch mif for devfreq */
			if (strcmp(vclk->name, "dvfs_mif") == 0) {
				if ((raw_rvh->table[j].volt) && (!mif_max_freq)) {
					mif_max_freq = raw_rvh->table[j].rate;
					vclk->max_freq = mif_max_freq;
					vclk->boot_freq = mif_max_freq;
				}
				/* hardcoded mif voltages */
				if ((raw_rvh->table[j].rate == 2002000) && (!raw_rvh->table[j].volt))
					raw_rvh->table[j].volt = 800000;
				else if ((raw_rvh->table[j].rate == 2093000) && (!raw_rvh->table[j].volt))
					raw_rvh->table[j].volt = 850000;
			}

			/* patch int for devfreq */
			if (strcmp(vclk->name, "dvfs_int") == 0) {
				if ((raw_rvh->table[j].volt) && (!int_max_freq)) {
					int_max_freq = raw_rvh->table[j].rate;
					vclk->max_freq = int_max_freq;
					vclk->boot_freq = int_max_freq;
				}
			}

			/* patch disp for devfreq */
			if (strcmp(vclk->name, "dvfs_disp") == 0) {
				if ((raw_rvh->table[j].volt) && (!disp_max_freq)) {
					disp_max_freq = raw_rvh->table[j].rate;
					vclk->max_freq = disp_max_freq;
				}
				vclk->min_freq = raw_rvh->table[j].rate;
				vclk->boot_freq = raw_rvh->table[j].rate;
			}

			/* copy to fvmap */
			rvh->table[j].volt = raw_rvh->table[j].volt;
		}

		for (j = 0; j < fvmap_header[i].num_of_pll; j++) {
			clks = sram_base + fvmap_header[i].o_members;
			plls = sram_base + clks->addr[j];
			clk_node = cmucal_get_node(vclk->list[j]);

			if (clk_node == NULL)
				continue;

			paddr_offset = clk_node->paddr & 0xFFFF;
			fvaddr_offset = plls->addr & 0xFFFF;
			if (paddr_offset == fvaddr_offset)
				continue;

			clk_node->paddr += fvaddr_offset - paddr_offset;
			clk_node->pll_con0 += fvaddr_offset - paddr_offset;
			if (clk_node->pll_con1)
				clk_node->pll_con1 += fvaddr_offset - paddr_offset;
		}

	}
	/* print custom dvfs table in kmsg */
	print_fvmap();
}

int fvmap_init(void __iomem *sram_base)
{
	fvmap_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %pK\n", __func__, sram_base);
	fvmap_copy_from_sram(fvmap_base, sram_base);

	if (IS_ENABLED(CONFIG_VDD_AUTO_CAL))
		exynos_acpm_vdd_auto_calibration(1);

	return 0;
}
