#ifdef CONFIG_MACH_DAVINCI_LEGOEV3
#warning "This is still really fragile test code"
#endif

/*
 * TI DA850/OMAP-L138 chip specific setup
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Derived from: arch/arm/mach-davinci/da830.c
 * Original Copyrights follow:
 *
 * 2009 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm/ehrpwm.h>
#include <linux/pwm_backlight.h>

#include <asm/mach/map.h>

#include <mach/psc.h>
#include <mach/irqs.h>
#include <mach/cputype.h>
#include <mach/common.h>
#include <mach/time.h>
#include <mach/da8xx.h>
#include <mach/cpufreq.h>
#include <mach/pm.h>
#include <mach/gpio-davinci.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/davinci/vpif_types.h>
#include "clock.h"
#include "mux.h"

/* SoC specific clock flags */
#define DA850_CLK_ASYNC3	BIT(16)

#define DA850_PLL1_BASE		0x01e1a000
#define DA850_TIMER64P2_BASE	0x01f0c000
#define DA850_TIMER64P3_BASE	0x01f0d000

#define DA850_REF_FREQ		24000000

#define CFGCHIP3_ASYNC3_CLKSRC	BIT(4)
#define CFGCHIP3_PLL1_MASTER_LOCK	BIT(5)
#define CFGCHIP0_PLL_MASTER_LOCK	BIT(4)
#define PLLC0_PLL1_SYSCLK3_EXTCLKSRC	BIT(9)

static int da850_set_armrate(struct clk *clk, unsigned long rate);
static int da850_round_armrate(struct clk *clk, unsigned long rate);
static int da850_set_pll0rate(struct clk *clk, unsigned long armrate);
static int da850_set_pll0sysclk3_rate(struct clk *clk, unsigned long rate);

static struct pll_data pll0_data = {
	.num		= 1,
	.phys_base	= DA8XX_PLL0_BASE,
	.flags		= PLL_HAS_PREDIV | PLL_HAS_POSTDIV,
};

static struct clk ref_clk = {
	.name		= "ref_clk",
	.rate		= DA850_REF_FREQ,
	.set_rate	= davinci_simple_set_rate,
};

static struct clk pll0_clk = {
	.name		= "pll0",
	.parent		= &ref_clk,
	.pll_data	= &pll0_data,
	.flags		= CLK_PLL,
	.set_rate	= da850_set_pll0rate,
};

static struct clk pll0_aux_clk = {
	.name		= "pll0_aux_clk",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL | PRE_PLL,
};

static struct clk pll0_sysclk2 = {
	.name		= "pll0_sysclk2",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV2,
};

static struct clk pll0_sysclk3 = {
	.name		= "pll0_sysclk3",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV3,
	.set_rate	= da850_set_pll0sysclk3_rate,
	.maxrate	= 148000000,
};

static struct clk pll0_sysclk4 = {
	.name		= "pll0_sysclk4",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV4,
};

static struct clk pll0_sysclk5 = {
	.name		= "pll0_sysclk5",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV5,
};

static struct clk pll0_sysclk6 = {
	.name		= "pll0_sysclk6",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV6,
};

static struct clk pll0_sysclk7 = {
	.name		= "pll0_sysclk7",
	.parent		= &pll0_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV7,
};

static struct pll_data pll1_data = {
	.num		= 2,
	.phys_base	= DA850_PLL1_BASE,
	.flags		= PLL_HAS_POSTDIV,
};

static struct clk pll1_clk = {
	.name		= "pll1",
	.parent		= &ref_clk,
	.pll_data	= &pll1_data,
	.flags		= CLK_PLL,
};

static struct clk pll1_aux_clk = {
	.name		= "pll1_aux_clk",
	.parent		= &pll1_clk,
	.flags		= CLK_PLL | PRE_PLL,
};

static struct clk pll1_sysclk2 = {
	.name		= "pll1_sysclk2",
	.parent		= &pll1_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV2,
};

static struct clk pll1_sysclk3 = {
	.name		= "pll1_sysclk3",
	.parent		= &pll1_clk,
	.flags		= CLK_PLL,
	.div_reg	= PLLDIV3,
};

static struct clk i2c0_clk = {
	.name		= "i2c0",
	.parent		= &pll0_aux_clk,
};

static struct clk timerp64_0_clk = {
	.name		= "timer0",
	.parent		= &pll0_aux_clk,
};

static struct clk timerp64_1_clk = {
	.name		= "timer1",
	.parent		= &pll0_aux_clk,
};

static struct clk arm_rom_clk = {
	.name		= "arm_rom",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_ARM_RAM_ROM,
	.flags		= ALWAYS_ENABLED,
};

static struct clk tpcc0_clk = {
	.name		= "tpcc0",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_TPCC,
	.flags		= ALWAYS_ENABLED | CLK_PSC,
};

static struct clk tptc0_clk = {
	.name		= "tptc0",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_TPTC0,
	.flags		= ALWAYS_ENABLED,
};

static struct clk tptc1_clk = {
	.name		= "tptc1",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_TPTC1,
	.flags		= ALWAYS_ENABLED,
};

static struct clk tpcc1_clk = {
	.name		= "tpcc1",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA850_LPSC1_TPCC1,
	.gpsc		= 1,
	.flags		= CLK_PSC | ALWAYS_ENABLED,
};

static struct clk tptc2_clk = {
	.name		= "tptc2",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA850_LPSC1_TPTC2,
	.gpsc		= 1,
	.flags		= ALWAYS_ENABLED,
};

static struct clk pruss_clk = {
	.name		= "pruss",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_PRUSS,
};

static struct clk uart0_clk = {
	.name		= "uart0",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_UART0,
};

static struct clk uart1_clk = {
	.name		= "uart1",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_UART1,
	.gpsc		= 1,
	.flags		= DA850_CLK_ASYNC3,
};

static struct clk uart2_clk = {
	.name		= "uart2",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_UART2,
	.gpsc		= 1,
	.flags		= DA850_CLK_ASYNC3,
};

static struct clk aintc_clk = {
	.name		= "aintc",
	.parent		= &pll0_sysclk4,
	.lpsc		= DA8XX_LPSC0_AINTC,
	.flags		= ALWAYS_ENABLED,
};

static struct clk gpio_clk = {
	.name		= "gpio",
	.parent		= &pll0_sysclk4,
	.lpsc		= DA8XX_LPSC1_GPIO,
	.gpsc		= 1,
};

static struct clk i2c1_clk = {
	.name		= "i2c1",
	.parent		= &pll0_sysclk4,
	.lpsc		= DA8XX_LPSC1_I2C,
	.gpsc		= 1,
};

static struct clk emif3_clk = {
	.name		= "emif3",
	.parent		= &pll0_sysclk5,
	.lpsc		= DA8XX_LPSC1_EMIF3C,
	.gpsc		= 1,
	.flags		= ALWAYS_ENABLED,
};

static struct clk arm_clk = {
	.name		= "arm",
	.parent		= &pll0_sysclk6,
	.lpsc		= DA8XX_LPSC0_ARM,
	.flags		= ALWAYS_ENABLED,
	.set_rate	= da850_set_armrate,
	.round_rate	= da850_round_armrate,
};

static struct clk rmii_clk = {
	.name		= "rmii",
	.parent		= &pll0_sysclk7,
};

static struct clk emac_clk = {
	.name		= "emac",
	.parent		= &pll0_sysclk4,
	.lpsc		= DA8XX_LPSC1_CPGMAC,
	.gpsc		= 1,
};

static struct clk mcasp_clk = {
	.name		= "mcasp",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_McASP0,
	.gpsc		= 1,
	.flags		= DA850_CLK_ASYNC3,
};

static struct clk lcdc_clk = {
	.name		= "lcdc",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_LCDC,
	.gpsc		= 1,
};

static struct clk mmcsd0_clk = {
	.name		= "mmcsd0",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_MMC_SD,
};

static struct clk mmcsd1_clk = {
	.name		= "mmcsd1",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA850_LPSC1_MMC_SD1,
	.gpsc		= 1,
};

static struct clk aemif_clk = {
	.name		= "aemif",
	.parent		= &pll0_sysclk3,
	.lpsc		= DA8XX_LPSC0_EMIF25,
	.flags		= ALWAYS_ENABLED,
};

static struct clk usb11_clk = {
	.name		= "usb11",
	.parent		= &pll0_sysclk4,
	.lpsc		= DA8XX_LPSC1_USB11,
	.gpsc		= 1,
};

static struct clk usb20_clk = {
	.name		= "usb20",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_USB20,
	.gpsc		= 1,
};

static struct clk spi0_clk = {
	.name		= "spi0",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC0_SPI0,
};

static struct clk spi1_clk = {
	.name		= "spi1",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_SPI1,
	.gpsc		= 1,
	.flags		= DA850_CLK_ASYNC3,
};

static struct clk vpif_clk = {
	.name		= "vpif",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA850_LPSC1_VPIF,
	.gpsc		= 1,
};

static struct clk sata_clk = {
	.name		= "sata",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA850_LPSC1_SATA,
	.gpsc		= 1,
	.flags		= PSC_FORCE,
};

static struct clk ehrpwm_clk = {
	.name		= "ehrpwm",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_PWM,
	.gpsc		= 1,
	.flags          = DA850_CLK_ASYNC3,
};

static struct clk ecap_clk = {
	.name		= "ecap",
	.parent		= &pll0_sysclk2,
	.lpsc		= DA8XX_LPSC1_ECAP,
	.gpsc		= 1,
	.flags          = DA850_CLK_ASYNC3,
};

static struct clk_lookup da850_clks[] = {
	CLK(NULL,		"ref",		&ref_clk),
	CLK(NULL,		"pll0",		&pll0_clk),
	CLK(NULL,		"pll0_aux",	&pll0_aux_clk),
	CLK(NULL,		"pll0_sysclk2",	&pll0_sysclk2),
	CLK(NULL,		"pll0_sysclk3",	&pll0_sysclk3),
	CLK(NULL,		"pll0_sysclk4",	&pll0_sysclk4),
	CLK(NULL,		"pll0_sysclk5",	&pll0_sysclk5),
	CLK(NULL,		"pll0_sysclk6",	&pll0_sysclk6),
	CLK(NULL,		"pll0_sysclk7",	&pll0_sysclk7),
	CLK(NULL,		"pll1",		&pll1_clk),
	CLK(NULL,		"pll1_aux",	&pll1_aux_clk),
	CLK(NULL,		"pll1_sysclk2",	&pll1_sysclk2),
	CLK(NULL,		"pll1_sysclk3",	&pll1_sysclk3),
	CLK("i2c_davinci.1",	NULL,		&i2c0_clk),
	CLK(NULL,		"timer0",	&timerp64_0_clk),
	CLK("watchdog",		NULL,		&timerp64_1_clk),
	CLK(NULL,		"arm_rom",	&arm_rom_clk),
	CLK(NULL,		"tpcc0",	&tpcc0_clk),
	CLK(NULL,		"tptc0",	&tptc0_clk),
	CLK(NULL,		"tptc1",	&tptc1_clk),
	CLK(NULL,		"tpcc1",	&tpcc1_clk),
	CLK(NULL,		"tptc2",	&tptc2_clk),
	CLK(NULL,		"pruss",	&pruss_clk),
	CLK(NULL,		"uart0",	&uart0_clk),
	CLK(NULL,		"uart1",	&uart1_clk),
	CLK(NULL,		"uart2",	&uart2_clk),
	CLK(NULL,		"aintc",	&aintc_clk),
	CLK(NULL,		"gpio",		&gpio_clk),
	CLK("i2c_davinci.2",	NULL,		&i2c1_clk),
	CLK(NULL,		"emif3",	&emif3_clk),
	CLK(NULL,		"arm",		&arm_clk),
	CLK(NULL,		"rmii",		&rmii_clk),
	CLK("davinci_emac.1",	NULL,		&emac_clk),
	CLK("davinci-mcasp.0",	NULL,		&mcasp_clk),
	CLK("da8xx_lcdc.0",	NULL,		&lcdc_clk),
	CLK("davinci_mmc.0",	NULL,		&mmcsd0_clk),
	CLK("davinci_mmc.1",	NULL,		&mmcsd1_clk),
	CLK(NULL,		"aemif",	&aemif_clk),
	CLK(NULL,		"usb11",	&usb11_clk),
	CLK(NULL,		"usb20",	&usb20_clk),
	CLK("spi_davinci.0",	NULL,		&spi0_clk),
	CLK("spi_davinci.1",	NULL,		&spi1_clk),
	CLK(NULL,		"vpif",		&vpif_clk),
	CLK("ahci",		NULL,		&sata_clk),
	CLK(NULL,               "ehrpwm",       &ehrpwm_clk),
	CLK(NULL,		"ecap",		&ecap_clk),
	CLK(NULL,		NULL,		NULL),
};

/*
 * Device specific mux setup
 *
 *		soc	description	mux	mode	mode	mux	dbg
 *					reg	offset	mask	mode
 */
static const struct mux_config da850_pins[] = {
#ifdef CONFIG_DAVINCI_MUX
	/* UART0 function */
	MUX_CFG(DA850, NUART0_CTS,	3,	24,	15,	2,	true)
	MUX_CFG(DA850, NUART0_RTS,	3,	28,	15,	2,	true)
	MUX_CFG(DA850, UART0_RXD,	3,	16,	15,	2,	true)
	MUX_CFG(DA850, UART0_TXD,	3,	20,	15,	2,	true)
	/* UART1 function */
	MUX_CFG(DA850, NUART1_CTS,	0,	20,	15,	4,	true)
	MUX_CFG(DA850, NUART1_RTS,	0,	16,	15,	4,	true)
	MUX_CFG(DA850, UART1_RXD,	4,	24,	15,	2,	true)
	MUX_CFG(DA850, UART1_TXD,	4,	28,	15,	2,	true)
	/* UART2 function */
	MUX_CFG(DA850, UART2_RXD,	4,	16,	15,	2,	true)
	MUX_CFG(DA850, UART2_TXD,	4,	20,	15,	2,	true)
	/* I2C1 function */
	MUX_CFG(DA850, I2C1_SCL,	4,	16,	15,	4,	true)
	MUX_CFG(DA850, I2C1_SDA,	4,	20,	15,	4,	true)
	/* I2C0 function */
	MUX_CFG(DA850, I2C0_SDA,	4,	12,	15,	2,	true)
	MUX_CFG(DA850, I2C0_SCL,	4,	8,	15,	2,	true)
	/* EMAC function */
	MUX_CFG(DA850, MII_TXEN,	2,	4,	15,	8,	true)
	MUX_CFG(DA850, MII_TXCLK,	2,	8,	15,	8,	true)
	MUX_CFG(DA850, MII_COL,		2,	12,	15,	8,	true)
	MUX_CFG(DA850, MII_TXD_3,	2,	16,	15,	8,	true)
	MUX_CFG(DA850, MII_TXD_2,	2,	20,	15,	8,	true)
	MUX_CFG(DA850, MII_TXD_1,	2,	24,	15,	8,	true)
	MUX_CFG(DA850, MII_TXD_0,	2,	28,	15,	8,	true)
	MUX_CFG(DA850, MII_RXCLK,	3,	0,	15,	8,	true)
	MUX_CFG(DA850, MII_RXDV,	3,	4,	15,	8,	true)
	MUX_CFG(DA850, MII_RXER,	3,	8,	15,	8,	true)
	MUX_CFG(DA850, MII_CRS,		3,	12,	15,	8,	true)
	MUX_CFG(DA850, MII_RXD_3,	3,	16,	15,	8,	true)
	MUX_CFG(DA850, MII_RXD_2,	3,	20,	15,	8,	true)
	MUX_CFG(DA850, MII_RXD_1,	3,	24,	15,	8,	true)
	MUX_CFG(DA850, MII_RXD_0,	3,	28,	15,	8,	true)
	MUX_CFG(DA850, MDIO_CLK,	4,	0,	15,	8,	true)
	MUX_CFG(DA850, MDIO_D,		4,	4,	15,	8,	true)
	MUX_CFG(DA850, RMII_TXD_0,	14,	12,	15,	8,	true)
	MUX_CFG(DA850, RMII_TXD_1,	14,	8,	15,	8,	true)
	MUX_CFG(DA850, RMII_TXEN,	14,	16,	15,	8,	true)
	MUX_CFG(DA850, RMII_CRS_DV,	15,	4,	15,	8,	true)
	MUX_CFG(DA850, RMII_RXD_0,	14,	24,	15,	8,	true)
	MUX_CFG(DA850, RMII_RXD_1,	14,	20,	15,	8,	true)
	MUX_CFG(DA850, RMII_RXER,	14,	28,	15,	8,	true)
	MUX_CFG(DA850, RMII_MHZ_50_CLK,	15,	0,	15,	0,	true)
	/* McASP function */
	MUX_CFG(DA850,	ACLKR,		0,	0,	15,	1,	true)
	MUX_CFG(DA850,	ACLKX,		0,	4,	15,	1,	true)
	MUX_CFG(DA850,	AFSR,		0,	8,	15,	1,	true)
	MUX_CFG(DA850,	AFSX,		0,	12,	15,	1,	true)
	MUX_CFG(DA850,	AHCLKX,		0,	20,	15,	1,	true)
	MUX_CFG(DA850,	AMUTE,		0,	24,	15,	1,	true)
	MUX_CFG(DA850,	AXR_15,		1,	0,	15,	1,	true)
	MUX_CFG(DA850,	AXR_14,		1,	4,	15,	1,	true)
	MUX_CFG(DA850,	AXR_13,		1,	8,	15,	1,	true)
	MUX_CFG(DA850,	AXR_12,		1,	12,	15,	1,	true)
	MUX_CFG(DA850,	AXR_11,		1,	16,	15,	1,	true)
	MUX_CFG(DA850,	AXR_10,		1,	20,	15,	1,	true)
	MUX_CFG(DA850,	AXR_9,		1,	24,	15,	1,	true)
	MUX_CFG(DA850,	AXR_8,		1,	28,	15,	1,	true)
	MUX_CFG(DA850,	AXR_7,		2,	0,	15,	1,	true)
	MUX_CFG(DA850,	AXR_6,		2,	4,	15,	1,	true)
	MUX_CFG(DA850,	AXR_5,		2,	8,	15,	1,	true)
	MUX_CFG(DA850,	AXR_4,		2,	12,	15,	1,	true)
	MUX_CFG(DA850,	AXR_3,		2,	16,	15,	1,	true)
	MUX_CFG(DA850,	AXR_2,		2,	20,	15,	1,	true)
	MUX_CFG(DA850,	AXR_1,		2,	24,	15,	1,	true)
	MUX_CFG(DA850,	AXR_0,		2,	28,	15,	1,	true)
	/* LCD function */
	MUX_CFG(DA850, LCD_D_7,		16,	8,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_6,		16,	12,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_5,		16,	16,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_4,		16,	20,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_3,		16,	24,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_2,		16,	28,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_1,		17,	0,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_0,		17,	4,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_15,	17,	8,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_14,	17,	12,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_13,	17,	16,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_12,	17,	20,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_11,	17,	24,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_10,	17,	28,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_9,		18,	0,	15,	2,	true)
	MUX_CFG(DA850, LCD_D_8,		18,	4,	15,	2,	true)
	MUX_CFG(DA850, LCD_PCLK,	18,	24,	15,	2,	true)
	MUX_CFG(DA850, LCD_MCLK,	18,	28,	15,	2,	true)
	MUX_CFG(DA850, LCD_HSYNC,	19,	0,	15,	2,	true)
	MUX_CFG(DA850, LCD_VSYNC,	19,	4,	15,	2,	true)
	MUX_CFG(DA850, NLCD_AC_ENB_CS,	19,	24,	15,	2,	true)
	/* MMC/SD0 function */
	MUX_CFG(DA850, MMCSD0_DAT_0,	10,	8,	15,	2,	true)
	MUX_CFG(DA850, MMCSD0_DAT_1,	10,	12,	15,	2,	true)
	MUX_CFG(DA850, MMCSD0_DAT_2,	10,	16,	15,	2,	true)
	MUX_CFG(DA850, MMCSD0_DAT_3,	10,	20,	15,	2,	true)
	MUX_CFG(DA850, MMCSD0_CLK,	10,	0,	15,	2,	true)
	MUX_CFG(DA850, MMCSD0_CMD,	10,	4,	15,	2,	true)
	/* MMC/SD1 function */
	MUX_CFG(DA850, MMCSD1_DAT_0,	18,	8,	15,	2,	true)
	MUX_CFG(DA850, MMCSD1_DAT_1,	19,	16,	15,	2,	true)
	MUX_CFG(DA850, MMCSD1_DAT_2,	19,	12,	15,	2,	true)
	MUX_CFG(DA850, MMCSD1_DAT_3,	19,	8,	15,	2,	true)
	MUX_CFG(DA850, MMCSD1_CLK,	18,	12,	15,	2,	true)
	MUX_CFG(DA850, MMCSD1_CMD,	18,	16,	15,	2,	true)
	/* EMIF2.5/EMIFA function */
	MUX_CFG(DA850, EMA_D_7,		9,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_6,		9,	4,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_5,		9,	8,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_4,		9,	12,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_3,		9,	16,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_2,		9,	20,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_1,		9,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_0,		9,	28,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_1,		12,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_2,		12,	20,	15,	1,	true)
	MUX_CFG(DA850, NEMA_CS_3,	7,	4,	15,	1,	true)
	MUX_CFG(DA850, NEMA_CS_4,	7,	8,	15,	1,	true)
	MUX_CFG(DA850, NEMA_WE,		7,	16,	15,	1,	true)
	MUX_CFG(DA850, NEMA_OE,		7,	20,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_0,		12,	28,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_3,		12,	16,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_4,		12,	12,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_5,		12,	8,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_6,		12,	4,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_7,		12,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_8,		11,	28,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_9,		11,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_10,	11,	20,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_11,	11,	16,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_12,	11,	12,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_13,	11,	8,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_14,	11,	4,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_15,	11,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_16,	10,	28,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_17,	10,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_18,	10,	20,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_19,	10,	16,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_20,	10,	12,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_21,	10,	8,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_22,	10,	4,	15,	1,	true)
	MUX_CFG(DA850, EMA_A_23,	10,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_8,		8,	28,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_9,		8,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_10,	8,	20,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_11,	8,	16,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_12,	8,	12,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_13,	8,	8,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_14,	8,	4,	15,	1,	true)
	MUX_CFG(DA850, EMA_D_15,	8,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_BA_1,	5,	24,	15,	1,	true)
	MUX_CFG(DA850, EMA_CLK,		6,	0,	15,	1,	true)
	MUX_CFG(DA850, EMA_WAIT_1,	6,	24,	15,	1,	true)
	MUX_CFG(DA850, NEMA_CS_2,	7,	0,	15,	1,	true)
	/* GPIO function */
	MUX_CFG(DA850, GPIO2_4,		6,	12,	15,	8,	true)
	MUX_CFG(DA850, GPIO2_6,		6,	4,	15,	8,	true)
	MUX_CFG(DA850, GPIO2_8,		5,	28,	15,	8,	true)
	MUX_CFG(DA850, GPIO2_15,	5,	0,	15,	8,	true)
	MUX_CFG(DA850, GPIO3_12,	7,	12,	15,	8,	true)
	MUX_CFG(DA850, GPIO3_13,	7,	8,	15,	8,	true)
	MUX_CFG(DA850, GPIO4_0,		10,	28,	15,	8,	true)
	MUX_CFG(DA850, GPIO4_1,		10,	24,	15,	8,	true)
	MUX_CFG(DA850, GPIO6_9,		13,	24,	15,	8,	true)
	MUX_CFG(DA850, GPIO6_10,	13,	20,	15,	8,	true)
	MUX_CFG(DA850, GPIO6_13,	13,	8,	15,	8,	true)
	MUX_CFG(DA850, GPIO1_4,		4,	12,	15,	8,	true)
	MUX_CFG(DA850, GPIO1_5,		4,	8,	15,	8,	true)
	MUX_CFG(DA850, GPIO0_11,	0,	16,	15,	8,	true)
	MUX_CFG(DA850, RTC_ALARM,	0,	28,	15,	2,	true)
	MUX_CFG(DA850, GPIO7_4,         17,     20,     15,     8,      true)
	/* eHRPWM0 function */
	MUX_CFG(DA850,	EHRPWM0_A,	3,	0,	15,	2,	true)
	MUX_CFG(DA850,	EHRPWM0_B,	3,	4,	15,	2,	true)
	MUX_CFG(DA850,	EHRPWM0_TZ,	1,	0,	15,	2,	true)
	/* eHRPWM1 function */
	MUX_CFG(DA850,	EHRPWM1_A,	5,	0,	15,	2,	true)
	MUX_CFG(DA850,	EHRPWM1_B,	5,	4,	15,	2,	true)
	MUX_CFG(DA850,	EHRPWM1_TZ,	2,	0,	15,	8,	true)
	/* eCAP0 function */
	MUX_CFG(DA850, ECAP0_APWM0,	2,	28,	15,	2,	true)
	/* eCAP1 function */
	MUX_CFG(DA850, ECAP1_APWM1,	1,	28,	15,	4,	true)
	/* eCAP2 function */
	MUX_CFG(DA850, ECAP2_APWM2,	1,	0,	15,	4,	true)
	/* VPIF Capture */
	MUX_CFG(DA850, VPIF_DIN0,	15,	4,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN1,	15,	0,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN2,	14,	28,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN3,	14,	24,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN4,	14,	20,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN5,	14,	16,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN6,	14,	12,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN7,	14,	8,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN8,	16,	4,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN9,	16,	0,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN10,	15,	28,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN11,	15,	24,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN12,	15,	20,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN13,	15,	16,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN14,	15,	12,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DIN15,	15,	8,	15,	1,	true)
        MUX_CFG(DA850, VPIF_CLKIN0,	14,	0,	15,	1,	true)
//      MUX_CFG(DA850, VPIF_CLKIN1,	14,	4,	15,	1,	true)
	MUX_CFG(DA850, VPIF_CLKIN1,	14,	4,	15,	8,	true)
	MUX_CFG(DA850, VPIF_CLKIN2,	19,	8,	15,	1,	true)
	MUX_CFG(DA850, VPIF_CLKIN3,	19,	16,	15,	1,	true)
	/* VPIF Display */
	MUX_CFG(DA850, VPIF_DOUT0,	17,	4,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT1,	17,	0,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT2,	16,	28,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT3,	16,	24,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT4,	16,	20,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT5,	16,	16,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT6,	16,	12,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT7,	16,	8,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT8,	18,	4,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT9,	18,	0,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT10,	17,	28,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT11,	17,	24,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT12,	17,	20,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT13,	17,	16,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT14,	17,	12,	15,	1,	true)
	MUX_CFG(DA850, VPIF_DOUT15,	17,	8,	15,	1,	true)
	MUX_CFG(DA850, VPIF_CLKO2,	19,	12,	15,	1,	true)
	MUX_CFG(DA850, VPIF_CLKO3,	19,	20,	15,	1,	true)
#ifdef CONFIG_MACH_DAVINCI_LEGOEV3
#warning "We're compiling for CONFIG_MACH_DAVINCI_LEGOEV3"
        // NOTE: Because of the way array initializers work, if any SOC_DESCRIPTION
        //       index that's configured below here overlaps the previous mux
        //       configuration entry, it will override the previous entry! The
        //       overrides are marked with "OVERRIDES PREVIOUS!"
        //
        // To OVERRIDE and DISABLE the pinmux setting of a pin that was set higher
        // up in this routine, use MUX_CFG() as usual but make the mask field 0
        // 
	MUX_CFG(EV3, NUART2_CTS,	0,	28,	15,	4,	true) // LEGO BT UART CTRL
	MUX_CFG(EV3, NUART2_RTS,	0,	24,	15,	4,	true) // LEGO BT UART CTRL

	MUX_CFG(EV3, AHCLKR,		0,	16,	15,	1,	true)

	/* PRU functions for soft can */
//	MUX_CFG(DA850, PRU0_R31_0,	7,	28,	15,	0,	true)
//	MUX_CFG(DA850, PRU1_R30_15,	12,	0,	15,	4,	true)
//	MUX_CFG(DA850, PRU1_R31_18,	11,	20,	15,	0,	true)

	/* SPI0 function */
	MUX_CFG(EV3, SPI0_CS_0,	4,	4,	15,	1,	true)
//	MUX_CFG(DA850, SPI0_CLK,	3,	0,	15,	1,	true)
//	MUX_CFG(DA850, SPI0_SOMI,	3,	8,	15,	1,	true)
//	MUX_CFG(DA850, SPI0_SIMO,	3,	12,	15,	1,	true)
	/* SPI1 function */
	MUX_CFG(EV3, SPI1_CS_0,	5,	4,	15,	1,	true)
//	MUX_CFG(DA850, SPI1_CLK,	5,	8,	15,	1,	true)
//	MUX_CFG(DA850, SPI1_SOMI,	5,	16,	15,	1,	true)
//	MUX_CFG(DA850, SPI1_SIMO,	5,	20,	15,	1,	true)
	/* MMC/SD1 function */
	/* GPIO function */
//  	MUX_CFG(DA850, GPIO2_0,		6,	28,	15,	8,	true)
//	MUX_CFG(DA850, GPIO2_11,	5,	16,	15,	8,	true)
//	MUX_CFG(DA850, GPIO2_12,	5,	12,	15,	8,	true)
//	MUX_CFG(DA850, GPIO4_2,		10,	20,	15,	8,	true)
//	MUX_CFG(DA850, GPIO4_9,		9,	24,	15,	8,	true) // LEGO BT Shutdown - EP2
//	MUX_CFG(DA850, GPIO5_0,		12,	28,	15,	8,	true)
//	MUX_CFG(DA850, GPIO7_9,		17,	0,	15,	8,	true)
//	MUX_CFG(DA850, GPIO7_10,	16,	28,	15,	8,	true)
//	MUX_CFG(DA850, GPIO6_3,		19,	12,	15,	8,	true)
//	MUX_CFG(DA850, GPIO6_11,	13,	16,	15,	8,	true)
//	MUX_CFG(DA850, GPIO6_14,	13,	4,	15,	8,	true)
//	MUX_CFG(DA850, GPIO6_15,	13,	0,	15,	8,	true) // LEGO BT ENABLE	
	/* MMC/SD1 function */
//	MUX_CFG(DA850, MMCSD1_DAT_0,	0,	0,	0,	0,	true) // Force no init
//	MUX_CFG(DA850, MMCSD1_DAT_1,	0,	0,	0,	0,	true)
//	MUX_CFG(DA850, MMCSD1_DAT_2,	0,	0,	0,	0,	true)
//	MUX_CFG(DA850, MMCSD1_DAT_3,	0,	0,	0,	0,	true)
//	MUX_CFG(DA850, MMCSD1_CLK,	0,	0,	0,	0,	true)
//	MUX_CFG(DA850, MMCSD1_CMD,	0,	0,	0,	0,	true)
	/* McBSP0 function */
//	MUX_CFG(DA850, MCBSP0_CLKR,	2,	4,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_CLKX,	2,	8,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_FSR,	2,	12,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_FSX,	2,	16,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_DR,	2,	20,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_DX,	2,	24,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP0_CLKS,	2,	28,	15,	0,	true)
	/* McBSP1 function */
//	MUX_CFG(DA850, MCBSP1_CLKR,	1,	4,	15,	2,	true)
//	MUX_CFG(DA850, MCBSP1_CLKX,	1,	8,	15,	2,	true)
 //	MUX_CFG(DA850, MCBSP1_FSR,	1,	12,	15,	2,	true)
 //	MUX_CFG(DA850, MCBSP1_FSX,	1,	16,	15,	2,	true)
 //	MUX_CFG(DA850, MCBSP1_DR,	1,	20,	15,	2,	true)
 //	MUX_CFG(DA850, MCBSP1_DX,	1,	24,	15,	2,	true)
 //	MUX_CFG(DA850, MCBSP1_CLKS,	1,	28,	15,	2,	true)
	/* Bluetooth slow clock */						// LEGO BT
	MUX_CFG(EV3, ECAP2_OUT,		1,	0,	15,	4,	true)  // LEGO BT
	MUX_CFG(EV3, ECAP2_OUT_ENABLE,	0,	12,	15,	8,	true)  // LEGO BT
        /* LEGO EV3 LED PINS */
//      MUX_CFG(EV3,   DIODE_0,        13,      12,     15,     8,      true)  // GPIO6_12
//      MUX_CFG(EV3,   DIODE_1,        13,       4,     15,     8,      true)  // GPIO6_14
//      MUX_CFG(EV3,   DIODE_2,        13,       8,     15,     8,      true)  // GPIO6_13
//      MUX_CFG(EV3,   DIODE_3,        14,       0,     15,     8,      true)  // GPIO6_7
        /* LEGO EV3 BUTTON PINS */
//      MUX_CFG(EV3,   BUTTON_0,       16,       8,     15,     8,      true)  // GPIO7_15
//      MUX_CFG(EV3,   BUTTON_1,        2,       8,     15,     4,      true)  // GPIO1_13
//      MUX_CFG(EV3,   BUTTON_2,       16,      12,     15,     8,      true)  // GPIO7_14
//      MUX_CFG(EV3,   BUTTON_3,       16,      20,     15,     8,      true)  // GPIO7_12
//      MUX_CFG(EV3,   BUTTON_4,       14,       4,     15,     8,      true)  // GPIO6_6
//      MUX_CFG(EV3,   BUTTON_5,       13,      20,     15,     8,      true)  // GPIO6_10
        /* LEGO EV3 MOTOR PINS */
//      MUX_CFG(EV3,   OUTPUTA_PWM,     5,       4,     15,     2,      true)  // GPIO_TO_PIN(2, 14 ) - EPWM1B
//      MUX_CFG(EV3,   OUTPUTA_DIR0,    7,       0,     15,     8,      true)  // GPIO_TO_PIN(3, 15 )
//      MUX_CFG(EV3,   OUTPUTA_DIR1,    8,       4,     15,     8,      true)  // GPIO_TO_PIN(3,  6 )
//      MUX_CFG(EV3,   OUTPUTA_INT,    11,      16,     15,     8,      true)  // GPIO_TO_PIN(5, 11 )
//      MUX_CFG(EV3,   OUTPUTA_DIRA,    1,      12,     15,     8,      true)  // GPIO_TO_PIN(0,  4 )

//      MUX_CFG(EV3,   OUTPUTB_PWM,     5,       0,     15,     2,      true)  // GPIO_TO_PIN(2, 15 ) - EPWM1A
//      MUX_CFG(EV3,   OUTPUTB_DIR0,    6,      24,     15,     8,      true)  // GPIO_TO_PIN(2,  1 )
//      MUX_CFG(EV3,   OUTPUTB_DIR1,    1,      16,     15,     8,      true)  // GPIO_TO_PIN(0,  3 )
//      MUX_CFG(EV3,   OUTPUTB_INT,    11,      28,     15,     8,      true)  // GPIO_TO_PIN(5,  8 )
//      MUX_CFG(EV3,   OUTPUTB_DIRA,    5,      24,     15,     8,      true)  // GPIO_TO_PIN(2,  9 )

//      MUX_CFG(EV3,   OUTPUTC_PWM,     2,      28,     15,     2,      true)  // GPIO_TO_PIN(8,  7 ) - APWM0
//      MUX_CFG(EV3,   OUTPUTC_DIR0,   13,      28,     15,     8,      true)  // GPIO_TO_PIN(6,  8 )
//      MUX_CFG(EV3,   OUTPUTC_DIR1,   11,      24,     15,     8,      true)  // GPIO_TO_PIN(5,  9 )
//      MUX_CFG(EV3,   OUTPUTC_INT,    11,       8,     15,     8,      true)  // GPIO_TO_PIN(5, 13 )
//      MUX_CFG(EV3,   OUTPUTC_DIRA,    7,       4,     15,     8,      true)  // GPIO_TO_PIN(3, 14 )

// #warning "Should the pin mode for this pin actually be 4 instead of 2?????"
//      MUX_CFG(EV3,   OUTPUTD_PWM,     1,      28,     15,     4,      true)  // GPIO_TO_PIN(0,  0 ) - APWM1
//      MUX_CFG(EV3,   OUTPUTD_DIR0,   12,      16,     15,     8,      true)  // GPIO_TO_PIN(5,  3 )
//      MUX_CFG(EV3,   OUTPUTD_DIR1,   11,      20,     15,     8,      true)  // GPIO_TO_PIN(5, 10 )
//      MUX_CFG(EV3,   OUTPUTD_INT,    13,      24,     15,     8,      true)  // GPIO_TO_PIN(6,  9 )
//      MUX_CFG(EV3,   OUTPUTD_DIRA,    5,      28,     15,     8,      true)  // GPIO_TO_PIN(2,  8 )

// New GPIO MUX definitions from LEGO AM1808.h file including where-used description
//
// Notes:
//
// Input UART 3 - RXD is not connected to any IO
// Input UART 4 - RXD is not connected to any IO
// Test Pin     - Depends on ENABLE_TEST_ON_PORT4 ? GP1_15 : GP2_7
// Sound Pins   - Assuming HARDWARE = ONE2ONE
// PWM Pins     - None of the ports have the sleep or fault pins enabled
// 
// CONFLICT!    - 5VONIGEN         & DIODE2  (GPIO6_14) - Turns out 5VONIGEN is not used in the code!
//                ECAP2_OUT        & GPIO0_7            - Bluetooth clock?
//                ECAP2_OUT_ENABLE & GPIO0_12           - Bluetooth clock?
//
// Double check the SPI0/1 CS pins!
//
// Investigate macros to enable/disable internal pullups

        MUX_CFG(EV3,   GPIO0_1,         1,      24,     15,     8,      true) // Input UART 4  - DIGID0
                                                                              // Analog Port 4 - Pin 5 - DIGID0 - Digital input/output
        MUX_CFG(EV3,   GPIO0_2,         1,      20,     15,     8,      true) // Input UART 1  - DIGIA0
                                                                              // Analog Port 1 - Pin 5 - DIGIA0 - Digital input/output
        MUX_CFG(EV3,   GPIO0_3,         1,      16,     15,     8,      true) // PWM Port B    - DIR1 B
                                                                              // Analog Port 2 - MAIN0
        MUX_CFG(EV3,   GPIO0_4,         1,      12,     15,     8,      true) // PWM Port A    - DIR A 
                                                                              // Analog Port 1 - DIRA
        MUX_CFG(EV3,   GPIO0_5,         1,       8,     15,     8,      true) // Unused - GPIO_TO_PIN(0,  5)
        MUX_CFG(EV3,   GPIO0_6,         1,       4,     15,     8,      true) // ADC Power     - ADCBATEN
        MUX_CFG(EV3,   GPIO0_7,         1,       0,     15,     8,      true) // Bluetooth ECAP2OUT - GPIO_TO_PIN(0,  7)
        MUX_CFG(EV3,   GPIO0_11,        0,      16,     15,     8,      true) // Unused - GPIO_TO_PIN(0, 11)
        MUX_CFG(EV3,   GPIO0_12,        0,      12,     15,     8,      true) // Input UART 3 - DIGIC0
                                                                              // Analog Port 3 - Pin 5 - DIGIC0 - Digital input/output
                                                                              // Bluetooth ECAP2_OUT_ENABLE
        MUX_CFG(EV3,   GPIO0_13,        0,       8,     15,     8,      true) // Input UART 2 - DIGIB1
                                                                              // I2C Port 2   - DATA
                                                                              // Analog Port 2 - Pin 5 - DIGIB1 - Digital input/output
        MUX_CFG(EV3,   GPIO0_14,        0,       4,     15,     8,      true) // Input UART 2 - DIGIB0
                                                                              // Analog Port 2 - Pin 5 - DIGIB0 - Digital input/output
        MUX_CFG(EV3,   GPIO0_15,        0,       0,     15,     8,      true) // I2C Port 1 - DATA
                                                                              // Analog Port 1 - Pin 6 - DIGIA1 - Digital input/output
	MUX_CFG(EV3,   GPIO1_4,		4,	12,	15,	8,	true) // ON_BD_USB_DRV - GPIO_TO_PIN(1,  4) 
        MUX_CFG(EV3,   GPIO1_0,         4,      28,     15,     8,      true) // I2C Port 1 - CLOCK
        MUX_CFG(EV3,   GPIO1_8,         3,       0,     15,     4,      true) // ADC SPI ???  - ADCCLK
        MUX_CFG(EV3,   GPIO1_9,         2,      24,     15,     4,      true) // Unused - GPIO_TO_PIN(1,  9)
        MUX_CFG(EV3,   GPIO1_10,        2,      20,     15,     4,      true) // Unused - GPIO_TO_PIN(1, 10)
        MUX_CFG(EV3,   GPIO1_11,        2,      16,     15,     4,      true) // I2C Port 4 - CLOCK
        MUX_CFG(EV3,   GPIO1_12,        2,      12,     15,     4,      true) // I2C Port 3 - CLOCK
        MUX_CFG(EV3,   GPIO1_13,        2,       8,     15,     4,      true) // UI BUTTONS - BUT1
        MUX_CFG(EV3,   GPIO1_14,        2,       4,     15,     4,      true) // Input UART 3 - DIGIC1
                                                                              // I2C Port 3   - DATA
                                                                              // Analog Port 3 - Pin 5 - DIGIC1 - Digital input/output
        MUX_CFG(EV3,   GPIO1_15,        2,       0,     15,     8,      true) // Input UART 4 - DIGID1
                                                                              // I2C Port 4   - DATA
                                                                              // Analog Port 4 - Pin 5 - DIGID1 - Digital input/output
        MUX_CFG(EV3,   GPIO2_0,         6,      28,     15,     8,      true) // Unused - GPIO_TO_PIN(2,  0) - Was PWM AB Fault pre-EP2
        MUX_CFG(EV3,   GPIO2_1,         6,      24,     15,     8,      true) // PWM Port B   - DIR0 B
                                                                              // Analog Port 2 - MAIN1
        MUX_CFG(EV3,   GPIO2_2,         6,      20,     15,     8,      true) // Analog Port 1 - Pin 2 - LEGDETA - Digital input pulled up
                                         
        MUX_CFG(EV3,   GPIO2_4,         6,      12,     15,     8,      true) // Unused GPIO_TO_PIN(2,  4)
        MUX_CFG(EV3,   GPIO2_5,         6,       8,     15,     8,      true) // Analog Port 2 - DETB0 TP19
        MUX_CFG(EV3,   GPIO2_6,         6,       4,     15,     8,      true) // Unused - GPIO_TO_PIN(2,  6)
        MUX_CFG(EV3,   GPIO2_7,         6,       0,     15,     8,      true) // Unused - GPIO_TO_PIN(2,  7) - Test Pin
        MUX_CFG(EV3,   GPIO2_8,         5,      28,     15,     8,      true) // PWM Port D   - DIR  D
                                                                              // Analog Port 4 - DIRD
        MUX_CFG(EV3,   GPIO2_9,         5,      24,     15,     8,      true) // PWM Port B   - DIR  B
                                                                              // Analog Port 2 - DIRB
        MUX_CFG(EV3,   GPIO2_10,        5,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(2, 10)
//      MUX_CFG(EV3,   GPIO2_11,        5,      16,     15,     8,      true) // Unused - GPIO_TO_PIN(2, 11)
//      MUX_CFG(EV3,   GPIO2_12,        5,      12,     15,     8,      true) // Unused - GPIO_TO_PIN(2, 12)
        MUX_CFG(EV3,   GPIO2_13,        5,       8,     15,     8,      true) // Unused - GPIO_TO_PIN(2, 13)
        MUX_CFG(EV3,   GPIO3_0,         8,      28,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  0)
        MUX_CFG(EV3,   GPIO3_1,         8,      24,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  1)
        MUX_CFG(EV3,   GPIO3_2,         8,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  2) - Was DETC0 TP20 pre-EP2
        MUX_CFG(EV3,   GPIO3_3,         8,      16,     15,     8,      true) // Bluetooth  - PIC_EN
        MUX_CFG(EV3,   GPIO3_4,         8,      12,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  4)
        MUX_CFG(EV3,   GPIO3_5,         8,       8,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  5)
        MUX_CFG(EV3,   GPIO3_6,         8,       4,     15,     8,      true) // PWM Port A - DIR1 A
                                                                              // Analog Port 1 - MAIN1
        MUX_CFG(EV3,   GPIO3_7,         8,       0,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  7)
        MUX_CFG(EV3,   GPIO3_8,         7,      28,     15,     8,      true) // Analog Port 3 - DETC0 TP20
        MUX_CFG(EV3,   GPIO3_9,         7,      24,     15,     8,      true) // Unused - GPIO_TO_PIN(3,  9)
        MUX_CFG(EV3,   GPIO3_10,        7,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(3, 10) - Was PWM AB Sleep pre-EP2
        MUX_CFG(EV3,   GPIO3_11,        7,      16,     15,     8,      true) // Unused - GPIO_TO_PIN(3, 11)
        MUX_CFG(EV3,   GPIO3_12,        7,      12,     15,     8,      true) // Unused - GPIO_TO_PIN(3, 12)
        MUX_CFG(EV3,   GPIO3_13,        7,       8,     15,     8,      true) // Unused - GPIO_TO_PIN(3, 13)
        MUX_CFG(EV3,   GPIO3_14,        7,       4,     15,     8,      true) // PWM Port C - DIR  C
                                                                              // Analog Port 3 - DIRC
        MUX_CFG(EV3,   GPIO3_15,        7,       0,     15,     8,      true) // PWM Port A - DIR0 A
                                                                              // Analog Port 1 - MAIN0
        MUX_CFG(EV3,   GPIO4_1,        10,      24,     15,     8,      true) // Bluetooth BT_SHUT_DOWN - GPIO_TO_PIN(4,  1)
        MUX_CFG(EV3,   GPIO4_8,         9,      28,     15,     8,      true) // Unused - GPIO_TO_PIN(4,  8)
        MUX_CFG(EV3,   GPIO4_9,         9,      24,     15,     8,      true) // Bluetooth BT_SHUTDOWN_EP2  - GPIO_TO_PIN(4,  9)
        MUX_CFG(EV3,   GPIO4_10,        9,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(4, 10)
        MUX_CFG(EV3,   GPIO4_12,        9,      12,     15,     8,      true) // Unused - GPIO_TO_PIN(4, 12) - Was PWM DIR1A pre-EP2
        MUX_CFG(EV3,   GPIO4_14,        9,       4,     15,     8,      true) // Bluetooth  - PIC_RST
        MUX_CFG(EV3,   GPIO5_0,        12,      28,     15,     8,      true) // LCD SPI - ??? - GPIO_TO_PIN(5,  0)
        MUX_CFG(EV3,   GPIO5_1,        12,      24,     15,     8,      true) // Unused - GPIO_TO_PIN(5,  1)
        MUX_CFG(EV3,   GPIO5_2,        12,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(5,  2)
        MUX_CFG(EV3,   GPIO5_3,        12,      16,     15,     8,      true) // PWM Port D - DIR0 D
                                                                              // Analog Port 4 - MAIN1
        MUX_CFG(EV3,   GPIO5_4,        12,      12,     15,     8,      true) // Analog Port 1 - DETA0 TP18
        MUX_CFG(EV3,   GPIO5_5,        12,       8,     15,     8,      true) // Unused - GPIO_TO_PIN(5,  5)
        MUX_CFG(EV3,   GPIO5_6,        12,       4,     15,     8,      true) // Unused - GPIO_TO_PIN(5,  6)
        MUX_CFG(EV3,   GPIO5_7,        12,       0,     15,     8,      true) // Bluetooth  - CTS_PIC
        MUX_CFG(EV3,   GPIO5_8,        11,      28,     15,     8,      true) // PWM Port B   - INT B
                                                                              // Analog Port 2 - INTB
        MUX_CFG(EV3,   GPIO5_9,        11,      24,     15,     8,      true) // PWM Port C - DIR1 C
                                                                              // Analog Port 3 - MAIN1
        MUX_CFG(EV3,   GPIO5_10,       11,      20,     15,     8,      true) // PWM Port D - DIR1 D
                                                                              // Analog Port 4 - MAIN0
        MUX_CFG(EV3,   GPIO5_11,       11,      16,     15,     8,      true) // PWM Port A - INT A
                                                                              // Analog Port 1 - INTA
        MUX_CFG(EV3,   GPIO5_12,       11,      12,     15,     8,      true) // Unused - GPIO_TO_PIN(5, 12)
        MUX_CFG(EV3,   GPIO5_13,       11,       8,     15,     8,      true) // PWM Port C - INT C
                                                                              // Analog Port 3 - INTC
        MUX_CFG(EV3,   GPIO5_14,       11,       4,     15,     8,      true) // Unused - GPIO_TO_PIN(5, 14)
        MUX_CFG(EV3,   GPIO5_15,       11,       0,     15,     8,      true) // Analog Port 4 - DETD0 TP21
        MUX_CFG(EV3,   GPIO6_0,        19,      24,     15,     8,      true) // Unused - GPIO_TO_PIN(6,  0) - Was PWM CD Fault pre-EP2
        MUX_CFG(EV3,   GPIO6_1,        19,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(6,  1)
        MUX_CFG(EV3,   GPIO6_2,        19,      16,     15,     8,      true) // POWER      - ADCACK
        MUX_CFG(EV3,   GPIO6_3,        19,      12,     15,     8,      true) // ON_BD_USB_OVC - GPIO_TO_PIN(6,  3)
        MUX_CFG(EV3,   GPIO6_4,        19,       8,     15,     8,      true) // Analog Port 4 - Pin 1 - I_OND - 9V Enable (high)
        MUX_CFG(EV3,   GPIO6_5,        16,       4,     15,     8,      true) // POWER      - P_EN
        MUX_CFG(EV3,   GPIO6_6,        14,       4,     15,     8,      true) // UI BUTTONS - BUT4
        MUX_CFG(EV3,   GPIO6_7,        14,       0,     15,     8,      true) // UI LEDS    - DIODE 0
        MUX_CFG(EV3,   GPIO6_8,        13,      28,     15,     8,      true) // PWM Port C - DIR0 C
                                                                              // Analog Port 3 - MAIN0
        MUX_CFG(EV3,   GPIO6_9,        13,      24,     15,     8,      true) // PWM Port D - INT  D
                                                                              // Analog Port 4 - INTD
        MUX_CFG(EV3,   GPIO6_10,       13,      20,     15,     8,      true) // UI BUTTONS - BUT5
        MUX_CFG(EV3,   GPIO6_11,       13,      16,     15,     8,      true) // POWER      - 5VPENON
        MUX_CFG(EV3,   GPIO6_12,       13,      12,     15,     8,      true) // UI LEDS    - DIODE 1
        MUX_CFG(EV3,   GPIO6_13,       13,       8,     15,     8,      true) // UI LEDS    - DIODE 3
        MUX_CFG(EV3,   GPIO6_14,       13,       4,     15,     8,      true) // UI LEDS    - DIODE 2
                                                                              // ADC Power  - 5VONIGEN
        MUX_CFG(EV3,   GPIO6_15,       13,       0,     15,     8,      true) // Sound - SOUNDEN
        MUX_CFG(EV3,   GPIO7_4,        17,      20,     15,     8,      true) // Unused - GPIO_TO_PIN(7,  4) - Was SOUNDEN pre-EP2
        MUX_CFG(EV3,   GPIO7_8,        17,       4,     15,     8,      true) // Analog Port 4 - Pin 2 - LEGDETD - Digital input pulled up
        MUX_CFG(EV3,   GPIO7_9,        17,       0,     15,     8,      true) // Input UART 3 - Buffer disable
                                                                              // I2C Port 3   - Buffer disabble
                                                                              // Analog Port 3 - Buffer disable
        MUX_CFG(EV3,   GPIO7_10,       16,      28,     15,     8,      true) // Input UART 4 - Buffer disable
                                                                              // I2C Port 4   - Buffer disabble
                                                                              // Analog Port 4 - Buffer disable
        MUX_CFG(EV3,   GPIO7_11,       16,      24,     15,     8,      true) // Analog Port 3 - Pin 2 - LEGDETC - Digital input pulled up
        MUX_CFG(EV3,   GPIO7_12,       16,      20,     15,     8,      true) // UI BUTTONS - BUT3
        MUX_CFG(EV3,   GPIO7_13,       16,      16,     15,     8,      true) // Unused - GPIO_TO_PIN(7, 13)
        MUX_CFG(EV3,   GPIO7_14,       16,      12,     15,     8,      true) // UI BUTTONS - BUT2
        MUX_CFG(EV3,   GPIO7_15,       16,       8,     15,     8,      true) // UI BUTTONS - BUT0
        MUX_CFG(EV3,   GPIO8_2,         3,      24,     15,     4,      true) // ADC SPI ???  - ADCCS
        MUX_CFG(EV3,   GPIO8_3,         3,      20,     15,     4,      true) // I2C Port 2 - CLOCK
        MUX_CFG(EV3,   GPIO8_5,         3,      12,     15,     4,      true) // ADC SPI ???  - ADCMOSI
        MUX_CFG(EV3,   GPIO8_6,         3,       8,     15,     4,      true) // ADC SPI ???  - ADCMISO
        MUX_CFG(EV3,   GPIO8_8,        19,       4,     15,     8,      true) // POWER        - SW_RECHARGE
        MUX_CFG(EV3,   GPIO8_9,        19,       0,     15,     8,      true) // Analog Port 3 - Pin 1 - I_ONC - 9V Enable (high)
        MUX_CFG(EV3,   GPIO8_10,       18,      28,     15,     8,      true) // Analog Port 1 - Pin 1 - I_ONA - 9V Enable (high)
        MUX_CFG(EV3,   GPIO8_11,       18,      24,     15,     8,      true) // Input UART 1 - Buffer disable
                                                                              // I2C Port 1   - Buffer disabble
                                                                              // Analog Port 1 - Buffer disable
        MUX_CFG(EV3,   GPIO8_12,       18,      20,     15,     8,      true) // Analog Port 2 - Pin 1 - I_ONB - 9V Enable (high)
        MUX_CFG(EV3,   GPIO8_13,       18,      16,     15,     8,      true) // Unused - GPIO_TO_PIN(8, 13)
        MUX_CFG(EV3,   GPIO8_14,       18,      12,     15,     8,      true) // Input UART 2 - Buffer disable
                                                                              // I2C Port 2   - Buffer disabble
                                                                              // Analog Port 2 - Buffer disable
        MUX_CFG(EV3,   GPIO8_15,       18,       8,     15,     8,      true) // Analog Port 2 - Pin 2 - LEGDETB - Digital input pulled up

        MUX_CFG(EV3,   UART0_TXD,       3,      20,     15,     2,      true) // Input UART 2 - TXD
        MUX_CFG(EV3,   UART0_RXD,       3,      16,     15,     2,      true) // Input UART 2 - XXD
        MUX_CFG(EV3,   UART1_TXD,       4,      28,     15,     2,      true) // Input UART 1 - TXD
        MUX_CFG(EV3,   UART1_RXD,       4,      24,     15,     2,      true) // Input UART 1 - RXD
        MUX_CFG(EV3,   SPI0_MOSI,       3,      12,     15,     1,      true) // ADC SPI      - ADCMOSI
        MUX_CFG(EV3,   SPI0_MISO,       3,       8,     15,     1,      true) // ADC SPI      - ADCMISO
        MUX_CFG(EV3,   SPI0_SCL,        3,       0,     15,     1,      true) // ADC SPI      - ADCCLK
        MUX_CFG(EV3,   SPI0_CS,         3,      24,     15,     1,      true) // ADC SPI      - ADCCS
        MUX_CFG(EV3,   SPI1_MOSI,       5,      20,     15,     1,      true) // 
        MUX_CFG(EV3,   SPI1_MISO,       5,      16,     15,     8,      true) // LCD SPI - Yes, was also GPIO2_11
        MUX_CFG(EV3,   SPI1_SCL,        5,       8,     15,     1,      true) // 
        MUX_CFG(EV3,   SPI1_CS,         5,      12,     15,     8,      true) // LCD SPI - Yes, was also GPIO2_12
        MUX_CFG(EV3,   EPWM1A,          5,       0,     15,     2,      true) // PWM Port B - PWM Motor B
        MUX_CFG(EV3,   EPWM1B,          5,       4,     15,     2,      true) // PWM Port A - PWM Motor A
        MUX_CFG(EV3,   APWM0,           2,      28,     15,     2,      true) // PWM Port C - PWM Motor C
        MUX_CFG(EV3,   APWM1,           1,      28,     15,     4,      true) // PWM Port D - PWM Motor D
        MUX_CFG(EV3,   EPWM0B,          3,       4,     15,     2,      true) // SOUND - SOUND_ARMA
        MUX_CFG(EV3,   AXR3,            2,      16,     15,     1,      true) // Input UART 4 - TXD
        MUX_CFG(EV3,   AXR4,            2,      12,     15,     1,      true) // Input UART 3 - TXD
#endif
#endif
};

/* ev3dev not needed
const short da850_pru_can_pins[] __initdata = {
	DA850_GPIO7_9,DA850_GPIO7_10,DA850_GPIO2_0, DA850_PRU0_R31_0, DA850_PRU1_R30_15,
	DA850_PRU1_R31_18,
	-1
};

const short da850_pru_suart_pins[] __initdata = {
	DA850_AHCLKX, DA850_ACLKX, DA850_AFSX,
    DA850_AHCLKR, DA850_ACLKR, DA850_AFSR,
    DA850_AXR_1, DA850_AXR_2, DA850_AXR_3,
	DA850_AXR_4, 
	-1
};
*/

/* TI
const short da850_pru_suart_pins[] __initdata = {
	DA850_AHCLKX, DA850_ACLKX, DA850_AFSX,
    DA850_AHCLKR, DA850_ACLKR, DA850_AFSR,
    DA850_AXR_13, DA850_AXR_9, DA850_AXR_7,
	DA850_AXR_14, DA850_AXR_10, DA850_AXR_8,
	-1
};
*/

/* ev3dev not needed
const short da850_uart1_pins[] __initdata = {
	DA850_UART1_RXD, DA850_UART1_TXD,
	DA850_NUART1_CTS,DA850_NUART1_RTS,
	-1
};
*/

//const short da850_uart2_pins[] __initdata = {
//	DA850_UART2_RXD, DA850_UART2_TXD,
//	-1
//};

/* ev3dev not needed
const short da850_uart2_pins[] __initdata = {
	DA850_UART2_RXD, DA850_UART2_TXD,
	DA850_NUART2_CTS, DA850_NUART2_RTS,     // LEGO BT
//#ifdef CONFIG_WIFI_CONTROL_FUNC                 // LEGO BT
        DA850_GPIO0_15,                         // LEGO BT
//#endif                                          // LEGO BT

	-1
};


const short da850_i2c0_pins[] __initdata = {
//	DA850_GPIO1_4, DA850_GPIO1_5,
	DA850_I2C0_SCL, DA850_I2C0_SDA,
	-1
};

const short da850_i2c1_pins[] __initdata = {
	DA850_I2C1_SCL, DA850_I2C1_SDA,
	-1
};

const short da850_cpgmac_pins[] __initdata = {
	DA850_MII_TXEN, DA850_MII_TXCLK, DA850_MII_COL, DA850_MII_TXD_3,
	DA850_MII_TXD_2, DA850_MII_TXD_1, DA850_MII_TXD_0, DA850_MII_RXER,
	DA850_MII_CRS, DA850_MII_RXCLK, DA850_MII_RXDV, DA850_MII_RXD_3,
	DA850_MII_RXD_2, DA850_MII_RXD_1, DA850_MII_RXD_0, DA850_MDIO_CLK,
	DA850_MDIO_D,
	-1
};

const short da850_rmii_pins[] __initdata = {
	DA850_RMII_TXD_0, DA850_RMII_TXD_1, DA850_RMII_TXEN,
	DA850_RMII_CRS_DV, DA850_RMII_RXD_0, DA850_RMII_RXD_1,
	DA850_RMII_RXER, DA850_RMII_MHZ_50_CLK, DA850_MDIO_CLK,
	DA850_MDIO_D,
	-1
};

const short da850_mcasp_pins[] __initdata = {
	DA850_AHCLKX, DA850_ACLKX, DA850_AFSX,
	DA850_AHCLKR, DA850_ACLKR, DA850_AFSR, DA850_AMUTE,
	DA850_AXR_11, DA850_AXR_12,
	-1
};

const short da850_lcdcntl_pins[] __initdata = {
	DA850_LCD_D_0, DA850_LCD_D_1, DA850_LCD_D_2, DA850_LCD_D_3,
	DA850_LCD_D_4, DA850_LCD_D_5, DA850_LCD_D_6, DA850_LCD_D_7,
	DA850_LCD_D_8, DA850_LCD_D_9, DA850_LCD_D_10, DA850_LCD_D_11,
	DA850_LCD_D_12, DA850_LCD_D_13, DA850_LCD_D_14, DA850_LCD_D_15,
	DA850_LCD_PCLK, DA850_LCD_MCLK, DA850_LCD_HSYNC, DA850_LCD_VSYNC,
	DA850_NLCD_AC_ENB_CS,
	-1
};

const short da850_mmcsd0_pins[] __initdata = {
	DA850_MMCSD0_DAT_0, DA850_MMCSD0_DAT_1, DA850_MMCSD0_DAT_2,
	DA850_MMCSD0_DAT_3, DA850_MMCSD0_CLK, DA850_MMCSD0_CMD,
	DA850_GPIO4_0, DA850_GPIO4_1,
	-1
};
*/

//Lego - DAT3 is CD ..DAT3 is Muxed with GPIO_$_2

/*const short da850_mmcsd0_pins[] __initdata = {
	DA850_MMCSD0_DAT_0, DA850_MMCSD0_DAT_1, DA850_MMCSD0_DAT_2,
	DA850_MMCSD0_CLK, DA850_MMCSD0_CMD,
	DA850_GPIO4_2, DA850_GPIO4_1,
	-1
};*/

/* ev3dev not needed
const short da850_nand_pins[] __initdata = {
	DA850_EMA_D_7, DA850_EMA_D_6, DA850_EMA_D_5, DA850_EMA_D_4,
	DA850_EMA_D_3, DA850_EMA_D_2, DA850_EMA_D_1, DA850_EMA_D_0,
	DA850_EMA_A_1, DA850_EMA_A_2, DA850_NEMA_CS_3, DA850_NEMA_CS_4,
	DA850_NEMA_WE, DA850_NEMA_OE,
	-1
};

const short da850_nor_pins[] __initdata = {
	DA850_EMA_BA_1, DA850_EMA_CLK, DA850_EMA_WAIT_1, DA850_NEMA_CS_2,
	DA850_NEMA_WE, DA850_NEMA_OE, DA850_EMA_D_0, DA850_EMA_D_1,
	DA850_EMA_D_2, DA850_EMA_D_3, DA850_EMA_D_4, DA850_EMA_D_5,
	DA850_EMA_D_6, DA850_EMA_D_7, DA850_EMA_D_8, DA850_EMA_D_9,
	DA850_EMA_D_10, DA850_EMA_D_11, DA850_EMA_D_12, DA850_EMA_D_13,
	DA850_EMA_D_14, DA850_EMA_D_15, DA850_EMA_A_0, DA850_EMA_A_1,
	DA850_EMA_A_2, DA850_EMA_A_3, DA850_EMA_A_4, DA850_EMA_A_5,
	DA850_EMA_A_6, DA850_EMA_A_7, DA850_EMA_A_8, DA850_EMA_A_9,
//	DA850_EMA_A_10,
        DA850_EMA_A_11, DA850_EMA_A_12, DA850_EMA_A_13,
	DA850_EMA_A_14, DA850_EMA_A_15, DA850_EMA_A_16, DA850_EMA_A_17,
	DA850_EMA_A_18, DA850_EMA_A_19, DA850_EMA_A_20, DA850_EMA_A_21,
	DA850_EMA_A_22, DA850_EMA_A_23,
	-1
};
*/

/* ev3dev not needed

const short da850_spi0_pins[] __initdata = {
	DA850_SPI0_CS_0, DA850_SPI0_CLK, DA850_SPI0_SOMI, DA850_SPI0_SIMO,
	-1
};

const short da850_spi1_pins[] __initdata = {
	DA850_SPI1_CS_0, DA850_SPI1_CLK, DA850_SPI1_SOMI, DA850_SPI1_SIMO,
	-1
};

const short da850_mcbsp0_pins[] __initdata = {
	DA850_MCBSP0_CLKR, DA850_MCBSP0_CLKX, DA850_MCBSP0_FSR,
	DA850_MCBSP0_FSX, DA850_MCBSP0_DR, DA850_MCBSP0_DX, DA850_MCBSP0_CLKS,
	-1
};

const short da850_mcbsp1_pins[] __initdata = {
	DA850_MCBSP1_CLKR, DA850_MCBSP1_CLKX, DA850_MCBSP1_FSR,
	DA850_MCBSP1_FSX, DA850_MCBSP1_DR, DA850_MCBSP1_DX, DA850_MCBSP1_CLKS,
	-1
};
*/

// const short da850_ehrpwm0_pins[] __initdata = {
// 	DA850_EHRPWM0_A, DA850_EHRPWM0_B, DA850_EHRPWM0_TZ,
// 	-1
// };
// 
// const short da850_ehrpwm1_pins[] __initdata = {
// 	DA850_EHRPWM1_A, DA850_EHRPWM1_TZ,
// 	-1
// };
// 
// const short da850_uart1_pins[] __initdata = {
// 	DA850_UART1_RXD, DA850_UART1_TXD,
// #ifdef CONFIG_DAVINCI_UART1_AFE
// 	DA850_NUART1_CTS, DA850_NUART1_RTS,
// #endif
// 	-1
// };

/* ev3dev - not needed
const short da850_vpif_capture_pins[] __initdata = {
	DA850_VPIF_DIN0, DA850_VPIF_DIN1, DA850_VPIF_DIN2, DA850_VPIF_DIN3,
	DA850_VPIF_DIN4, DA850_VPIF_DIN5, DA850_VPIF_DIN6, DA850_VPIF_DIN7,
	DA850_VPIF_DIN8, DA850_VPIF_DIN9, DA850_VPIF_DIN10, DA850_VPIF_DIN11,
	DA850_VPIF_DIN12, DA850_VPIF_DIN13, DA850_VPIF_DIN14, DA850_VPIF_DIN15,
	DA850_VPIF_CLKIN0, DA850_VPIF_CLKIN1, DA850_VPIF_CLKIN2,
	DA850_VPIF_CLKIN3,
	-1
};

const short da850_vpif_display_pins[] __initdata = {
	DA850_VPIF_DOUT0, DA850_VPIF_DOUT1, DA850_VPIF_DOUT2, DA850_VPIF_DOUT3,
	DA850_VPIF_DOUT4, DA850_VPIF_DOUT5, DA850_VPIF_DOUT6, DA850_VPIF_DOUT7,
	DA850_VPIF_DOUT8, DA850_VPIF_DOUT9, DA850_VPIF_DOUT10,
	DA850_VPIF_DOUT11, DA850_VPIF_DOUT12, DA850_VPIF_DOUT13,
	DA850_VPIF_DOUT14, DA850_VPIF_DOUT15, DA850_VPIF_CLKO2,
	DA850_VPIF_CLKO3,
	-1
};

const short da850_evm_usb11_pins[] __initdata = {
//	DA850_GPIO6_11, DA850_GPIO6_14, -1
	DA850_GPIO6_3, -1
};

const short da850_sata_pins[] __initdata = {
	-1
};
*/

/* FIQ are pri 0-1; otherwise 2-7, with 7 lowest priority */
static u8 da850_default_priorities[DA850_N_CP_INTC_IRQ] = {
	[IRQ_DA8XX_COMMTX]		= 7,
	[IRQ_DA8XX_COMMRX]		= 7,
	[IRQ_DA8XX_NINT]		= 7,
	[IRQ_DA8XX_EVTOUT0]		= 7,
	[IRQ_DA8XX_EVTOUT1]		= 7,
	[IRQ_DA8XX_EVTOUT2]		= 7,
	[IRQ_DA8XX_EVTOUT3]		= 7,
	[IRQ_DA8XX_EVTOUT4]		= 7,
	[IRQ_DA8XX_EVTOUT5]		= 7,
	[IRQ_DA8XX_EVTOUT6]		= 7,
	[IRQ_DA8XX_EVTOUT6]		= 7,
	[IRQ_DA8XX_EVTOUT7]		= 7,
	[IRQ_DA8XX_CCINT0]		= 7,
	[IRQ_DA8XX_CCERRINT]		= 7,
	[IRQ_DA8XX_TCERRINT0]		= 7,
	[IRQ_DA8XX_AEMIFINT]		= 7,
	[IRQ_DA8XX_I2CINT0]		= 7,
	[IRQ_DA8XX_MMCSDINT0]		= 7,
	[IRQ_DA8XX_MMCSDINT1]		= 7,
	[IRQ_DA8XX_ALLINT0]		= 7,
	[IRQ_DA8XX_RTC]			= 7,
	[IRQ_DA8XX_SPINT0]		= 7,
	[IRQ_DA8XX_TINT12_0]		= 7,
	[IRQ_DA8XX_TINT34_0]		= 7,
	[IRQ_DA8XX_TINT12_1]		= 7,
	[IRQ_DA8XX_TINT34_1]		= 0,
	[IRQ_DA8XX_UARTINT0]		= 7,
	[IRQ_DA8XX_KEYMGRINT]		= 7,
	[IRQ_DA8XX_SECINT]		= 7,
	[IRQ_DA8XX_SECKEYERR]		= 7,
	[IRQ_DA850_MPUADDRERR0]		= 7,
	[IRQ_DA850_MPUPROTERR0]		= 7,
	[IRQ_DA850_IOPUADDRERR0]	= 7,
	[IRQ_DA850_IOPUPROTERR0]	= 7,
	[IRQ_DA850_IOPUADDRERR1]	= 7,
	[IRQ_DA850_IOPUPROTERR1]	= 7,
	[IRQ_DA850_IOPUADDRERR2]	= 7,
	[IRQ_DA850_IOPUPROTERR2]	= 7,
	[IRQ_DA850_BOOTCFG_ADDR_ERR]	= 7,
	[IRQ_DA850_BOOTCFG_PROT_ERR]	= 7,
	[IRQ_DA850_MPUADDRERR1]		= 7,
	[IRQ_DA850_MPUPROTERR1]		= 7,
	[IRQ_DA850_IOPUADDRERR3]	= 7,
	[IRQ_DA850_IOPUPROTERR3]	= 7,
	[IRQ_DA850_IOPUADDRERR4]	= 7,
	[IRQ_DA850_IOPUPROTERR4]	= 7,
	[IRQ_DA850_IOPUADDRERR5]	= 7,
	[IRQ_DA850_IOPUPROTERR5]	= 7,
	[IRQ_DA850_MIOPU_BOOTCFG_ERR]	= 7,
//	[IRQ_DA850_MPUADDRERR0]		= 7,
	[IRQ_DA8XX_CHIPINT0]		= 7,
	[IRQ_DA8XX_CHIPINT1]		= 7,
	[IRQ_DA8XX_CHIPINT2]		= 7,
	[IRQ_DA8XX_CHIPINT3]		= 7,
	[IRQ_DA8XX_TCERRINT1]		= 7,
	[IRQ_DA8XX_C0_RX_THRESH_PULSE]	= 7,
	[IRQ_DA8XX_C0_RX_PULSE]		= 7,
	[IRQ_DA8XX_C0_TX_PULSE]		= 7,
	[IRQ_DA8XX_C0_MISC_PULSE]	= 7,
	[IRQ_DA8XX_C1_RX_THRESH_PULSE]	= 7,
	[IRQ_DA8XX_C1_RX_PULSE]		= 7,
	[IRQ_DA8XX_C1_TX_PULSE]		= 7,
	[IRQ_DA8XX_C1_MISC_PULSE]	= 7,
	[IRQ_DA8XX_MEMERR]		= 7,
	[IRQ_DA8XX_GPIO0]		= 7,
	[IRQ_DA8XX_GPIO1]		= 7,
	[IRQ_DA8XX_GPIO2]		= 7,
	[IRQ_DA8XX_GPIO3]		= 7,
	[IRQ_DA8XX_GPIO4]		= 7,
	[IRQ_DA8XX_GPIO5]		= 2,
	[IRQ_DA8XX_GPIO6]		= 2,
	[IRQ_DA8XX_GPIO7]		= 7,
	[IRQ_DA8XX_GPIO8]		= 7,
	[IRQ_DA8XX_I2CINT1]		= 7,
	[IRQ_DA8XX_LCDINT]		= 7,
	[IRQ_DA8XX_UARTINT1]		= 7,
	[IRQ_DA8XX_MCASPINT]		= 7,
	[IRQ_DA8XX_ALLINT1]		= 7,
	[IRQ_DA8XX_SPINT1]		= 7,
	[IRQ_DA8XX_UHPI_INT1]		= 7,
	[IRQ_DA8XX_USB_INT]		= 7,
	[IRQ_DA8XX_IRQN]		= 7,
	[IRQ_DA8XX_RWAKEUP]		= 7,
	[IRQ_DA8XX_UARTINT2]		= 7,
	[IRQ_DA8XX_DFTSSINT]		= 7,
	[IRQ_DA8XX_EHRPWM0]		= 7,
	[IRQ_DA8XX_EHRPWM0TZ]		= 7,
	[IRQ_DA8XX_EHRPWM1]		= 7,
	[IRQ_DA8XX_EHRPWM1TZ]		= 7,
	[IRQ_DA850_SATAINT]		= 7,
	[IRQ_DA850_TINT12_2]		= 7,
	[IRQ_DA850_TINT34_2]		= 7,
	[IRQ_DA850_TINTALL_2]		= 7,
	[IRQ_DA8XX_ECAP0]		= 7,
	[IRQ_DA8XX_ECAP1]		= 7,
	[IRQ_DA8XX_ECAP2]		= 7,
	[IRQ_DA850_MMCSDINT0_1]		= 7,
	[IRQ_DA850_MMCSDINT1_1]		= 7,
	[IRQ_DA850_T12CMPINT0_2]	= 7,
	[IRQ_DA850_T12CMPINT1_2]	= 7,
	[IRQ_DA850_T12CMPINT2_2]	= 7,
	[IRQ_DA850_T12CMPINT3_2]	= 7,
	[IRQ_DA850_T12CMPINT4_2]	= 7,
	[IRQ_DA850_T12CMPINT5_2]	= 7,
	[IRQ_DA850_T12CMPINT6_2]	= 7,
	[IRQ_DA850_T12CMPINT7_2]	= 7,
	[IRQ_DA850_T12CMPINT0_3]	= 7,
	[IRQ_DA850_T12CMPINT1_3]	= 7,
	[IRQ_DA850_T12CMPINT2_3]	= 7,
	[IRQ_DA850_T12CMPINT3_3]	= 7,
	[IRQ_DA850_T12CMPINT4_3]	= 7,
	[IRQ_DA850_T12CMPINT5_3]	= 7,
	[IRQ_DA850_T12CMPINT6_3]	= 7,
	[IRQ_DA850_T12CMPINT7_3]	= 7,
	[IRQ_DA850_RPIINT]		= 7,
	[IRQ_DA850_VPIFINT]		= 7,
	[IRQ_DA850_CCINT1]		= 7,
	[IRQ_DA850_CCERRINT1]		= 7,
	[IRQ_DA850_TCERRINT2]		= 7,
	[IRQ_DA850_TINT12_3]		= 7,
	[IRQ_DA850_TINT34_3]		= 7,
	[IRQ_DA850_TINTALL_3]		= 7,
	[IRQ_DA850_MCBSP0RINT]		= 7,
	[IRQ_DA850_MCBSP0XINT]		= 7,
	[IRQ_DA850_MCBSP1RINT]		= 7,
	[IRQ_DA850_MCBSP1XINT]		= 7,
	[IRQ_DA8XX_ARMCLKSTOPREQ]	= 7,
};

static struct map_desc da850_io_desc[] = {
	{
		.virtual	= IO_VIRT,
		.pfn		= __phys_to_pfn(IO_PHYS),
		.length		= IO_SIZE,
		.type		= MT_DEVICE
	},
	{
		.virtual	= DA8XX_CP_INTC_VIRT,
		.pfn		= __phys_to_pfn(DA8XX_CP_INTC_BASE),
		.length		= DA8XX_CP_INTC_SIZE,
		.type		= MT_DEVICE
	},
	{
		.virtual	= SRAM_VIRT,
		.pfn		= __phys_to_pfn(DA8XX_SHARED_RAM_BASE),
		.length		= SZ_8K,
		.type		= MT_DEVICE
	},
};

static u32 da850_psc_bases[] = { DA8XX_PSC0_BASE, DA8XX_PSC1_BASE };

/* Contents of JTAG ID register used to identify exact cpu type */
static struct davinci_id da850_ids[] = {
	{
		.variant	= 0x1,
		.part_no	= 0xb7d1,
		.manufacturer	= 0x017,	/* 0x02f >> 1 */
		.cpu_id		= DAVINCI_CPU_ID_DA850,
		.name		= "da850/omap-l138",
	},
	{
		.variant	= 0x1,
		.part_no	= 0xb7d1,
		.manufacturer	= 0x017,	/* 0x02f >> 1 */
		.cpu_id		= DAVINCI_CPU_ID_DA850,
		.name		= "da850/omap-l138/am18xx",
	},
};

static struct davinci_timer_instance da850_timer_instance[4] = {
	{
		.base		= DA8XX_TIMER64P0_BASE,
		.bottom_irq	= IRQ_DA8XX_TINT12_0,
		.top_irq	= IRQ_DA8XX_TINT34_0,
	},
	{
		.base		= DA8XX_TIMER64P1_BASE,
		.bottom_irq	= IRQ_DA8XX_TINT12_1,
		.top_irq	= IRQ_DA8XX_TINT34_1,
	},
	{
		.base		= DA850_TIMER64P2_BASE,
		.bottom_irq	= IRQ_DA850_TINT12_2,
		.top_irq	= IRQ_DA850_TINT34_2,
	},
	{
		.base		= DA850_TIMER64P3_BASE,
		.bottom_irq	= IRQ_DA850_TINT12_3,
		.top_irq	= IRQ_DA850_TINT34_3,
	},
};

/*
 * T0_BOT: Timer 0, bottom		: Used for clock_event
 * T0_TOP: Timer 0, top			: Used for clocksource
 * T1_BOT, T1_TOP: Timer 1, bottom & top: Used for watchdog timer
 */
static struct davinci_timer_info da850_timer_info = {
	.timers		= da850_timer_instance,
	.clockevent_id	= T0_BOT,
	.clocksource_id	= T0_TOP,
};

static void da850_set_async3_src(int pllnum)
{
	struct clk *clk, *newparent = pllnum ? &pll1_sysclk2 : &pll0_sysclk2;
	struct clk_lookup *c;
	unsigned int v;
	int ret;

	for (c = da850_clks; c->clk; c++) {
		clk = c->clk;
		if (clk->flags & DA850_CLK_ASYNC3) {
			ret = clk_set_parent(clk, newparent);
			WARN(ret, "DA850: unable to re-parent clock %s",
								clk->name);
		}
       }

	v = __raw_readl(DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP3_REG));
	if (pllnum)
		v |= CFGCHIP3_ASYNC3_CLKSRC;
	else
		v &= ~CFGCHIP3_ASYNC3_CLKSRC;
	__raw_writel(v, DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP3_REG));
}

static int da850_set_pll0sysclk3_rate(struct clk *clk, unsigned long rate)
{
	struct clk *arm_clk;
	unsigned long sys_clk3_rate = 148000000;
	int ret;

	arm_clk = clk_get(NULL, "arm");
	if (WARN(IS_ERR(arm_clk), "Unable to get ARM clock\n"))
		return PTR_ERR(arm_clk);

	/* Set EMIF clock based on OPPs */
	switch (clk_get_rate(arm_clk)) {
	case 200000000:
		sys_clk3_rate = 75000000;
		break;
	case 96000000:
		sys_clk3_rate = 50000000;
		break;
	}

	if (rate)
		sys_clk3_rate = min(sys_clk3_rate, rate);

	ret = davinci_set_sysclk_rate(clk, sys_clk3_rate);
	if (WARN_ON(ret))
		return ret;

	return 0;
}

#ifdef CONFIG_CPU_FREQ
/*
 * Notes:
 * According to the TRM, minimum PLLM results in maximum power savings.
 * The OPP definitions below should keep the PLLM as low as possible.
 *
 * The output of the PLLM must be between 300 to 600 MHz.
 */
struct da850_opp {
	unsigned int	freq;	/* in KHz */
	unsigned int	prediv;
	unsigned int	mult;
	unsigned int	postdiv;
	unsigned int	cvdd_min; /* in uV */
	unsigned int	cvdd_max; /* in uV */
};

static const struct da850_opp da850_opp_456 = {
	.freq		= 456000,
	.prediv		= 1,
	.mult		= 19,
	.postdiv	= 1,
	.cvdd_min	= 1300000,
	.cvdd_max	= 1350000,
};

static const struct da850_opp da850_opp_408 = {
	.freq		= 408000,
	.prediv		= 1,
	.mult		= 17,
	.postdiv	= 1,
	.cvdd_min	= 1300000,
	.cvdd_max	= 1350000,
};

static const struct da850_opp da850_opp_372 = {
	.freq		= 372000,
	.prediv		= 2,
	.mult		= 31,
	.postdiv	= 1,
	.cvdd_min	= 1200000,
	.cvdd_max	= 1320000,
};

static const struct da850_opp da850_opp_300 = {
	.freq		= 300000,
	.prediv		= 1,
	.mult		= 25,
	.postdiv	= 2,
	.cvdd_min	= 1200000,
	.cvdd_max	= 1320000,
};

static const struct da850_opp da850_opp_200 = {
	.freq		= 200000,
	.prediv		= 1,
	.mult		= 25,
	.postdiv	= 3,
	.cvdd_min	= 1100000,
	.cvdd_max	= 1160000,
};

static const struct da850_opp da850_opp_96 = {
	.freq		= 96000,
	.prediv		= 1,
	.mult		= 20,
	.postdiv	= 5,
	.cvdd_min	= 1000000,
	.cvdd_max	= 1050000,
};

#define OPP(freq) 		\
	{				\
		.index = (unsigned int) &da850_opp_##freq,	\
		.frequency = freq * 1000, \
	}

static struct cpufreq_frequency_table da850_freq_table[] = {
	OPP(456),
	OPP(408),
	OPP(372),
	OPP(300),
	OPP(200),
	OPP(96),
	{
		.index		= 0,
		.frequency	= CPUFREQ_TABLE_END,
	},
};

#ifdef CONFIG_REGULATOR
static int da850_set_voltage(unsigned int index);
static int da850_regulator_init(void);
#endif

static struct davinci_cpufreq_config cpufreq_info = {
	.freq_table = da850_freq_table,
#ifdef CONFIG_REGULATOR
	.init = da850_regulator_init,
	.set_voltage = da850_set_voltage,
#endif
	.emif_rate = CONFIG_DA850_FIX_PLL0_SYSCLK3RATE,
};

#ifdef CONFIG_REGULATOR
static struct regulator *cvdd;

static int da850_set_voltage(unsigned int index)
{
	struct da850_opp *opp;

	if (!cvdd)
		return -ENODEV;

	opp = (struct da850_opp *) cpufreq_info.freq_table[index].index;

	return regulator_set_voltage(cvdd, opp->cvdd_min, opp->cvdd_max);
}

static int da850_regulator_init(void)
{
	cvdd = regulator_get(NULL, "cvdd");
	if (WARN(IS_ERR(cvdd), "Unable to obtain voltage regulator for CVDD;"
					" voltage scaling unsupported\n")) {
		return PTR_ERR(cvdd);
	}

	return 0;
}
#endif

static void da850_set_pll0_bypass_src(bool pll1_sysclk3)
{
	struct clk *clk = &pll0_clk;
	struct pll_data *pll;
	unsigned int v;

	pll = clk->pll_data;
	v = __raw_readl(pll->base + PLLCTL);
	if (pll1_sysclk3)
		v |= PLLC0_PLL1_SYSCLK3_EXTCLKSRC;
	else
		v &= ~PLLC0_PLL1_SYSCLK3_EXTCLKSRC;
	__raw_writel(v, pll->base + PLLCTL);
}

static struct platform_device da850_cpufreq_device = {
	.name			= "cpufreq-davinci",
	.dev = {
		.platform_data	= &cpufreq_info,
	},
	.id = -1,
};

unsigned int da850_max_speed = 300000;

int __init da850_register_cpufreq(char *async_clk)
{
	int i;

	/* cpufreq driver can help keep an "async" clock constant */
	if (async_clk)
		clk_add_alias("async", da850_cpufreq_device.name,
							async_clk, NULL);

	/* Use PLL1_SYSCLK3 for the PLL0 bypass clock */
	da850_set_pll0_bypass_src(true);

	for (i = 0; i < ARRAY_SIZE(da850_freq_table); i++) {
		if (da850_freq_table[i].frequency <= da850_max_speed) {
			cpufreq_info.freq_table = &da850_freq_table[i];
			break;
		}
	}

	return platform_device_register(&da850_cpufreq_device);
}

static int da850_round_armrate(struct clk *clk, unsigned long rate)
{
	int i, ret = 0, diff;
	unsigned int best = (unsigned int) -1;
	struct cpufreq_frequency_table *table = cpufreq_info.freq_table;

	rate /= 1000; /* convert to kHz */

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		diff = table[i].frequency - rate;
		if (diff < 0)
			diff = -diff;

		if (diff < best) {
			best = diff;
			ret = table[i].frequency;
		}
	}

	return ret * 1000;
}

static int da850_set_armrate(struct clk *clk, unsigned long index)
{
	struct clk *pllclk = &pll0_clk;

	return clk_set_rate(pllclk, index);
}

static int da850_set_pll0rate(struct clk *clk, unsigned long index)
{
	unsigned int prediv, mult, postdiv;
	struct da850_opp *opp;
	struct pll_data *pll = clk->pll_data;
	int ret;

	opp = (struct da850_opp *) cpufreq_info.freq_table[index].index;
	prediv = opp->prediv;
	mult = opp->mult;
	postdiv = opp->postdiv;

	ret = davinci_set_pllrate(pll, prediv, mult, postdiv);
	if (WARN_ON(ret))
		return ret;

	return 0;
}
#else
int __init da850_register_cpufreq(char *async_clk)
{
	return 0;
}

static int da850_set_armrate(struct clk *clk, unsigned long rate)
{
	return -EINVAL;
}

static int da850_set_pll0rate(struct clk *clk, unsigned long armrate)
{
	return -EINVAL;
}

static int da850_round_armrate(struct clk *clk, unsigned long rate)
{
	return clk->rate;
}
#endif

#define DA8XX_EHRPWM0_BASE	0x01F00000

static struct resource da850_ehrpwm0_resource[] = {
	{
		.start	= DA8XX_EHRPWM0_BASE,
		.end	= DA8XX_EHRPWM0_BASE + 0x1fff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_DA8XX_EHRPWM0TZ,
		.end	= IRQ_DA8XX_EHRPWM0TZ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_DA8XX_EHRPWM0,
		.end	= IRQ_DA8XX_EHRPWM0,
		.flags	= IORESOURCE_IRQ,
	 },
};

static struct ehrpwm_platform_data da850_ehrpwm0_data;

static struct platform_device da850_ehrpwm0_dev = {
	.name		= "ehrpwm",
	.id		= 0,
	.dev		= {
		.platform_data	= &da850_ehrpwm0_data,
	},
	.resource	= da850_ehrpwm0_resource,
	.num_resources	= ARRAY_SIZE(da850_ehrpwm0_resource),
};

#define DA8XX_EHRPWM1_BASE	0x01F02000

static struct resource da850_ehrpwm1_resource[] = {
	{
		.start	= DA8XX_EHRPWM1_BASE,
		.end	= DA8XX_EHRPWM1_BASE + 0x1fff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_DA8XX_EHRPWM1TZ,
		.end	= IRQ_DA8XX_EHRPWM1TZ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= IRQ_DA8XX_EHRPWM1,
		.end	= IRQ_DA8XX_EHRPWM1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ehrpwm_platform_data da850_ehrpwm1_data;

static struct platform_device da850_ehrpwm1_dev = {
	.name		= "ehrpwm",
	.id		= 1,
	.dev		= {
		.platform_data	= &da850_ehrpwm1_data,
	},
	.resource	= da850_ehrpwm1_resource,
	.num_resources	= ARRAY_SIZE(da850_ehrpwm1_resource),
};

#define DA8XX_CHIPCFG1		DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP1_REG)

void __init da850_register_ehrpwm(char mask)
{
	int ret = 0;

	__raw_writew(__raw_readw(DA8XX_CHIPCFG1) | BIT(12), DA8XX_CHIPCFG1);
	if (mask & 0x3) {
		da850_ehrpwm0_data.channel_mask = mask & 0x3;
		ret = platform_device_register(&da850_ehrpwm0_dev);
		if (ret)
			pr_warning("da850_evm_init: eHRPWM module0 registration failed\n");
	}

	if ((mask >> 0x2) & 0x3) {
		da850_ehrpwm1_data.channel_mask = mask >> 0x2;
		ret = platform_device_register(&da850_ehrpwm1_dev);
		if (ret)
			pr_warning("da850_evm_init: eHRPWM module1 registration failed\n");
	}
}

#define DA8XX_ECAP0_BASE        0x01F06000

static struct resource da850_ecap0_resource[] = {
	{
	.start		= DA8XX_ECAP0_BASE,
	.end		= DA8XX_ECAP0_BASE + 0xfff,
	.flags		= IORESOURCE_MEM,
	},
	{
	.start          = IRQ_DA8XX_ECAP0,
	.end            = IRQ_DA8XX_ECAP0,
	.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device da850_ecap0_dev = {
	.name		= "ecap",
	.id		= 0,
	.resource       = da850_ecap0_resource,
	.num_resources  = ARRAY_SIZE(da850_ecap0_resource),
};

static struct platform_device da850_ecap0_cap_dev = {
	.name		= "ecap_cap",
	.id		= 0,
	.resource       = da850_ecap0_resource,
	.num_resources  = ARRAY_SIZE(da850_ecap0_resource),
};

#define DA8XX_ECAP1_BASE        0x01F07000

static struct resource da850_ecap1_resource[] = {
	{
	.start		= DA8XX_ECAP1_BASE,
	.end		= DA8XX_ECAP1_BASE + 0xfff,
	.flags		= IORESOURCE_MEM,
	},
	{
	.start		= IRQ_DA8XX_ECAP1,
	.end		= IRQ_DA8XX_ECAP1,
	.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device da850_ecap1_dev = {
	.name		= "ecap",
	.id		= 1,
	.resource	= da850_ecap1_resource,
	.num_resources	= ARRAY_SIZE(da850_ecap1_resource),
};

static struct platform_device da850_ecap1_cap_dev = {
	.name		= "ecap_cap",
	.id		= 1,
	.resource	= da850_ecap1_resource,
	.num_resources	= ARRAY_SIZE(da850_ecap1_resource),
};

#define DA8XX_ECAP2_BASE        0x01F08000

static struct resource da850_ecap2_resource[] = {
	{
	.start		= DA8XX_ECAP2_BASE,
	.end		= DA8XX_ECAP2_BASE + 0xfff,
	.flags		= IORESOURCE_MEM,
	},
	{
	.start		= IRQ_DA8XX_ECAP2,
	.end		= IRQ_DA8XX_ECAP2,
	.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device da850_ecap2_dev = {
	.name		= "ecap",
	.id		= 2,
	.resource	= da850_ecap2_resource,
	.num_resources	= ARRAY_SIZE(da850_ecap2_resource),
};

static struct platform_device da850_ecap2_cap_dev = {
	.name		= "ecap_cap",
	.id		= 2,
	.resource	= da850_ecap2_resource,
	.num_resources	= ARRAY_SIZE(da850_ecap2_resource),
};

int __init da850_register_ecap(char instance)
{
	if (instance == 0)
		return platform_device_register(&da850_ecap0_dev);
	else if (instance == 1)
		return platform_device_register(&da850_ecap1_dev);
	else if (instance == 2)
		return platform_device_register(&da850_ecap2_dev);
	else
		return -EINVAL;
}

int __init da850_register_ecap_cap(char instance)
{
	if (instance == 0)
		return platform_device_register(&da850_ecap0_cap_dev);
	else if (instance == 1)
		return platform_device_register(&da850_ecap1_cap_dev);
	else if (instance == 2)
		return platform_device_register(&da850_ecap2_cap_dev);
	else
		return -EINVAL;
}

int __init da850_register_pm(struct platform_device *pdev)
{
	int ret;
	struct davinci_pm_config *pdata = pdev->dev.platform_data;

	ret = davinci_cfg_reg(DA850_RTC_ALARM);
	if (ret)
		return ret;

	pdata->ddr2_ctlr_base = da8xx_get_mem_ctlr();
	pdata->deepsleep_reg = DA8XX_SYSCFG1_VIRT(DA8XX_DEEPSLEEP_REG);
	pdata->ddrpsc_num = DA8XX_LPSC1_EMIF3C;

	pdata->cpupll_reg_base = ioremap(DA8XX_PLL0_BASE, SZ_4K);
	if (!pdata->cpupll_reg_base)
		return -ENOMEM;

	pdata->ddrpll_reg_base = ioremap(DA850_PLL1_BASE, SZ_4K);
	if (!pdata->ddrpll_reg_base) {
		ret = -ENOMEM;
		goto no_ddrpll_mem;
	}

	pdata->ddrpsc_reg_base = ioremap(DA8XX_PSC1_BASE, SZ_4K);
	if (!pdata->ddrpsc_reg_base) {
		ret = -ENOMEM;
		goto no_ddrpsc_mem;
	}

	return platform_device_register(pdev);

no_ddrpsc_mem:
	iounmap(pdata->ddrpll_reg_base);
no_ddrpll_mem:
	iounmap(pdata->cpupll_reg_base);
	return ret;
}

int __init da850_register_backlight(struct platform_device *pdev,
			struct platform_pwm_backlight_data *backlight_data)
{

	backlight_data->pwm_id	= "ehrpwm.1";
	backlight_data->ch	= 1;

	pdev->dev.platform_data = backlight_data;
	return platform_device_register(pdev);
}

/* VPIF resource, platform data */
static u64 da850_vpif_dma_mask = DMA_BIT_MASK(32);

static struct resource da850_vpif_resource[] = {
	{
		.start	= DA8XX_VPIF_BASE,
		.end	= DA8XX_VPIF_BASE + 0xfff,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device da850_vpif_dev = {
	.name		= "vpif",
	.id		= -1,
	.dev		= {
			.dma_mask		= &da850_vpif_dma_mask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= da850_vpif_resource,
	.num_resources	= ARRAY_SIZE(da850_vpif_resource),
};

/* VPIF display resource, platform data */
static struct resource da850_vpif_display_resource[] = {
	{
		.start = IRQ_DA850_VPIFINT,
		.end   = IRQ_DA850_VPIFINT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device da850_vpif_display_dev = {
	.name		= "vpif_display",
	.id		= -1,
	.dev		= {
		.dma_mask		= &da850_vpif_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= da850_vpif_display_resource,
	.num_resources	= ARRAY_SIZE(da850_vpif_display_resource),
};

/* VPIF capture resource, platform data */
static struct resource da850_vpif_capture_resource[] = {
	{
		.start = IRQ_DA850_VPIFINT,
		.end   = IRQ_DA850_VPIFINT,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_DA850_VPIFINT,
		.end   = IRQ_DA850_VPIFINT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device da850_vpif_capture_dev = {
	.name		= "vpif_capture",
	.id		= -1,
	.dev		= {
		.dma_mask		= &da850_vpif_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= da850_vpif_capture_resource,
	.num_resources	= ARRAY_SIZE(da850_vpif_capture_resource),
};

int __init da850_register_vpif(void)
{
	return platform_device_register(&da850_vpif_dev);
}

int __init da850_register_vpif_display(struct vpif_display_config
						*display_config)
{
	da850_vpif_display_dev.dev.platform_data = display_config;
	return platform_device_register(&da850_vpif_display_dev);
}

int __init da850_register_vpif_capture(struct vpif_capture_config
							*capture_config)
{
	da850_vpif_capture_dev.dev.platform_data = capture_config;
	return platform_device_register(&da850_vpif_capture_dev);
}

static struct davinci_soc_info davinci_soc_info_da850 = {
	.io_desc		= da850_io_desc,
	.io_desc_num		= ARRAY_SIZE(da850_io_desc),
	.jtag_id_reg		= DA8XX_SYSCFG0_BASE + DA8XX_JTAG_ID_REG,
	.ids			= da850_ids,
	.ids_num		= ARRAY_SIZE(da850_ids),
	.cpu_clks		= da850_clks,
	.psc_bases		= da850_psc_bases,
	.psc_bases_num		= ARRAY_SIZE(da850_psc_bases),
	.pinmux_base		= DA8XX_SYSCFG0_BASE + 0x120,
	.pinmux_pins		= da850_pins,
	.pinmux_pins_num	= ARRAY_SIZE(da850_pins),
	.intc_base		= DA8XX_CP_INTC_BASE,
	.intc_type		= DAVINCI_INTC_TYPE_CP_INTC,
	.intc_irq_prios		= da850_default_priorities,
	.intc_irq_num		= DA850_N_CP_INTC_IRQ,
	.timer_info		= &da850_timer_info,
	.gpio_type		= GPIO_TYPE_DAVINCI,
	.gpio_base		= DA8XX_GPIO_BASE,
	.gpio_num		= 144,
	.gpio_irq		= IRQ_DA8XX_GPIO0,
	.serial_dev		= &da8xx_serial_device,
	.emac_pdata		= &da8xx_emac_pdata,
	.sram_phys		= DA8XX_ARM_RAM_BASE,
	.sram_len		= SZ_8K,
};

void __init da850_init(void)
{
	unsigned int v;

	davinci_common_init(&davinci_soc_info_da850);

	da8xx_syscfg0_base = ioremap(DA8XX_SYSCFG0_BASE, SZ_4K);
	if (WARN(!da8xx_syscfg0_base, "Unable to map syscfg0 module"))
		return;

	da8xx_syscfg1_base = ioremap(DA8XX_SYSCFG1_BASE, SZ_4K);
	if (WARN(!da8xx_syscfg1_base, "Unable to map syscfg1 module"))
		return;


// FIXME: Add support for these two items if we need it
//	da8xx_psc1_base = ioremap(DA8XX_PSC1_BASE, SZ_4K);		// LEGO BT slow clock
//	if (WARN(!da8xx_psc1_base, "Unable to map psc1 module"))	// LEGO BT slow clock
//		return;							// LEGO BT slow clock
//
//	da8xx_ecap2_base = ioremap(DA8XX_ECAP2_BASE, SZ_4K);		// LEGO BT slow clock
//	if (WARN(!da8xx_ecap2_base, "Unable to map ecap2 module"))	// LEGO BT slow clock
//		return;							// LEGO BT slow clock
//


//	davinci_soc_info_da850.jtag_id_base =
//					DA8XX_SYSCFG0_VIRT(DA8XX_JTAG_ID_REG);
//	davinci_soc_info_da850.pinmux_base = DA8XX_SYSCFG0_VIRT(0x120);


	/*
	 * Move the clock source of Async3 domain to PLL1 SYSCLK2.
	 * This helps keeping the peripherals on this domain insulated
	 * from CPU frequency changes caused by DVFS. The firmware sets
	 * both PLL0 and PLL1 to the same frequency so, there should not
	 * be any noticeable change even in non-DVFS use cases.
	 */
	da850_set_async3_src(1);

	/* Unlock writing to PLL0 registers */
	v = __raw_readl(DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP0_REG));
	v &= ~CFGCHIP0_PLL_MASTER_LOCK;
	__raw_writel(v, DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP0_REG));

	/* Unlock writing to PLL1 registers */
	v = __raw_readl(DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP3_REG));
	v &= ~CFGCHIP3_PLL1_MASTER_LOCK;
	__raw_writel(v, DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP3_REG));
}
