#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include "clk.h"

/* add a clock instance to the clock lookup table used for dt based lookup */
void owl_clk_add_lookup(struct owl_clk_provider *ctx, struct clk *clk,
				unsigned int id)
{
	if (ctx->clk_data.clks && id)
		ctx->clk_data.clks[id] = clk;
}



/* register a list of fixed clocks */
void __init owl_clk_register_fixed_rate(struct owl_clk_provider *ctx,
		struct owl_fixed_rate_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = clk_register_fixed_rate(NULL, clks[i].name,
				clks[i].parent_name,
				clks[i].flags,
				clks[i].fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Samsung platforms.
		 */
		ret = clk_register_clkdev(clk, clks[i].name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, clks[i].name);
	}
}

/* register a list of fixed factor clocks */
void __init owl_clk_register_fixed_factor(struct owl_clk_provider *ctx,
		struct owl_fixed_factor_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = clk_register_fixed_factor(NULL, clks[i].name,
				clks[i].parent_name,
				clks[i].flags,
				clks[i].mult,
				clks[i].div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Samsung platforms.
		 */
		ret = clk_register_clkdev(clk, clks[i].name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, clks[i].name);
	}
}

/* register a list of pll clocks */
void __init owl_clk_register_pll(struct owl_clk_provider *ctx,
		struct owl_pll_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = owl_pll_clk_register(clks[i].name, clks[i].parent_name,
				clks[i].flags, ctx->reg_base + clks[i].offset,
				clks[i].bfreq, clks[i].enable_bit,
				clks[i].shift, clks[i].width,
				clks[i].min_mul, clks[i].max_mul,
				clks[i].pll_flags, clks[i].table,
				&ctx->lock);

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Samsung platforms.
		 */
		ret = clk_register_clkdev(clk, clks[i].name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, clks[i].name);
	}
}

void __init owl_clk_register_divider(struct owl_clk_provider *ctx,
		struct owl_divider_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = clk_register_divider_table(NULL, clks[i].name,
				clks[i].parent_name,
				clks[i].flags,
				ctx->reg_base + clks[i].offset,
				clks[i].shift, clks[i].width,
				clks[i].div_flags,
				clks[i].table,
				&ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		if (clks[i].alias) {
			ret = clk_register_clkdev(clk, clks[i].alias, NULL);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, clks[i].alias);
		}
	}
}

void __init owl_clk_register_factor(struct owl_clk_provider *ctx,
		struct owl_factor_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = owl_factor_clk_register(NULL, clks[i].name,
						 clks[i].parent_name,
						 clks[i].flags,
						 ctx->reg_base + clks[i].offset,
						 clks[i].shift, clks[i].width,
						 clks[i].div_flags,
						 clks[i].table,
						 &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		if (clks[i].alias) {
			ret = clk_register_clkdev(clk, clks[i].alias, NULL);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, clks[i].alias);
		}
	}
}

void __init owl_clk_register_mux(struct owl_clk_provider *ctx,
		struct owl_mux_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = clk_register_mux(NULL, clks[i].name, clks[i].parent_names,
				clks[i].num_parents, clks[i].flags,
				ctx->reg_base + clks[i].offset, clks[i].shift,
				clks[i].width, clks[i].mux_flags,
				&ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		if (clks[i].alias) {
			ret = clk_register_clkdev(clk, clks[i].alias, NULL);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, clks[i].alias);
		}
	}
}

void __init owl_clk_register_gate(struct owl_clk_provider *ctx,
		struct owl_gate_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = clk_register_gate(NULL, clks[i].name,
				clks[i].parent_name,
				clks[i].flags,
				ctx->reg_base + clks[i].offset,
				clks[i].bit_idx,
				clks[i].gate_flags,
				&ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
			       __func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		if (clks[i].alias) {
			ret = clk_register_clkdev(clk, clks[i].alias, NULL);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, clks[i].alias);
		}
	}
}

static struct clk * __init _register_composite(struct owl_clk_provider *ctx,
			struct owl_composite_clock *cclk)
{
	struct clk *clk;
	struct owl_mux_clock *amux;
	struct owl_gate_clock *agate;
	union rate_clock *arate;
	struct clk_gate *gate = NULL;
	struct clk_mux *mux = NULL;
	struct clk_fixed_rate *fixed = NULL;
	struct clk_fixed_factor *fixed_factor = NULL;
	struct clk_divider *div = NULL;
	struct owl_factor *factor = NULL;
	struct clk_hw *mux_hw = NULL;
	struct clk_hw *gate_hw = NULL;
	struct clk_hw *rate_hw = NULL;
	const struct clk_ops *rate_ops = NULL;
	const char *clk_name = cclk->name;
	const char **parent_names;
	int i, num_parents;

	amux = &cclk->mux;
	agate = &cclk->gate;
	arate = &cclk->rate;

	parent_names = NULL;
	num_parents = 0;

	if (amux->id) {
		pr_debug("%s %d: clk %s\n", __func__, __LINE__, clk_name);

		num_parents = amux->num_parents;
		if (num_parents > 0) {
			parent_names = kzalloc((sizeof(char *) * num_parents),
					GFP_KERNEL);
			if (!parent_names)
				return ERR_PTR(-ENOMEM);

			for (i = 0; i < num_parents; i++)
				parent_names[i] = kstrdup(amux->parent_names[i],
						GFP_KERNEL);
		}

		mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
		if (!mux)
			return NULL;

		/* set up gate properties */
		mux->reg = ctx->reg_base + amux->offset;
		mux->shift = amux->shift;
		mux->mask = BIT(amux->width) - 1;
		mux->flags = amux->mux_flags;
		mux->lock = &ctx->lock;
		mux_hw = &mux->hw;
	}

	if (arate->fixed.id) {
		switch (cclk->type) {
		case OWL_COMPOSITE_TYPE_FIXED_RATE:
			fixed = kzalloc(sizeof(struct clk_fixed_rate),
					GFP_KERNEL);
			if (!fixed)
				return NULL;
			fixed->fixed_rate = arate->fixed.fixed_rate;
			rate_ops = &clk_fixed_rate_ops;
			rate_hw = &fixed->hw;
			break;

		case OWL_COMPOSITE_TYPE_FIXED_FACTOR:
			fixed_factor = kzalloc(sizeof(struct clk_fixed_factor),
					GFP_KERNEL);
			if (!fixed_factor)
				return NULL;
			fixed_factor->mult = arate->fixed_factor.mult;
			fixed_factor->div = arate->fixed_factor.div;

			rate_ops = &clk_fixed_factor_ops;
			rate_hw = &fixed_factor->hw;
			break;

		case OWL_COMPOSITE_TYPE_DIVIDER:
			div = kzalloc(sizeof(struct clk_divider), GFP_KERNEL);
			if (!div)
				return NULL;
			div->reg = ctx->reg_base + arate->div.offset;
			div->shift = arate->div.shift;
			div->width = arate->div.width;
			div->flags = arate->div.div_flags;
			div->table = arate->div.table;
			div->lock = &ctx->lock;

			rate_ops = &clk_divider_ops;
			rate_hw = &div->hw;
			break;

		case OWL_COMPOSITE_TYPE_FACTOR:
			factor = kzalloc(sizeof(struct owl_factor),
					GFP_KERNEL);
			if (!factor)
				return NULL;
			factor->reg = ctx->reg_base + arate->factor.offset;
			factor->shift = arate->factor.shift;
			factor->width = arate->factor.width;
			factor->flags = arate->factor.div_flags;
			factor->table = arate->factor.table;
			factor->lock = &ctx->lock;

			rate_ops = &owl_factor_ops;
			rate_hw = &factor->hw;
			break;

		default:
			break;
		}
	}

	if (agate->id) {
		gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
		if (!gate)
			return ERR_PTR(-ENOMEM);

		/* set up gate properties */
		gate->reg = ctx->reg_base + agate->offset;
		gate->bit_idx = agate->bit_idx;
		gate->lock = &ctx->lock;
		gate_hw = &gate->hw;
	}

	clk = clk_register_composite(NULL, clk_name,
			parent_names, num_parents,
			mux_hw, &clk_mux_ops,
			rate_hw, rate_ops,
			gate_hw, &clk_gate_ops, cclk->flags);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register clock %s\n",
			__func__, clk_name);
	}

	return clk;
}

void __init owl_clk_register_composite(struct owl_clk_provider *ctx,
		struct owl_composite_clock *clks, int nums)
{
	struct clk *clk;
	int i, ret;

	for (i = 0; i < nums; i++) {
		clk = _register_composite(ctx, &clks[i]);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, clks[i].name);
			continue;
		}

		owl_clk_add_lookup(ctx, clk, clks[i].id);

		ret = clk_register_clkdev(clk, clks[i].name, NULL);
		if (ret)
			pr_err("%s: failed to register lookup %s\n",
				__func__, clks[i].name);
		}
}

/* setup the essentials required to support clock lookup using ccf */
struct owl_clk_provider * __init owl_clk_init(struct device_node *np,
		void __iomem *base, unsigned long nr_clks)
{
	struct owl_clk_provider *ctx;
	struct clk **clk_table;
	int ret;
	int i;

	ctx = kzalloc(sizeof(struct owl_clk_provider), GFP_KERNEL);
	
	if (!ctx) {
		panic("could not allocate clock provider context.\n");
	}

	clk_table = kcalloc(nr_clks, sizeof(struct clk *), GFP_KERNEL);
	
	if (!clk_table) {
		panic("could not allocate clock lookup table\n");
	}

	for (i = 0; i < nr_clks; ++i) {
		clk_table[i] = ERR_PTR(-ENOENT);
	}

	ctx->reg_base = base;
	ctx->clk_data.clks = clk_table;
	ctx->clk_data.clk_num = nr_clks;
	spin_lock_init(&ctx->lock);

	if (!np)
		return ctx;

	ret = of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data);
	
	if (ret)
		panic("could not register clock provide\n");

	return ctx;
}
