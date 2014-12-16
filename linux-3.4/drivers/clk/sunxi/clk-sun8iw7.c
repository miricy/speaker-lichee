/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/clk-private.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/clk/sunxi.h>
#include <mach/sys_config.h>
#include "clk-sunxi.h"
#include "clk-factors.h"
#include "clk-periph.h"
#include "clk-sun8iw7.h"
#include "clk-sun8iw7_tbl.c"

#ifndef CONFIG_EVB_PLATFORM
    #define LOCKBIT(x) 31
#else
    #define LOCKBIT(x) x
#endif
static DEFINE_SPINLOCK(clk_lock);
void __iomem *sunxi_clk_base;
void __iomem *sunxi_clk_cpus_base=0;
int	sunxi_clk_maxreg =SUNXI_CLK_MAX_REG;
int cpus_clk_maxreg = 0;
#ifdef CONFIG_SUNXI_CLK_DUMMY_DEBUG
unsigned int dummy_reg[1024];
unsigned int dummy_readl(unsigned int* regaddr)
{
	unsigned int val;
	val = *regaddr;
	printk("--%s-- dummy_readl to read reg 0x%x with val 0x%x\n",__func__,((unsigned int)regaddr - (unsigned int)&dummy_reg[0]),val);
	return val;
}
void  dummy_writel(unsigned int val,unsigned int* regaddr)
	{
		*regaddr = val;
		printk("--%s-- dummy_writel to write reg 0x%x with val 0x%x\n",__func__,((unsigned int)regaddr - (unsigned int)&dummy_reg[0]),val);
	}

void dummy_reg_init(void)
{
	memset(dummy_reg,0x0,sizeof(dummy_reg));
	dummy_reg[PLL1_CFG/4]=0x00001000;
	dummy_reg[PLL2_CFG/4]=0x00035514;
	dummy_reg[PLL3_CFG/4]=0x03006207;
	dummy_reg[PLL4_CFG/4]=0x03006207;
	dummy_reg[PLL5_CFG/4]=0x00001000;
	dummy_reg[PLL6_CFG/4]=0x00041811;
	dummy_reg[PLL8_CFG/4]=0x03006207;
	dummy_reg[PLL9_CFG/4]=0x03001300;
	dummy_reg[PLL10_CFG/4]=0x03006207;
	dummy_reg[CPU_CFG/4]=0x00001000;
	dummy_reg[AHB1_CFG/4]=0x00001010;
	dummy_reg[APB2_CFG/4]=0x01000000;
	dummy_reg[ATS_CFG/4]=0x80000000;
	dummy_reg[PLL_LOCK/4]=0x000000FF;
	dummy_reg[CPU_LOCK/4]=0x000000FF;
}
#endif // of CONFIG_SUNXI_CLK_DUMMY_DEBUG

// media means: video/ve/gpu/hsic/de
/*                            ns  nw  ks  kw  ms  mw  ps  pw  d1s  d1w  d2s  d2w  {frac  out  mode}  en-s   sdmss  sdmsw  sdmpat      sdmval*/
SUNXI_CLK_FACTORS(pll_cpu,    8,  5,  4,  2,  0,  2,  16, 2,  0,   0,   0,   0,    0,    0,   0,     31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_audio,  8,  7,  0,  0,  0,  5,  16, 4,  0,   0,   0,   0,    0,    0,   0,     31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_video,  8,  7,  0,  0,  0,  4,  0,  0,  0,   0,   0,   0,    1,    25,  24,    31,   24,     0,       PLL_VIDEOPAT,0xd1303333);
SUNXI_CLK_FACTORS(pll_ve,     8,  7,  0,  0,  0,  4,  0,  0,  0,   0,   0,   0,    1,    25,  24,    31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_ddr,    8,  5,  4,  2,  0,  2,  0,  0,  0,   0,   0,   0,    0,    0,   0,     31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_periph0,8,  5,  4,  2,  0,  0,  0,  0,  0,   0,   0,   0,    0,    0,   0,     31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_gpu,    8,  7,  0,  0,  0,  4,  0,  0,  0,   0,   0,   0,    1,    25,  24,    31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_periph1,8,  5,  4,  2,  0,  0,  0,  0,  0,   0,   0,   0,    0,    0,   0,     31,    0,     0,       0,        0);
SUNXI_CLK_FACTORS(pll_de,     8,  7,  0,  0,  0,  4,  0,  0,  0,   0,   0,   0,    1,    25,  24,    31,    0,     0,       0,        0);


static int get_factors_pll_cpu(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
   int index;
    u64 tmp_rate;
    if(!factor)
        return -1;
    tmp_rate = rate>pllcpu_max ? pllcpu_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
 		if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_cpu,factor, factor_pllcpu_tbl,index,sizeof(factor_pllcpu_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    return 0;
}

static int get_factors_pll_ddr(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
	int index;
	u64 tmp_rate;
	if (!factor)
		return -1;
	tmp_rate = rate > pllddr_max ? pllddr_max : rate;
	do_div(tmp_rate, 1000000);
	index = tmp_rate;
	if (sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_ddr, factor,
					 factor_pllddr_tbl, index,
					 sizeof(factor_pllddr_tbl)/sizeof(struct sunxi_clk_factor_freq)))
		return -1;

	return 0;
}
static int get_factors_pll_periph0(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
   int index;
    u64 tmp_rate;
    if(!factor)
        return -1;
    tmp_rate = rate>pllperiph0_max ? pllperiph0_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
        if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_periph0,factor, factor_pllperiph0_tbl,index,sizeof(factor_pllperiph0_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    return 0;
}
static int get_factors_pll_periph1(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
   int index;
    u64 tmp_rate;
    if(!factor)
        return -1;
    tmp_rate = rate>pllperiph1_max ? pllperiph1_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
        if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_periph1,factor, factor_pllperiph1_tbl,index,sizeof(factor_pllperiph1_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    return 0;
}
static int get_factors_pll_audio(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
    if(rate == 22579200) {
        factor->factorn = 78;
        factor->factorm = 20;
        factor->factorp = 3;
    } else if(rate == 24576000) {
        factor->factorn = 85;
        factor->factorm = 20;
        factor->factorp = 3;
    } else
        return -1;

    return 0;
}

static int get_factors_pll_video(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate;
    int index;
    if(!factor)
        return -1;

    tmp_rate = rate>pllvideo_max ? pllvideo_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
 		if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_video,factor, factor_pllvideo_tbl,index,sizeof(factor_pllvideo_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    if(rate == 297000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 1;
        factor->factorm = 0;
    }
    else if(rate == 270000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 0;
        factor->factorm = 0;
    } else {
        factor->frac_mode = 1;
        factor->frac_freq = 0;
    }

    return 0;
}
static int get_factors_pll_ve(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate;
    int index;
    if(!factor)
        return -1;

    tmp_rate = rate>pllve_max ? pllve_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
 		if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_ve,factor, factor_pllve_tbl,index,sizeof(factor_pllve_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    if(rate == 297000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 1;
        factor->factorm = 0;
    }
    else if(rate == 270000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 0;
        factor->factorm = 0;
    } else {
        factor->frac_mode = 1;
        factor->frac_freq = 0;
    }

    return 0;
}
static int get_factors_pll_gpu(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate;
    int index;
    if(!factor)
        return -1;

    tmp_rate = rate>pllgpu_max ? pllgpu_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
 		if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_gpu,factor, factor_pllgpu_tbl,index,sizeof(factor_pllgpu_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    if(rate == 297000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 1;
        factor->factorm = 0;
    }
    else if(rate == 270000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 0;
        factor->factorm = 0;
    } else {
        factor->frac_mode = 1;
        factor->frac_freq = 0;
    }

    return 0;
}
static int get_factors_pll_de(u32 rate, u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate;
    int index;
    if(!factor)
        return -1;

    tmp_rate = rate>pllde_max ? pllde_max : rate;
    do_div(tmp_rate, 1000000);
    index = tmp_rate;
 		if(sunxi_clk_get_common_factors_search(&sunxi_clk_factor_pll_de,factor, factor_pllde_tbl,index,sizeof(factor_pllde_tbl)/sizeof(struct sunxi_clk_factor_freq)))
 			return -1;
    if(rate == 297000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 1;
        factor->factorm = 0;
    }
    else if(rate == 270000000) {
        factor->frac_mode = 0;
        factor->frac_freq = 0;
        factor->factorm = 0;
    } else {
        factor->frac_mode = 1;
        factor->frac_freq = 0;
    }

    return 0;
}

static unsigned long calc_rate_pll_cpu(u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate?parent_rate:24000000);
    tmp_rate = tmp_rate * (factor->factorn+1) * (factor->factork+1);
    do_div(tmp_rate, (factor->factorm+1) * (1 << factor->factorp));
    return (unsigned long)tmp_rate;
}

static unsigned long calc_rate_pll_audio(u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate?parent_rate:24000000);
    if((factor->factorn == 78) && (factor->factorm == 20) && (factor->factorp == 3))
        return 22579200;
    else if((factor->factorn == 85) && (factor->factorm == 20) && (factor->factorp == 3))
        return 24576000;
    else
    {
        tmp_rate = tmp_rate * (factor->factorn+1);
        do_div(tmp_rate, (factor->factorm+1) * (factor->factorp+1));
        return (unsigned long)tmp_rate;
    }
}

static unsigned long calc_rate_media(u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate?parent_rate:24000000);
    if(factor->frac_mode == 0)
    {
        if(factor->frac_freq == 1)
          return 297000000;
        else
          return 270000000;
    }
    else
    {
        tmp_rate = tmp_rate * (factor->factorn+1);
        do_div(tmp_rate, factor->factorm+1);
        return (unsigned long)tmp_rate;
    }
}

static unsigned long calc_rate_pll_ddr(u32 parent_rate, struct clk_factors_value *factor)
{
    u64 tmp_rate = (parent_rate?parent_rate:24000000);
    tmp_rate = tmp_rate * (factor->factorn+1) * (factor->factork+1);
    do_div(tmp_rate, factor->factorm+1);
    return (unsigned long)tmp_rate;
}
static unsigned long calc_rate_pll_periph(u32 parent_rate, struct clk_factors_value *factor)
{
    return (unsigned long)(parent_rate?(parent_rate/2):12000000) * (factor->factorn+1) * (factor->factork+1);
}

static const char *hosc_parents[] = {"hosc"};
struct factor_init_data sunxi_factos[] = {
    /* name         parent        parent_num, flags       reg         lock_reg    lock_bit    config                        get_factors               calc_rate       priv_ops*/
    {"pll_cpu",     hosc_parents, 1,CLK_GET_RATE_NOCACHE, PLL_CPU,    PLL_CPU,    LOCKBIT(28),&sunxi_clk_factor_pll_cpu,    &get_factors_pll_cpu,     &calc_rate_pll_cpu   ,(struct clk_ops*)NULL},
    {"pll_audio",   hosc_parents, 1,          0,          PLL_AUDIO,  PLL_AUDIO,  LOCKBIT(28),&sunxi_clk_factor_pll_audio,  &get_factors_pll_audio,   &calc_rate_pll_audio ,(struct clk_ops*)NULL},
    {"pll_video",   hosc_parents, 1,          0,          PLL_VIDEO,  PLL_VIDEO,  LOCKBIT(28),&sunxi_clk_factor_pll_video,  &get_factors_pll_video,   &calc_rate_media     ,(struct clk_ops*)NULL},
    {"pll_ve",      hosc_parents, 1,          0,          PLL_VE,     PLL_VE,     LOCKBIT(28),&sunxi_clk_factor_pll_ve,     &get_factors_pll_ve,      &calc_rate_media     ,(struct clk_ops*)NULL},
    {"pll_ddr",     hosc_parents, 1,CLK_GET_RATE_NOCACHE, PLL_DDR,    PLL_DDR,    LOCKBIT(28),&sunxi_clk_factor_pll_ddr,    &get_factors_pll_ddr,     &calc_rate_pll_ddr   ,(struct clk_ops*)NULL},
    {"pll_periph0", hosc_parents, 1,          0,          PLL_PERIPH0,PLL_PERIPH0,LOCKBIT(28),&sunxi_clk_factor_pll_periph0,&get_factors_pll_periph0, &calc_rate_pll_periph,(struct clk_ops*)NULL},
    {"pll_gpu",     hosc_parents, 1,          0,          PLL_GPU,    PLL_GPU,    LOCKBIT(28),&sunxi_clk_factor_pll_gpu,    &get_factors_pll_gpu,     &calc_rate_media     ,(struct clk_ops*)NULL},
    {"pll_periph1", hosc_parents, 1,          0,          PLL_PERIPH1,PLL_PERIPH1,LOCKBIT(28),&sunxi_clk_factor_pll_periph1,&get_factors_pll_periph1, &calc_rate_pll_periph,(struct clk_ops*)NULL},
    {"pll_de",      hosc_parents, 1,          0,          PLL_DE,     PLL_DE,     LOCKBIT(28),&sunxi_clk_factor_pll_de,     &get_factors_pll_de,      &calc_rate_media     ,(struct clk_ops*)NULL},
};


struct periph_init_data {
    const char          *name;
    unsigned long       flags;
    const char          **parent_names;
    int                 num_parents;
    struct sunxi_clk_periph *periph;
    struct clk_ops*     priv_clkops;
};

static const char *cpu_parents[] = {"losc", "hosc", "pll_cpu", "pll_cpu"};
static const char *axi_parents[] = {"cpu"};
static const char *pll_periph0ahb1_parents[] = {"pll_periph0"};
static const char *ahb1_parents[] = {"losc", "hosc", "axi", "pll_periph0ahb1"};
static const char *apb1_parents[] = {"ahb1"};
static const char *apb2_parents[] = {"losc", "hosc", "pll_periph0", "pll_periph0"};
static const char* ths_parents[] = {"hosc","","",""};
static const char *periph0_parents[] = {"hosc", "pll_periph0","",""};
static const char *periph_parents[] = {"hosc", "pll_periph0","pll_periph1",""};
static const char *periphx_parents[] = {"pll_periph0","pll_periph1","","","","","",""};
static const char *i2s_parents[] = {"pll_audiox8", "pll_audiox4", "pll_audiox2", "pll_audio"};
static const char *display_parents[] = {"pll_periph0x2","pll_de","","","","","",""};
static const char *tcon0_parents[] = {"pll_video", "", "", "", "", "", "", ""};
static const char *tve_parents[] = {"pll_de", "pll_periph1", "", "", "", "", "", ""};
static const char *csi_m_parents[] = {"hosc", "pll_video","pll_periph1","","","","",""};
static const char *mbus_parents[] = {"hosc", "pll_periph0x2", "pll_ddr", ""};
static const char *audio_parents[] = {"pll_audio"};
static const char *ve_parents[] = {"pll_ve"};
static const char *gpu_parents[] = {"pll_gpu"};
static const char *ahb1mod_parents[] = {"ahb1"};
static const char *apb1mod_parents[] = {"apb1"};
static const char *apb2mod_parents[] = {"apb2"};
static const char *losc_parents[] = {"losc"};
static const char *sdmmc2_parents[] = {"sdmmc2mod"};
static const char *hdmi_parents[]= {"pll_video","","",""};
static const char *cpurdev_parents[]  = {"losc", "hosc","",""};

struct sunxi_clk_comgate com_gates[]={
{"csi",      0,  0x3,    BUS_GATE_SHARE|RST_GATE_SHARE|MBUS_GATE_SHARE, 0},
};
/*
SUNXI_CLK_PERIPH(name,    mux_reg,    mux_shift, mux_width, div_reg,    div_mshift, div_mwidth, div_nshift, div_nwidth, gate_flags, enable_reg, reset_reg, bus_gate_reg, drm_gate_reg, enable_shift, reset_shift, bus_gate_shift, dram_gate_shift, lock,com_gate,com_gate_off)
*/
SUNXI_CLK_PERIPH(cpu,     CPU_CFG,    16,        2,         0,          0,          0,          0,          0,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(axi,     0,           0,        0,         CPU_CFG,    0,          2,          0,          0,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(pll_periph0ahb1,  0,  0,        0,         AHB1_CFG,   6,          2,          0,          0,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ahb1,    AHB1_CFG,   12,        2,         AHB1_CFG,   0,          0,          4,          2,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(apb1,    0,           0,        0,         AHB1_CFG,   0,          0,          8,          2,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(apb2,    APB2_CFG,   24,        2,         APB2_CFG,   0,          5,         16,          2,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ths,     THS_CFG,    24,        2,         THS_CFG,    0,          0,          0,          2,          0,          THS_CFG,    BUS_RST3,  BUS_GATE2,    0,           31,            8,           8,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(nand,    NAND_CFG,   24,        2,         NAND_CFG,   0,          4,         16,          2,          0,          NAND_CFG,   BUS_RST0,  BUS_GATE0,    0,           31,           13,          13,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(sdmmc0,  SD0_CFG,    24,        2,         SD0_CFG,    0,          4,         16,          2,          0,          SD0_CFG,    BUS_RST0,  BUS_GATE0,    0,           31,            8,           8,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(sdmmc1,  SD1_CFG,    24,        2,         SD1_CFG,    0,          4,         16,          2,          0,          SD1_CFG,    BUS_RST0,  BUS_GATE0,    0,           31,            9,           9,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2mod,SD2_CFG,   24,        2,         SD2_CFG,   30,          1,          0,          0,          0,          0,          0,         0,            0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(sdmmc2,  0,           0,        0,         SD2_CFG,    0,          4,         16,          2,          0,          SD2_CFG,    BUS_RST0,  BUS_GATE0,    0,           31,           10,          10,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ts,      TS_CFG,     24,        4,         TS_CFG,     0,          4,         16,          2,          0,          TS_CFG,     BUS_RST0,  BUS_GATE0,    DRAM_GATE,   31,           18,          18,              3,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ss,      SS_CFG,     24,        2,         SS_CFG,     0,          4,         16,          2,          0,          SS_CFG,     BUS_RST0,  BUS_GATE0,    0,           31,            5,           5,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(spi0,    SPI0_CFG,   24,        2,         SPI0_CFG,   0,          4,         16,          2,          0,          SPI0_CFG,   BUS_RST0,  BUS_GATE0,    0,           31,           20,          20,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(spi1,    SPI1_CFG,   24,        2,         SPI1_CFG,   0,          4,         16,          2,          0,          SPI1_CFG,   BUS_RST0,  BUS_GATE0,    0,           31,           21,          21,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(i2s0,    I2S0_CFG,   16,        2,         0,          0,          0,          0,          0,          0,          I2S0_CFG,   BUS_RST3,  BUS_GATE2,    0,           31,           12,          12,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(i2s1,    I2S1_CFG,   16,        2,         0,          0,          0,          0,          0,          0,          I2S1_CFG,   BUS_RST3,  BUS_GATE2,    0,           31,           13,          13,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(i2s2,    I2S2_CFG,   16,        2,         0,          0,          0,          0,          0,          0,          I2S2_CFG,   BUS_RST3,  BUS_GATE2,    0,           31,           14,          14,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(owa,     0,           0,        0,         OWA_CFG,    0,          4,          0,          0,          0,          SPI1_CFG,   BUS_RST3,  BUS_GATE2,    0,           31,            1,           1,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbohci3,0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    BUS_RST0,  BUS_GATE0,    0,           19,           31,          31,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbohci2,0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    BUS_RST0,  BUS_GATE0,    0,           18,           30,          30,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbohci1,0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    BUS_RST0,  BUS_GATE0,    0,           17,           29,          29,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbohci0,0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    BUS_RST0,  BUS_GATE0,    0,           16,           28,          28,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbehci3,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           27,          27,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbehci2,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           26,          26,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbehci1,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           25,          25,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbehci0,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           24,          24,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbphy0, 0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    USB_CFG,   0,            0,            8,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbphy1, 0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    USB_CFG,   0,            0,            9,            1,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbphy2, 0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    USB_CFG,   0,            0,           10,            2,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbphy3, 0,           0,        0,         0,          0,          0,          0,          0,          0,          USB_CFG,    USB_CFG,   0,            0,           11,            3,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(usbotg,  0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           23,          23,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(de,      DE_CFG,     24,        3,         DE_CFG,     0,          4,          0,          0,          0,          DE_CFG,     BUS_RST1,  BUS_GATE1,    0,           31,           12,          12,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(tcon0,   TCON0_CFG,  24,        3,         TCON0_CFG,  0,          4,          0,          0,          0,          TCON0_CFG,  BUS_RST1,  BUS_GATE1,    0,           31,            3,           3,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(tve,     TVE_CFG,    24,        3,         TVE_CFG,    0,          4,          0,          0,          0,          TVE_CFG,    BUS_RST1,  BUS_GATE1,    0,           31,            9,           9,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(deinterlace,DEINTERLACE_CFG,24, 3, DEINTERLACE_CFG,    0,          4,          0,          0,          0,  DEINTERLACE_CFG,    BUS_RST1,  BUS_GATE1,    DRAM_GATE,   31,            5,           5,              2,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(csi_s,   CSI_CFG,    24,        3,         CSI_CFG,   16,          4,          0,          0,          0,          CSI_CFG,    BUS_RST1,  BUS_GATE1,    DRAM_GATE,   31,            8,           8,              1,               &clk_lock,&com_gates[0],    0);
SUNXI_CLK_PERIPH(csi_m,   CSI_CFG,     8,        3,         CSI_CFG,    0,          4,          0,          0,          0,          CSI_CFG,    BUS_RST1,  BUS_GATE1,    DRAM_GATE,   15,            8,           8,              1,               &clk_lock,&com_gates[0],    1);
SUNXI_CLK_PERIPH(csi_misc,CSI_MISC,    0,        0,         0,          0,          0,          0,          0,          0,          CSI_MISC,   BUS_RST1,  BUS_GATE1,    DRAM_GATE,   31,            8,           8,              1,               &clk_lock,&com_gates[0],    2);
SUNXI_CLK_PERIPH(ve,      0,           0,        0,         VE_CFG,    16,          3,          0,          0,          0,          VE_CFG,     BUS_RST1,  BUS_GATE1,    DRAM_GATE,   31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(adda,    0,           0,        0,         0,          0,          0,          0,          0,          0,          ADDA_CFG,   BUS_RST3,  BUS_GATE2,    0,           31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(avs,     0,           0,        0,         0,          0,          0,          0,          0,          0,          AVS_CFG,    0,          0,           0,           31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(hdmi,	   HDMI_CFG,  24,        2,         HDMI_CFG,   0,          4,          0,          0,          0,          HDMI_CFG,   BUS_RST1,  BUS_GATE1,    BUS_RST1,    31,           11,          11,             10,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(hdmi_slow,0,  		   0,        0,         0,          0,          0,          0,          0,          0,          HDMI_SLOW,  0,          0,           0,           31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(mbus,    MBUS_CFG,   24,        2,         MBUS_CFG,   0,          3,          0,          0,          0,          MBUS_CFG,   0,          0,           0,           31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(gpu,     0,           0,        0,         GPU_CFG,    0,          3,          0,          0,          0,          GPU_CFG,    BUS_RST1,  BUS_GATE1,    0,           31,           20,          20,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ats,     ATS_CFG,    24,        2,         ATS_CFG,    0,          3,          0,          0,          0,          ATS_CFG,    0,          0,           0,           31,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(scr,     0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,  BUS_GATE3,    0,            0,           20,          20,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(gmac,    0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,           17,          17,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(ephy,    0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST2,  BUS_GATE4,    0,            0,            2,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(spinlock,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST1,  BUS_GATE1,    0,            0,           22,          22,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(msgbox,  0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST1,  BUS_GATE1,    0,            0,           21,          21,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(dma,     0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST0,  BUS_GATE0,    0,            0,            6,           6,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(pio,     0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          0,         BUS_GATE2,    0,            0,            0,           5,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(twi0,    0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(twi1,    0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,            1,           1,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(twi2,    0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,            2,           2,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(uart0,   0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,           16,          16,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(uart1,   0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,           17,          17,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(uart2,   0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,           18,          18,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(uart3,   0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          BUS_RST4,   BUS_GATE3,   0,            0,           19,          19,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(losc_out,0,           0,        0,         0,          0,          0,          0,          0,          0,          0,          0,          LOSC_OUT_GATE,0,           0,            0,           0,              0,               &clk_lock,NULL,             0);
SUNXI_CLK_PERIPH(cpurcir,  CPUS_CIR,  24,        2,         CPUS_CIR,   0,          4,          16,         2,          0,          CPUS_CIR,   CPUS_APB0_RST,CPUS_APB0_GATE,0,       31,            1,           1,              0,               &clk_lock,NULL,             0);
struct periph_init_data sunxi_periphs_init[] = {
    {"cpu",      CLK_GET_RATE_NOCACHE,	cpu_parents,      ARRAY_SIZE(cpu_parents),      &sunxi_clk_periph_cpu},
    {"axi",      0,       				axi_parents,      ARRAY_SIZE(axi_parents),      &sunxi_clk_periph_axi},
    {"pll_periph0ahb1",CLK_IGNORE_SYNCBOOT,pll_periph0ahb1_parents,ARRAY_SIZE(pll_periph0ahb1_parents),&sunxi_clk_periph_pll_periph0ahb1},
    {"ahb1",     0,       				ahb1_parents,     ARRAY_SIZE(ahb1_parents),     &sunxi_clk_periph_ahb1},
    {"apb1",     0,       				apb1_parents,     ARRAY_SIZE(apb1_parents),     &sunxi_clk_periph_apb1},
    {"apb2",     0,       				apb2_parents,     ARRAY_SIZE(apb2_parents),     &sunxi_clk_periph_apb2},
    {"ths",      0,       				ths_parents,      ARRAY_SIZE(ths_parents),      &sunxi_clk_periph_ths},
    {"nand",     0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_nand},
    {"sdmmc0",   0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_sdmmc0},
    {"sdmmc1",   0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_sdmmc1},
    {"sdmmc2mod",0,         			periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_sdmmc2mod},
    {"sdmmc2",   0,       			    sdmmc2_parents,   ARRAY_SIZE(sdmmc2_parents),   &sunxi_clk_periph_sdmmc2},
    {"ts",       0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_ts},
    {"ss",       0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_ss},
    {"spi0",     0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_spi0},
    {"spi1",     0,       				periph_parents,   ARRAY_SIZE(periph_parents),   &sunxi_clk_periph_spi1},
    {"i2s0",     0,       				i2s_parents,      ARRAY_SIZE(i2s_parents),      &sunxi_clk_periph_i2s0},
    {"i2s1",     0,       				i2s_parents,      ARRAY_SIZE(i2s_parents),      &sunxi_clk_periph_i2s1},
    {"i2s2",     0,       				i2s_parents,      ARRAY_SIZE(i2s_parents),      &sunxi_clk_periph_i2s2},
    {"owa",      0,       				audio_parents,    ARRAY_SIZE(audio_parents),    &sunxi_clk_periph_owa},
    {"usbohci3", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbohci3},
    {"usbohci2", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbohci2},
    {"usbohci1", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbohci1},
    {"usbohci0", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbohci0},
    {"usbehci3", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbehci3},
    {"usbehci2", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbehci2},
    {"usbehci1", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbehci1},
    {"usbehci0", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbehci0},
    {"usbotg",   0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_usbotg},
    {"usbphy3",  0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_usbphy3},
    {"usbphy2",  0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_usbphy2},
    {"usbphy1",  0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_usbphy1},
    {"usbphy0",  0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_usbphy0},
    {"de",       0,       				display_parents,  ARRAY_SIZE(display_parents),  &sunxi_clk_periph_de},
    {"tcon0",    0,       				tcon0_parents,    ARRAY_SIZE(tcon0_parents),    &sunxi_clk_periph_tcon0},
    {"tve",      0,       				tve_parents,      ARRAY_SIZE(tve_parents),      &sunxi_clk_periph_tve},
    {"deinterlace",0,    	            periphx_parents,  ARRAY_SIZE(periphx_parents),  &sunxi_clk_periph_deinterlace},
    {"csi_s",    0,       				periphx_parents,  ARRAY_SIZE(periphx_parents),  &sunxi_clk_periph_csi_s},
    {"csi_m",    0,       				csi_m_parents,    ARRAY_SIZE(csi_m_parents),    &sunxi_clk_periph_csi_m},
    {"csi_misc", 0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_csi_misc},
    {"ve",       0,       				ve_parents,       ARRAY_SIZE(ve_parents),       &sunxi_clk_periph_ve},
    {"adda",     0,       				audio_parents,    ARRAY_SIZE(audio_parents),    &sunxi_clk_periph_adda},
    {"avs",      0,       				hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_avs},
    {"hdmi",     0,        			    hdmi_parents,     ARRAY_SIZE(hdmi_parents),     &sunxi_clk_periph_hdmi},
    {"hdmi_slow",0,       			    hosc_parents,     ARRAY_SIZE(hosc_parents),     &sunxi_clk_periph_hdmi_slow},
    {"mbus",     0,       				mbus_parents,     ARRAY_SIZE(mbus_parents),     &sunxi_clk_periph_mbus},
    {"gpu",      0,       				gpu_parents,      ARRAY_SIZE(gpu_parents),      &sunxi_clk_periph_gpu},
    {"ats",      0,       				periph0_parents,  ARRAY_SIZE(periph0_parents),  &sunxi_clk_periph_ats},
    {"scr",      0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_scr},
    {"gmac",     0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_gmac},
    {"ephy",     0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_ephy},
    {"spinlock", 0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_spinlock},
    {"msgbox",   0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_msgbox},
    {"dma",      0,       				ahb1mod_parents,  ARRAY_SIZE(ahb1mod_parents),  &sunxi_clk_periph_dma},
    {"pio",      0,       				apb1mod_parents,  ARRAY_SIZE(apb1mod_parents),  &sunxi_clk_periph_pio},
    {"twi0",     0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_twi0},
    {"twi1",     0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_twi1},
    {"twi2",     0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_twi2},
    {"uart0",    0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_uart0},
    {"uart1",    0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_uart1},
    {"uart2",    0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_uart2},
    {"uart3",    0,       				apb2mod_parents,  ARRAY_SIZE(apb2mod_parents),  &sunxi_clk_periph_uart3},
	{"losc_out", 0,           			losc_parents,     ARRAY_SIZE(losc_parents),     &sunxi_clk_periph_losc_out},
};
static struct periph_init_data sunxi_periphs_cpus_init[] = {
    {"cpurcir",CLK_GET_RATE_NOCACHE,    cpurdev_parents,ARRAY_SIZE(cpurdev_parents),    &sunxi_clk_periph_cpurcir},
};


static unsigned int ths_n_factor[]={1,2,4,6};
static unsigned long sunxi_ths_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
    unsigned long reg;
    struct sunxi_clk_periph *periph = to_clk_periph(hw);
    struct sunxi_clk_periph_div *divider = &periph->divider;
    unsigned long div, div_n = 0;
    u64 rate = parent_rate;
    if(!divider->reg)
        return parent_rate;

    reg = periph_readl(periph,divider->reg);
    if(divider->nwidth)
        div_n = GET_BITS(divider->nshift, divider->nwidth, reg);
    div = ths_n_factor[div_n];
    do_div(rate, div);
    return rate;
}
static long sunxi_ths_round_rate(struct clk_hw *hw, unsigned long rate, unsigned long *prate)
{
    int j;
    int n=0;
    unsigned long cur_rate=0,new_rate=0;
    unsigned long cur_delta,new_delta;
    u32 parent_rate = *prate;

    for(j=0;j<4;j++)
    {
        new_rate = parent_rate/ths_n_factor[j];
        new_delta = (new_rate >rate)?(new_rate-rate):(rate-new_rate);
        cur_delta = (cur_rate >rate)?(cur_rate-rate):(rate-cur_rate);
        if(new_delta < cur_delta)
        {
            cur_rate = new_rate;
            n = j;
        }
    }
    return cur_rate;
}
struct clk_ops ths_ops;
void __init sunxi_init_clocks(void)
{
    int     i;
	struct clk *clk;
    struct factor_init_data *factor;
    struct periph_init_data *periph;

#ifdef CONFIG_SUNXI_CLK_DUMMY_DEBUG
		sunxi_clk_base = &dummy_reg[0];
		dummy_reg_init();
#else
    /* get clk register base address */
    sunxi_clk_base = IO_ADDRESS(0x01c20000);
    sunxi_clk_cpus_base = IO_ADDRESS(0x01f01400);
#endif
    sunxi_clk_factor_initlimits();
    /* register oscs */
    clk = clk_register_fixed_rate(NULL, "losc", NULL, CLK_IS_ROOT, 32768);
    clk_register_clkdev(clk, "losc", NULL);

    clk = clk_register_fixed_rate(NULL, "hosc", NULL, CLK_IS_ROOT, 24000000);
    clk_register_clkdev(clk, "hosc", NULL);

    /* register normal factors, based on sunxi factor framework */
    for(i=0; i<ARRAY_SIZE(sunxi_factos); i++) {
        factor = &sunxi_factos[i];
        clk = sunxi_clk_register_factors(NULL,  sunxi_clk_base, &clk_lock,factor);
        clk_register_clkdev(clk, factor->name, NULL);
    }

    /* register fixed factors, based on clk-fixed-factor framework, such as pllx2 for ex. */
    clk = clk_register_fixed_factor(NULL, "pll_audiox8", "pll_audio", 0, 8, 1);
    clk_register_clkdev(clk, "pll_audiox8", NULL);

    clk = clk_register_fixed_factor(NULL, "pll_audiox4", "pll_audio", 0, 8, 2);
    clk_register_clkdev(clk, "pll_audiox4", NULL);

    clk = clk_register_fixed_factor(NULL, "pll_audiox2", "pll_audio", 0, 8, 4);
    clk_register_clkdev(clk, "pll_audiox2", NULL);

    clk = clk_register_fixed_factor(NULL, "pll_videox2", "pll_video", 0, 2, 1);
    clk_register_clkdev(clk, "pll_videox2", NULL);

    clk = clk_register_fixed_factor(NULL, "pll_periph0x2", "pll_periph0", 0, 2, 1);
    clk_register_clkdev(clk, "pll_periph0x2", NULL);

    clk = clk_register_fixed_factor(NULL, "hoscd2", "hosc", 0, 1, 2);
    clk_register_clkdev(clk, "hoscd2", NULL);

    sunxi_clk_get_periph_ops(&ths_ops);
    ths_ops.recalc_rate = sunxi_ths_recalc_rate;
    ths_ops.round_rate = sunxi_ths_round_rate;
    /* register periph clock */
    for(i=0; i<ARRAY_SIZE(sunxi_periphs_init); i++) {
        periph = &sunxi_periphs_init[i];
        if(!strcmp(periph->name,"ths"))
        {
            periph->periph->priv_clkops = &ths_ops;
        }
		if((unsigned int)periph->periph->gate.bus == (unsigned int)LOSC_OUT_GATE)
			clk = sunxi_clk_register_periph(periph->name, periph->parent_names,
					periph->num_parents,periph->flags, IO_ADDRESS(0x00000000), periph->periph);
		else
			clk = sunxi_clk_register_periph(periph->name, periph->parent_names,
					periph->num_parents,periph->flags, sunxi_clk_base, periph->periph);
        clk_register_clkdev(clk, periph->name, NULL);
    }
    for(i=0; i<ARRAY_SIZE(sunxi_periphs_cpus_init); i++) {
        periph = &sunxi_periphs_cpus_init[i];
        clk = sunxi_clk_register_periph(periph->name, periph->parent_names,
                        periph->num_parents,periph->flags, sunxi_clk_cpus_base , periph->periph);
        clk_register_clkdev(clk, periph->name, NULL);
    }
    clk_add_alias("pll1",NULL,"pll_cpu",NULL);
    clk_add_alias("pll2",NULL,"pll_audio",NULL);
    clk_add_alias("pll3",NULL,"pll_video",NULL);
    clk_add_alias("pll4",NULL,"pll_ve",NULL);
    clk_add_alias("pll5",NULL,"pll_ddr",NULL);
    clk_add_alias("pll6",NULL,"pll_periph0",NULL);
    clk_add_alias("pll8",NULL,"pll_gpu",NULL);
    clk_add_alias("pll10",NULL,"pll_de",NULL);
    clk_add_alias("pll6ahb1",NULL,"pll_periph0ahb1",NULL);
#ifdef CONFIG_COMMON_CLK_ENABLE_SYNCBOOT_EARLY
	clk_syncboot();
#endif
}

