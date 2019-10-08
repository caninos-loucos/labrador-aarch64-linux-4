

#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clk.h"

/**
 * struct owl_pll
 * @hw:      Handle between common and hardware-specific interfaces
 * @reg:     PLL control register
 * @lock:    Register lock
 * @bfreq:   the base frequence of the PLL. PLL frequence = bfreq * mul
 * @enable_bit: the enable bit for PLL
 * @shift:   shift to the muliplier bit field
 * @width:   width of the muliplier bit field
 * @min_mul: the minimum muliple for the PLL
 * @max_mul: the maximum muliple for the PLL
 */
struct owl_pll {
	struct clk_hw		hw;
	void __iomem		*reg;
	spinlock_t		*lock;
	unsigned long		bfreq;
	u8			enable_bit;
	u8			shift;
	u8			width;
	u8			min_mul;
	u8			max_mul;
	u8			pll_flags;
	const struct clk_pll_table *table;
};

#define to_owl_pll(_hw)		container_of(_hw, struct owl_pll, hw)
#define mul_mask(m)		((1 << ((m)->width)) - 1)
#define PLL_STABILITY_WAIT_US	(50)

/**
 * owl_pll_calculate_mul() - cacluate muliple for specific rate
 * @pll:	owl pll
 * @rate:	Desired clock frequency
 * Returns appropriate muliple closest to @rate the hardware can generate.
 */
static u32 owl_pll_calculate_mul(struct owl_pll *pll, unsigned long rate)
{
	u32 mul;

	mul = DIV_ROUND_CLOSEST(rate, pll->bfreq);
	if (mul < pll->min_mul)
		mul = pll->min_mul;
	else if (mul > pll->max_mul)
		mul = pll->max_mul;

	mul &= mul_mask(pll);

	return mul;
}

static unsigned int _get_table_rate(const struct clk_pll_table *table,
		unsigned int val)
{
	const struct clk_pll_table *clkt;

	for (clkt = table; clkt->rate; clkt++) {
		if (clkt->val == val)
			return clkt->rate;
	}

	return 0;
}

static unsigned int _get_table_val(const struct clk_pll_table *table,
		unsigned long rate)
{
	const struct clk_pll_table *clkt;
	unsigned int val = 0;

	for (clkt = table; clkt->rate; clkt++) {
		if (clkt->rate == rate) {
			val = clkt->val;
			break;
		} else if (clkt->rate < rate)
			val = clkt->val;
	}

	return val;
}


static unsigned long _get_table_round_rate(const struct clk_pll_table *table,
		unsigned long rate)
{
	const struct clk_pll_table *clkt;
	unsigned long round_rate = 0;

	for (clkt = table; clkt->rate; clkt++) {
		if (clkt->rate == rate) {
			round_rate = clkt->rate;
			break;
		} else if (clkt->rate < rate)
			round_rate = clkt->rate;
	}

	return round_rate;
}


/**
 * owl_pll_round_rate() - Round a clock frequency
 * @hw:		Handle between common and hardware-specific interfaces
 * @rate:	Desired clock frequency
 * @prate:	Clock frequency of parent clock
 * Returns frequency closest to @rate the hardware can generate.
 */
static long owl_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long round_rate;
	u32 mul;

	if (pll->table) {
		round_rate = _get_table_round_rate(pll->table, rate);
		return round_rate;
	}

	/* fixed frequence */
	if (pll->width == 0)
		return pll->bfreq;

	mul = owl_pll_calculate_mul(pll, rate);

	return pll->bfreq * mul;
}

/**
 * owl_pll_recalc_rate() - Recalculate clock frequency
 * @hw:			Handle between common and hardware-specific interfaces
 * @parent_rate:	Clock frequency of parent clock
 * Returns current clock frequency.
 */
static unsigned long owl_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long rate;
	u32 val, mul;

	if (pll->table) {
		val = readl(pll->reg) >> pll->shift;
		val &= mul_mask(pll);

		rate = _get_table_rate(pll->table, val);

		return rate;
	}

	/* fixed frequence */
	if (pll->width == 0)
		return pll->bfreq;

	mul = (readl(pll->reg) >> pll->shift) & mul_mask(pll);

	return pll->bfreq * mul;
}

/**
 * owl_pll_is_enabled - Check if a clock is enabled
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 1 if the clock is enabled, 0 otherwise.
 *
 * Not sure this is a good idea, but since disabled means bypassed for
 * this clock implementation we say we are always enabled.
 */
static int owl_pll_is_enabled(struct clk_hw *hw)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long flags = 0;
	u32 v;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	v = readl(pll->reg);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	return !!(v & BIT(pll->enable_bit));
}

/**
 * owl_pll_enable - Enable clock
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 0 on success
 */
static int owl_pll_enable(struct clk_hw *hw)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long flags = 0;
	u32 v;

	if (owl_pll_is_enabled(hw))
		return 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	v = readl(pll->reg);
	v |= BIT(pll->enable_bit);
	writel(v, pll->reg);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	udelay(PLL_STABILITY_WAIT_US);

	return 0;
}

/**
 * owl_pll_disable - Disable clock
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 0 on success
 */
static void owl_pll_disable(struct clk_hw *hw)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long flags = 0;
	u32 v;

	if (owl_pll_is_enabled(hw))
		return;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	v = readl(pll->reg);
	v &= ~BIT(pll->enable_bit);
	writel(v, pll->reg);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
}

static int owl_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct owl_pll *pll = to_owl_pll(hw);
	unsigned long flags = 0;
	u32 val, v;

	pr_debug("%s: rate %ld, parent_rate %ld, before set rate reg 0x%x\n",
		__func__, rate, parent_rate, readl(pll->reg));

	/* fixed frequence */
	if (pll->width == 0)
		return 0;

	if (pll->table)
		val = _get_table_val(pll->table, rate);
	else
		val = owl_pll_calculate_mul(pll, rate);

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	v = readl(pll->reg);
	v &= ~mul_mask(pll);
	v |= val << pll->shift;
	writel(v, pll->reg);

	udelay(PLL_STABILITY_WAIT_US);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	pr_debug("%s: after set rate reg 0x%x\n", __func__,
		readl(pll->reg));

	return 0;
}

static const struct clk_ops owl_pll_ops = {
	.enable = owl_pll_enable,
	.disable = owl_pll_disable,
	.is_enabled = owl_pll_is_enabled,
	.round_rate = owl_pll_round_rate,
	.recalc_rate = owl_pll_recalc_rate,
	.set_rate = owl_pll_set_rate,
};

#ifndef CONFIG_ARCH_OWL_ATS3605_SOC
/**
 * owl_corepll_recalc_rate() - Recalculate clock frequency of corepll
 * @hw:			Handle between common and hardware-specific interfaces
 * @parent_rate:	Clock frequency of parent clock
 * Returns current clock frequency.
 */
static unsigned long owl_corepll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	
	
	unsigned long rate;
	
	

	//rate = _owl_corepll_recalc_rate();
	
	
	

	return rate;
}

/**
 * owl_corepll_set_rate() - Set clock frequency of corepll
 * @hw:			Handle between common and hardware-specific interfaces
 * @rate:		Desired clock frequency of corepll
 * @parent_rate:	Clock frequency of parent clock
 * Returns 0 on success.
 */
static int owl_corepll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct owl_pll *pll = to_owl_pll(hw);

	pr_debug("%s: rate %ld, parent_rate %ld, before set rate reg 0x%x\n",
		__func__, rate, parent_rate, readl(pll->reg));

	//_owl_corepll_set_rate(rate);
	
	return -1;
	
	
	
	

	udelay(PLL_STABILITY_WAIT_US);

	pr_debug("%s: after set rate reg 0x%x\n", __func__,
		readl(pll->reg));

	return 0;
}

static const struct clk_ops owl_corepll_ops = {
	.enable = owl_pll_enable,
	.disable = owl_pll_disable,
	.is_enabled = owl_pll_is_enabled,
	.round_rate = owl_pll_round_rate,
	.recalc_rate = owl_pll_recalc_rate,
	.set_rate = owl_corepll_set_rate,
};
#endif
/**
 * owl_clk_register_pll() - Register PLL with the clock framework
 * @name	PLL name
 * @parent	Parent clock name
 * @reg	Pointer to PLL control register
 * @pll_status	Pointer to PLL status register
 * @lock_index	Bit index to this PLL's lock status bit in @pll_status
 * @lock	Register lock
 * Returns handle to the registered clock.
 */
struct clk *owl_pll_clk_register(const char *name, const char *parent_name,
		unsigned long flags, void __iomem *reg, unsigned long bfreq,
		u8 enable_bit, u8 shift, u8 width, u8 min_mul, u8 max_mul, u8 pll_flags,
		const struct clk_pll_table *table, spinlock_t *lock)
{
	struct owl_pll *pll;
	struct clk *clk;
	struct clk_init_data initd;

	pll = kmalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: Could not allocate OWL PLL clk.\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	/* Populate the struct */
	initd.name = name;
	initd.parent_names = (parent_name ? &parent_name : NULL);
	initd.num_parents = (parent_name ? 1 : 0);
#ifndef CONFIG_ARCH_OWL_ATS3605_SOC
	if (strcmp(name, "core_pll") == 0)
		initd.ops = &owl_corepll_ops;
	else
#endif 
		initd.ops = &owl_pll_ops;
	initd.flags = flags;

	pll->hw.init = &initd;
	pll->bfreq = bfreq;
	pll->enable_bit = enable_bit;
	pll->shift = shift;
	pll->width = width;
	pll->min_mul = min_mul;
	pll->max_mul = max_mul;
	pll->pll_flags = pll_flags;
	pll->table = table;
	pll->reg = reg;
	pll->lock = lock;

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk)))
		goto free_pll;

	return clk;

free_pll:
	kfree(pll);

	return clk;
}
