#include <linux/init.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqchip.h>
#include <linux/syscore_ops.h>
#include <asm/irq.h>

#define OWL_MAX_NR_SIRQS			3

/* INTC_EXTCTL register offset for S900 */
#define S900_INTC_EXTCTL0			0x200
#define S900_INTC_EXTCTL1			0x528
#define S900_INTC_EXTCTL2			0x52C

#define S700_INTC_EXTCTL			0x200
/* INTC_EXTCTL register offset for S500 */
#define S500_INTC_EXTCTL			0x200

#define INTC_EXTCTL_PENDING			(0x1 << 0)
#define INTC_EXTCTL_CLK_SEL			(0x1 << 4)
#define		INTC_EXTCTL_CLK_SEL_32K			(0x0 << 4)
#define		INTC_EXTCTL_CLK_SEL_24M			(0x1 << 4)
#define INTC_EXTCTL_EN					(0x1 << 5)
#define INTC_EXTCTL_TYPE(x)			(((x) & 0x03) << 6)
#define		INTC_EXTCTL_TYPE_MASK			INTC_EXTCTL_TYPE(0x3)
#define		INTC_EXTCTL_TYPE_HIGH			INTC_EXTCTL_TYPE(0x0)
#define		INTC_EXTCTL_TYPE_LOW			INTC_EXTCTL_TYPE(0x1)
#define		INTC_EXTCTL_TYPE_RISING			INTC_EXTCTL_TYPE(0x2)
#define		INTC_EXTCTL_TYPE_FALLING		INTC_EXTCTL_TYPE(0x3)

struct owl_sirq_info {
	void __iomem		*base;
	struct irq_domain	*irq_domain;
	unsigned long		reg;
	unsigned long		hwirq;
	unsigned int		virq;
	unsigned int		parent_irq;
	bool			share_reg;
};

static struct owl_sirq_info s900_sirq_info[OWL_MAX_NR_SIRQS] = {
	{ .reg = S900_INTC_EXTCTL0, .share_reg = false },
	{ .reg = S900_INTC_EXTCTL1, .share_reg = false },
	{ .reg = S900_INTC_EXTCTL2, .share_reg = false },
};

static struct owl_sirq_info s700_sirq_info[OWL_MAX_NR_SIRQS] = {
	{ .reg = S700_INTC_EXTCTL, .share_reg = true },
	{ .reg = S700_INTC_EXTCTL, .share_reg = true },
	{ .reg = S700_INTC_EXTCTL, .share_reg = true },
};

/* Some SOC's sirqs share a same ctrl register,
 * so we need use one spinlock for all sirqs.
 */
static DEFINE_SPINLOCK(owl_sirq_lock);

static unsigned int sirq_read_extctl(struct owl_sirq_info *sirq)
{
	unsigned int val;

	val = readl_relaxed(sirq->base + sirq->reg);
	if (sirq->share_reg)
		val = (val >> (2 - sirq->hwirq) * 8) & 0xff;

	return val;
}

static void sirq_write_extctl(struct owl_sirq_info *sirq, unsigned int extctl)
{
	unsigned int val;

	if (sirq->share_reg) {
		val = readl_relaxed(sirq->base + sirq->reg);
		val &= ~(0xff << (2 - sirq->hwirq) * 8);
		extctl &= 0xff;
		extctl = (extctl << (2 - sirq->hwirq) * 8) | val;
	}

	writel_relaxed(extctl, sirq->base + sirq->reg);
}

static void owl_sirq_ack(struct irq_data *d)
{
	struct owl_sirq_info *sirq = irq_data_get_irq_chip_data(d);
	unsigned int extctl;
	unsigned long flags;

	spin_lock_irqsave(&owl_sirq_lock, flags);

	extctl = sirq_read_extctl(sirq);
	extctl |= INTC_EXTCTL_PENDING;
	sirq_write_extctl(sirq, extctl);

	spin_unlock_irqrestore(&owl_sirq_lock, flags);
}

static void owl_sirq_mask(struct irq_data *d)
{
	struct owl_sirq_info *sirq = irq_data_get_irq_chip_data(d);
	unsigned int extctl;
	unsigned long flags;

	spin_lock_irqsave(&owl_sirq_lock, flags);

	extctl = sirq_read_extctl(sirq);
	extctl &= ~(INTC_EXTCTL_EN);
	sirq_write_extctl(sirq, extctl);

	spin_unlock_irqrestore(&owl_sirq_lock, flags);
}

static void owl_sirq_unmask(struct irq_data *d)
{
	struct owl_sirq_info *sirq = irq_data_get_irq_chip_data(d);
	unsigned int extctl;
	unsigned long flags;

	spin_lock_irqsave(&owl_sirq_lock, flags);

	/* we don't hold the irq pending generated before irq enabled */
	extctl = sirq_read_extctl(sirq);
	extctl |= INTC_EXTCTL_EN;
	sirq_write_extctl(sirq, extctl);

	spin_unlock_irqrestore(&owl_sirq_lock, flags);
}

static int owl_sirq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct owl_sirq_info *sirq = irq_data_get_irq_chip_data(d);
	unsigned int extctl, type;
	unsigned long flags;

	spin_lock_irqsave(&owl_sirq_lock, flags);

	switch (flow_type) {
	case IRQF_TRIGGER_LOW:
		type = INTC_EXTCTL_TYPE_LOW;
		break;
	case IRQF_TRIGGER_HIGH:
		type = INTC_EXTCTL_TYPE_HIGH;
		break;
	case IRQF_TRIGGER_FALLING:
		type = INTC_EXTCTL_TYPE_FALLING;
		break;
	case IRQF_TRIGGER_RISING:
		type = INTC_EXTCTL_TYPE_RISING;
		break;
	default:
		return  -EINVAL;
	}

	extctl = sirq_read_extctl(sirq);
	extctl &= ~(INTC_EXTCTL_PENDING | INTC_EXTCTL_TYPE_MASK);
	extctl |= type;
	sirq_write_extctl(sirq, extctl);

	spin_unlock_irqrestore(&owl_sirq_lock, flags);

	/* TODO: pull up/down the pads */

	return 0;
}

static void owl_sirq_handler(struct irq_desc *desc)
{
	unsigned int irq = irq_desc_get_irq(desc);
	struct owl_sirq_info *sirq = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);
	unsigned int extctl;
	
	chained_irq_enter(chip, desc);
	
	extctl = sirq_read_extctl(sirq);
	
	if (extctl & INTC_EXTCTL_PENDING) {
		generic_handle_irq(sirq->virq);
	}
	
	chained_irq_exit(chip, desc);
}

static struct irq_chip owl_irq_chip = {
	.name = "owl-sirq",
	.irq_ack = owl_sirq_ack,
	.irq_mask = owl_sirq_mask,
	.irq_unmask = owl_sirq_unmask,
	.irq_set_type = owl_sirq_set_type,
};

int __init owl_sirq_init(struct owl_sirq_info *sirq_info, int nr_sirq,
			struct device_node *np)
{
	struct owl_sirq_info *sirq;
	void __iomem *base;
	struct irq_domain *irq_domain;
	int i, irq_base;

	pr_info("[OWL] init sirqs, nr_sirq %d\n", nr_sirq);

	base = of_iomap(np, 0);
	if (!base) {
		pr_err("%s: failed to map sirq registers\n", __func__);
		return -ENXIO;
	}

	irq_base = irq_alloc_descs(-1, 32, nr_sirq, 0);
	if (IS_ERR_VALUE(irq_base)) {
		pr_err("%s: irq desc alloc failed\n", __func__);
		goto err_unmap;
	}

	irq_domain = irq_domain_add_legacy(np, nr_sirq, irq_base, 0,
			&irq_domain_simple_ops, NULL);
	if (WARN_ON(!irq_domain)) {
		pr_warn("%s: irq domain init failed\n", __func__);
		goto err_free_desc;
	}

	for (i = 0; i < nr_sirq; i++) {
		sirq = &sirq_info[i];

		sirq->base = base;
		sirq->irq_domain = irq_domain;
		sirq->hwirq = i;
		sirq->virq = irq_base + i;

		sirq->parent_irq = irq_of_parse_and_map(np, i);
		irq_set_handler_data(sirq->parent_irq, sirq);
		irq_set_chained_handler(sirq->parent_irq, owl_sirq_handler);

		irq_set_chip_and_handler(sirq->virq, &owl_irq_chip, handle_level_irq);

		irq_clear_status_flags(sirq->virq, IRQ_NOREQUEST);
		irq_set_chip_data(sirq->virq, sirq);
		
		pr_debug("%s: virq %d, hwirq %ld, parrent_irq %d\n", __func__,
			sirq->virq, sirq->hwirq, sirq->parent_irq);
	}

	return 0;

err_free_desc:
	irq_free_descs(irq_base, nr_sirq);
err_unmap:
	iounmap(base);
	return -ENXIO;
}

int __init s700_sirq_of_init(struct device_node *np, struct device_node *parent)
{
	return owl_sirq_init(s700_sirq_info, ARRAY_SIZE(s700_sirq_info), np);
}

IRQCHIP_DECLARE(s700_sirq, "caninos,k7-sirq", s700_sirq_of_init);

int __init s900_sirq_of_init(struct device_node *np, struct device_node *parent)
{
	return owl_sirq_init(s900_sirq_info, ARRAY_SIZE(s900_sirq_info), np);
}

IRQCHIP_DECLARE(s900_sirq, "caninos,k9-sirq", s900_sirq_of_init);

