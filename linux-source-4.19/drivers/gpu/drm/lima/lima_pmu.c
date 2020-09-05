// SPDX-License-Identifier: GPL-2.0 OR MIT
/* Copyright 2017-2018 Qiang Yu <yuq825@gmail.com> */

#include <linux/io.h>
#include <linux/device.h>

#include "lima_device.h"
#include "lima_pmu.h"
#include "lima_regs.h"

#define pmu_write(reg, data) writel(data, ip->iomem + LIMA_PMU_##reg)
#define pmu_read(reg) readl(ip->iomem + LIMA_PMU_##reg)

static int lima_pmu_wait_cmd(struct lima_ip *ip)
{
	struct lima_device *dev = ip->dev;
	u32 stat, timeout;

	for (timeout = 1000000; timeout > 0; timeout--) {
		stat = pmu_read(INT_RAWSTAT);
		if (stat & LIMA_PMU_INT_CMD_MASK)
			break;
	}

	if (!timeout) {
		dev_err(dev->dev, "timeout wait pmd cmd\n");
		return -ETIMEDOUT;
	}

	pmu_write(INT_CLEAR, LIMA_PMU_INT_CMD_MASK);
	return 0;
}

int lima_pmu_init(struct lima_ip *ip)
{
	int err;
	u32 stat;

	pmu_write(INT_MASK, 0);

	/* If this value is too low, when in high GPU clk freq,
	 * GPU will be in unstable state. */
	pmu_write(SW_DELAY, 0xffff);

	/* status reg 1=off 0=on */
	stat = pmu_read(STATUS);

	/* power up all ip */
	if (stat) {
		pmu_write(POWER_UP, stat);
		err = lima_pmu_wait_cmd(ip);
		if (err)
			return err;
	}
	return 0;
}

void lima_pmu_fini(struct lima_ip *ip)
{

}
