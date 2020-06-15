
#include <linux/delay.h>
#include <linux/io.h>

#define SPS_PG_CTL	0x0

int caninos_sps_set_pg
	(void __iomem *base, u32 pwr_mask, u32 ack_mask, bool enable)
{
	int timeout = 5000;
	bool ack;
	u32 val;
	
	val = readl(base + SPS_PG_CTL);
	ack = (val & ack_mask) == ack_mask;
	
	if (ack == enable) {
		return 0;
	}
	if (enable) {
		val |= pwr_mask;
	}
	else {
		val &= ~pwr_mask;
	}
	
	writel(val, base + SPS_PG_CTL);
	
	while (timeout > 0)
	{
		val = readl(base + SPS_PG_CTL);
		ack = (val & ack_mask) == ack_mask;
		
		if (ack == enable) {
			break;
		}
		
		udelay(50);
		timeout -= 50;
	}
	
	if (timeout <= 0) {
		return -ETIMEDOUT;
	}
	
	udelay(10);
	return 0;
}
EXPORT_SYMBOL_GPL(caninos_sps_set_pg);

