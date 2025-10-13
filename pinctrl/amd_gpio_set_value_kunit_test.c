// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>

static int amd_gpio_set_value(struct gpio_chip *gc, unsigned int offset,
			      int value)
{
	u32 pin_reg;
	unsigned long flags;
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + offset * 4);
	if (value)
		pin_reg |= BIT(OUTPUT_VALUE_OFF);
	else
		pin_reg &= ~BIT(OUTPUT_VALUE_OFF);
	writel(pin_reg, gpio_dev->base + offset * 4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	return 0;
}

#define OUTPUT_VALUE_OFF 0

static struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
} mock_gpio_dev;

static char test_mmio_buffer[4096];

static void test_amd_gpio_set_value_high(struct kunit *test)
{
	struct gpio_chip gc;
	unsigned int offset = 1;
	int value = 1;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0x0, mock_gpio_dev.base + offset * 4);
	
	int ret = amd_gpio_set_value(&gc, offset, value);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + offset * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(OUTPUT_VALUE_OFF), BIT(OUTPUT_VALUE_OFF));
}

static void test_amd_gpio_set_value_low(struct kunit *test)
{
	struct gpio_chip gc;
	unsigned int offset = 2;
	int value = 0;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0xFFFFFFFF, mock_gpio_dev.base + offset * 4);
	
	int ret = amd_gpio_set_value(&gc, offset, value);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + offset * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(OUTPUT_VALUE_OFF), 0);
}

static void test_amd_gpio_set_value_multiple_offsets(struct kunit *test)
{
	struct gpio_chip gc;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	for (unsigned int offset = 0; offset < 4; offset++) {
		writel(0x0, mock_gpio_dev.base + offset * 4);
		int ret = amd_gpio_set_value(&gc, offset, offset % 2);
		KUNIT_EXPECT_EQ(test, ret, 0);
		
		u32 val = readl(mock_gpio_dev.base + offset * 4);
		KUNIT_EXPECT_EQ(test, val & BIT(OUTPUT_VALUE_OFF), (offset % 2) ? BIT(OUTPUT_VALUE_OFF) : 0);
	}
}

static struct kunit_case amd_gpio_set_value_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_set_value_high),
	KUNIT_CASE(test_amd_gpio_set_value_low),
	KUNIT_CASE(test_amd_gpio_set_value_multiple_offsets),
	{}
};

static struct kunit_suite amd_gpio_set_value_test_suite = {
	.name = "amd_gpio_set_value_test",
	.test_cases = amd_gpio_set_value_test_cases,
};

kunit_test_suite(amd_gpio_set_value_test_suite);