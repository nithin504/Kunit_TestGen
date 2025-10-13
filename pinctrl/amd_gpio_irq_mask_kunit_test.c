
// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/irq.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#define INTERRUPT_MASK_OFF 11

struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
};

static void amd_gpio_irq_mask(struct irq_data *d)
{
	u32 pin_reg;
	unsigned long flags;
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + (d->hwirq)*4);
	pin_reg &= ~BIT(INTERRUPT_MASK_OFF);
	writel(pin_reg, gpio_dev->base + (d->hwirq)*4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);
}

static char test_mmio_buffer[4096];
static struct amd_gpio mock_gpio_dev;

static void test_amd_gpio_irq_mask(struct kunit *test)
{
	struct irq_data d;
	struct gpio_chip gc;
	unsigned int hwirq = 5;
	u32 initial_reg_value = BIT(INTERRUPT_MASK_OFF);
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	d.hwirq = hwirq;
	d.domain = NULL;
	d.chip_data = &gc;
	
	gc.parent = NULL;
	gc.label = "test";
	gc.owner = THIS_MODULE;
	
	writel(initial_reg_value, mock_gpio_dev.base + hwirq * 4);
	
	amd_gpio_irq_mask(&d);
	
	u32 final_reg_value = readl(mock_gpio_dev.base + hwirq * 4);
	KUNIT_EXPECT_EQ(test, final_reg_value & BIT(INTERRUPT_MASK_OFF), 0);
}

static struct kunit_case amd_gpio_irq_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_irq_mask),
	{}
};

static struct kunit_suite amd_gpio_irq_test_suite = {
	.name = "amd_gpio_irq_test",
	.test_cases = amd_gpio_irq_test_cases,
};

kunit_test_suite(amd_gpio_irq_test_suite);