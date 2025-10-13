// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#define WAKE_INT_STATUS_REG0 0x00
#define WAKE_INT_STATUS_REG1 0x04
#define WAKE_INT_MASTER_REG 0x08
#define PIN_IRQ_PENDING (1 << 28)
#define WAKE_STS_OFF 29
#define INTERRUPT_MASK_OFF 30
#define EOI_MASK (1 << 31)

struct amd_gpio {
	struct gpio_chip gc;
	raw_spinlock_t lock;
	void __iomem *base;
	struct platform_device *pdev;
};

static bool do_amd_gpio_irq_handler(int irq, void *dev_id)
{
	struct amd_gpio *gpio_dev = dev_id;
	struct gpio_chip *gc = &gpio_dev->gc;
	unsigned int i, irqnr;
	unsigned long flags;
	u32 __iomem *regs;
	bool ret = false;
	u32  regval;
	u64 status, mask;

	/* Read the wake status */
	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	status = readl(gpio_dev->base + WAKE_INT_STATUS_REG1);
	status <<= 32;
	status |= readl(gpio_dev->base + WAKE_INT_STATUS_REG0);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	/* Bit 0-45 contain the relevant status bits */
	status &= (1ULL << 46) - 1;
	regs = gpio_dev->base;
	for (mask = 1, irqnr = 0; status; mask <<= 1, regs += 4, irqnr += 4) {
		if (!(status & mask))
			continue;
		status &= ~mask;

		/* Each status bit covers four pins */
		for (i = 0; i < 4; i++) {
			regval = readl(regs + i);

			if (regval & PIN_IRQ_PENDING)
				pm_pr_dbg("GPIO %d is active: 0x%x",
					  irqnr + i, regval);

			/* caused wake on resume context for shared IRQ */
			if (irq < 0 && (regval & BIT(WAKE_STS_OFF)))
				return true;

			if (!(regval & PIN_IRQ_PENDING) ||
			    !(regval & BIT(INTERRUPT_MASK_OFF)))
				continue;
			generic_handle_domain_irq_safe(gc->irq.domain, irqnr + i);

			/* Clear interrupt.
			 * We must read the pin register again, in case the
			 * value was changed while executing
			 * generic_handle_domain_irq() above.
			 * If the line is not an irq, disable it in order to
			 * avoid a system hang caused by an interrupt storm.
			 */
			raw_spin_lock_irqsave(&gpio_dev->lock, flags);
			regval = readl(regs + i);
			if (!gpiochip_line_is_irq(gc, irqnr + i)) {
				regval &= ~BIT(INTERRUPT_MASK_OFF);
				dev_dbg(&gpio_dev->pdev->dev,
					"Disabling spurious GPIO IRQ %d\n",
					irqnr + i);
			} else {
				ret = true;
			}
			writel(regval, regs + i);
			raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);
		}
	}
	/* did not cause wake on resume context for shared IRQ */
	if (irq < 0)
		return false;

	/* Signal EOI to the GPIO unit */
	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	regval = readl(gpio_dev->base + WAKE_INT_MASTER_REG);
	regval |= EOI_MASK;
	writel(regval, gpio_dev->base + WAKE_INT_MASTER_REG);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	return ret;
}

static char test_mmio_buffer[8192];
static struct amd_gpio mock_gpio_dev;
static struct irq_domain mock_irq_domain;
static struct gpio_chip mock_gc;
static struct platform_device mock_pdev;

static int mock_gpiochip_line_is_irq(struct gpio_chip *gc, unsigned int offset)
{
	return (offset % 2) == 0;
}

static void mock_generic_handle_domain_irq_safe(struct irq_domain *domain, unsigned int hwirq)
{
}

#define gpiochip_line_is_irq mock_gpiochip_line_is_irq
#define generic_handle_domain_irq_safe mock_generic_handle_domain_irq_safe

static void test_do_amd_gpio_irq_handler_normal_irq(struct kunit *test)
{
	bool ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.gc.irq.domain = &mock_irq_domain;
	mock_gpio_dev.pdev = &mock_pdev;
	
	writel(0x1, test_mmio_buffer + WAKE_INT_STATUS_REG0);
	writel(0x0, test_mmio_buffer + WAKE_INT_STATUS_REG1);
	writel(PIN_IRQ_PENDING | BIT(INTERRUPT_MASK_OFF), test_mmio_buffer);
	
	ret = do_amd_gpio_irq_handler(1, &mock_gpio_dev);
	KUNIT_EXPECT_EQ(test, ret, true);
}

static void test_do_amd_gpio_irq_handler_wake_irq(struct kunit *test)
{
	bool ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.gc.irq.domain = &mock_irq_domain;
	mock_gpio_dev.pdev = &mock_pdev;
	
	writel(0x1, test_mmio_buffer + WAKE_INT_STATUS_REG0);
	writel(0x0, test_mmio_buffer + WAKE_INT_STATUS_REG1);
	writel(BIT(WAKE_STS_OFF), test_mmio_buffer);
	
	ret = do_amd_gpio_irq_handler(-1, &mock_gpio_dev);
	KUNIT_EXPECT_EQ(test, ret, true);
}

static void test_do_amd_gpio_irq_handler_no_wake(struct kunit *test)
{
	bool ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.gc.irq.domain = &mock_irq_domain;
	mock_gpio_dev.pdev = &mock_pdev;
	
	writel(0x1, test_mmio_buffer + WAKE_INT_STATUS_REG0);
	writel(0x0, test_mmio_buffer + WAKE_INT_STATUS_REG1);
	writel(PIN_IRQ_PENDING, test_mmio_buffer);
	
	ret = do_amd_gpio_irq_handler(-1, &mock_gpio_dev);
	KUNIT_EXPECT_EQ(test, ret, false);
}

static void test_do_amd_gpio_irq_handler_no_pending_irq(struct kunit *test)
{
	bool ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.gc.irq.domain = &mock_irq_domain;
	mock_gpio_dev.pdev = &mock_pdev;
	
	writel(0x1, test_mmio_buffer + WAKE_INT_STATUS_REG0);
	writel(0x0, test_mmio_buffer + WAKE_INT_STATUS_REG1);
	writel(0x0, test_mmio_buffer);
	
	ret = do_amd_gpio_irq_handler(1, &mock_gpio_dev);
	KUNIT_EXPECT_EQ(test, ret, false);
}

static void test_do_amd_gpio_irq_handler_spurious_irq(struct kunit *test)
{
	bool ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.gc.irq.domain = &mock_irq_domain;
	mock_gpio_dev.pdev = &mock_pdev;
	
	writel(0x1, test_mmio_buffer + WAKE_INT_STATUS_REG0);
	writel(0x0, test_mmio_buffer + WAKE_INT_STATUS_REG1);
	writel(PIN_IRQ_PENDING | BIT(INTERRUPT_MASK_OFF), test_mmio_buffer + 4);
	
	ret = do_amd_gpio_irq_handler(1, &mock_gpio_dev);
	KUNIT_EXPECT_EQ(test, ret, true);
}

static struct kunit_case amd_gpio_irq_test_cases[] = {
	KUNIT_CASE(test_do_amd_gpio_irq_handler_normal_irq),
	KUNIT_CASE(test_do_amd_gpio_irq_handler_wake_irq),
	KUNIT_CASE(test_do_amd_gpio_irq_handler_no_wake),
	KUNIT_CASE(test_do_amd_gpio_irq_handler_no_pending_irq),
	KUNIT_CASE(test_do_amd_gpio_irq_handler_spurious_irq),
	{}
};

static struct kunit_suite amd_gpio_irq_test_suite = {
	.name = "amd_gpio_irq_test",
	.test_cases = amd_gpio_irq_test_cases,
};

kunit_test_suite(amd_gpio_irq_test_suite);