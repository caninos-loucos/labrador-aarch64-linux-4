#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>

#define PWM_CTL0 0x50
#define PWM_CTL1 0x54
#define PWM_CTL2 0x58
#define PWM_CTL3 0x5c
#define PWM_CTL4 0x78
#define PWM_CTL5 0x7c

struct caninos_pwm_chip
{
	struct clk *clk, *losc, *hosc;
	struct pwm_chip	chip;
	void __iomem *base;
	
	struct pinctrl *pctl;
	struct pinctrl_state *def_state;
	struct pinctrl_state *extio_state;
};

static struct caninos_pwm_chip *to_caninos_pwm_chip(struct pwm_chip *chip) {
	return container_of(chip, struct caninos_pwm_chip, chip);
}

static int caninos_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct caninos_pwm_chip *pc = to_caninos_pwm_chip(chip);
	
	if (pc->extio_state)
	{
		if (pinctrl_select_state(pc->pctl, pc->extio_state) < 0) {
			return -EAGAIN;
		}
	}
	return 0;
}

static void caninos_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct caninos_pwm_chip *pc = to_caninos_pwm_chip(chip);
	
	if (pc->extio_state) {
		pinctrl_select_state(pc->pctl, pc->def_state);
	}
}

static int caninos_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
                             struct pwm_state *state)
{
	struct caninos_pwm_chip *pc = to_caninos_pwm_chip(chip);
	unsigned long period_cycles, duty_cycles;
	const unsigned long prescale = 100;
	struct pwm_state cstate;
	unsigned long long c;
	u32 val;
	int ret;
	
	pwm_get_state(pwm, &cstate);
	
	if (state->enabled)
	{
		c = clk_get_rate(pc->hosc);
		c *= state->period;
		do_div(c, prescale);
		do_div(c, 1000000000);
		period_cycles = c;
		
		if (period_cycles == 0) {
			period_cycles = 1;
		}
		
		if (period_cycles <= 1024)
		{
			clk_set_parent(pc->clk, pc->hosc);
			clk_set_rate(pc->clk, clk_get_rate(pc->hosc) / period_cycles);
		}
		else
		{
			c = clk_get_rate(pc->losc);
			c *= state->period;
			do_div(c, prescale);
			do_div(c, 1000000000);
			period_cycles = c;
			
			if (period_cycles == 0) {
				period_cycles = 1;
			}
			if (period_cycles > 1024) {
				period_cycles = 1024;
			}
			
			clk_set_parent(pc->clk, pc->losc);
			clk_set_rate(pc->clk, clk_get_rate(pc->losc) / period_cycles);
		}
		
		if (!cstate.enabled)
		{
			ret = clk_prepare_enable(pc->clk);
			if (ret) {
				return ret;
			}
		}
		
		duty_cycles = pwm_get_relative_duty_cycle(state, prescale);
		
		if (!duty_cycles) {
			duty_cycles++;
		}
		
		val = readl(pc->base + PWM_CTL3);
		
		val &= ~(0xFFFFF);
		val |= (prescale & 0x3FF);
		val |= (duty_cycles & 0x3FF) << 10;
		
		if (state->polarity == PWM_POLARITY_NORMAL) {
			val |= BIT(20); // high at duty cycle period
		}
		else {
			val &= ~BIT(20); // low at duty cycle period
		}
		
		writel(val, pc->base + PWM_CTL3);
	}
	else if (cstate.enabled)
	{
		clk_disable_unprepare(pc->clk);
	}
	return 0;
}

static const struct pwm_ops caninos_pwm_ops = {
	.request = caninos_pwm_request,
	.free = caninos_pwm_free,
	.apply = caninos_pwm_apply,
	.owner = THIS_MODULE,
};

static int caninos_pwm_probe(struct platform_device *pdev)
{
	struct caninos_pwm_chip *pc;
	int ret;
	
	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	
	if (!pc) {
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, pc);
	
	pc->clk = devm_clk_get(&pdev->dev, "pwm3");
	
	if (IS_ERR(pc->clk))
	{
		dev_err(&pdev->dev, "devm_clk_get() failed\n");
		return PTR_ERR(pc->clk);
	}
	
	pc->losc = devm_clk_get(&pdev->dev, "losc");
	
	if (IS_ERR(pc->losc))
	{
		dev_err(&pdev->dev, "devm_clk_get() failed\n");
		return PTR_ERR(pc->losc);
	}
	
	pc->hosc = devm_clk_get(&pdev->dev, "hosc");
	
	if (IS_ERR(pc->hosc))
	{
		dev_err(&pdev->dev, "devm_clk_get() failed\n");
		return PTR_ERR(pc->hosc);
	}
	
	pc->pctl = devm_pinctrl_get(&pdev->dev);
	
	if (IS_ERR(pc->pctl))
	{
		dev_err(&pdev->dev, "devm_pinctrl_get() failed\n");
		return PTR_ERR(pc->pctl);
	}
	
	pc->def_state = pinctrl_lookup_state(pc->pctl, PINCTRL_STATE_DEFAULT);
	
	if (IS_ERR(pc->def_state))
	{
		dev_err(&pdev->dev, "could not get pinctrl default state\n");
		return PTR_ERR(pc->def_state);
	}
	
	pc->extio_state = pinctrl_lookup_state(pc->pctl, "extio");
	
	if (IS_ERR(pc->extio_state)) {
		pc->extio_state = NULL; // it is optional
	}
	
	pc->base = of_iomap(pdev->dev.of_node, 0);
	
	if (!pc->base)
	{
		dev_err(&pdev->dev, "of_iomap() failed\n");
		return -ENOMEM;
	}
	
	ret = pinctrl_select_state(pc->pctl, pc->def_state);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "could not select default pinctrl state\n");
		iounmap(pc->base);
		return ret;
	}
	
	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &caninos_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = 1;
	pc->chip.of_xlate = of_pwm_xlate_with_flags;
	pc->chip.of_pwm_n_cells = 3;
	
	ret = pwmchip_add(&pc->chip);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "pwmchip_add() failed\n");
		iounmap(pc->base);
		return ret;
	}
	return 0;
}

static int caninos_pwm_remove(struct platform_device *pdev)
{
	struct caninos_pwm_chip * pc = platform_get_drvdata(pdev);
	
	if (WARN_ON(!pc)) {
		return -ENODEV;
	}
	
	pwmchip_remove(&pc->chip);
	iounmap(pc->base);
	return 0;
}

static const struct of_device_id caninos_pwm_dt_ids[] = {
	{ .compatible = "caninos,k7-pwm", },
	{ .compatible = "caninos,k5-pwm", },
	{ },
};
MODULE_DEVICE_TABLE(of, caninos_pwm_dt_ids);

static struct platform_driver caninos_pwm_driver = {
	.driver	= {
		.name = "pwm-caninos",
		.owner = THIS_MODULE,
		.of_match_table = caninos_pwm_dt_ids,
	},
	.probe = caninos_pwm_probe,
	.remove = caninos_pwm_remove,
};

static int __init caninos_pwm_init(void) {
	return platform_driver_register(&caninos_pwm_driver);
}

static void __exit caninos_pwm_exit(void) {
	platform_driver_unregister(&caninos_pwm_driver);
}

device_initcall(caninos_pwm_init);
module_exit(caninos_pwm_exit);

