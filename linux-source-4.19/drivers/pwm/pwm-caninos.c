#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
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
	bool enabled;
	
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
	pinctrl_select_state(pc->pctl, pc->extio_state);
	clk_prepare_enable(pc->clk);
	return 0;
}

static void caninos_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct caninos_pwm_chip *pc = to_caninos_pwm_chip(chip);
	clk_disable_unprepare(pc->clk);
	pinctrl_select_state(pc->pctl, pc->def_state);
}

/*
 * struct pwm_state - state of a PWM channel
 * @period: PWM period (in nanoseconds)
 * @duty_cycle: PWM duty cycle (in nanoseconds)
 * @polarity: PWM polarity
 * @enabled: PWM enabled status
 */
//struct pwm_state {
//	unsigned int period;
//	unsigned int duty_cycle;
//	enum pwm_polarity polarity;
//	bool enabled;
//};

static int caninos_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
                             struct pwm_state *state)
{
	//u64 rate;
	//rate = clk_get_rate(ip->clk);
	
	unsigned long losc, hosc;
	
	
	
	
	//state->period in nanoseconds
	//state->duty_cycle in nanoseconds
	//state->polarity
	//state->enabled
	
	//divide 1024

	//0->9
	
	//if (state->polarity == PWM_POLARITY_NORMAL) {
	//	value |= BIT(20); // high at duty cycle period
	//}
	//else {
	//	value &= BIT(20); // low at duty cycle period
	//}
	
	
	return 0;
}

static void caninos_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
                                  struct pwm_state *state)
{
	return;
}

static const struct pwm_ops caninos_pwm_ops = {
	.request = caninos_pwm_request,
	.free = caninos_pwm_free,
	.apply = caninos_pwm_apply,
	.get_state = caninos_pwm_get_state,
	.owner = THIS_MODULE,
};

static int caninos_pwm_probe(struct platform_device *pdev)
{
	struct caninos_pwm_chip *pc;
	struct resource *res;
	int ret;
	
	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	
	if (!pc) {
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, pc);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(&pdev->dev, "could not get memory resource\n");
		return -EINVAL;
	}
	
	pc->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	
	if (pc->base == NULL)
	{
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}
	
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
	
	if (IS_ERR(pc->extio_state))
	{
		dev_err(&pdev->dev, "could not get pinctrl extio state\n");
		return PTR_ERR(pc->extio_state);
	}
	
	ret = pinctrl_select_state(pc->pctl, pc->def_state);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "could not select default pinctrl state\n");
		return ret;
	}
	
	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &caninos_pwm_ops;
	pc->chip.base = -1;
	pc->chip.npwm = 1;
	pc->chip.of_xlate = of_pwm_xlate_with_flags;
	pc->chip.of_pwm_n_cells = 3;
	
	pc->enabled = false;
	
	ret = pwmchip_add(&pc->chip);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "pwmchip_add() failed\n");
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
	
	return pwmchip_remove(&pc->chip);
}

static const struct of_device_id caninos_pwm_dt_ids[] = {
	{ .compatible = "caninos,k7-pwm", },
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

