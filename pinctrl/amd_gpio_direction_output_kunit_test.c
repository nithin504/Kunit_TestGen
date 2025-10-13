// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>

static struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
} mock_gpio_dev;

static char test_mmio_buffer[4096];

static int amd_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
	u32 pin_reg;
	unsigned long flags;
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + offset * 4);
	pin_reg |= BIT(OUTPUT_ENABLE_OFF);
	if (value)
		pin_reg |= BIT(OUTPUT_VALUE_OFF);
	else
		pin_reg &= ~BIT(OUTPUT_VALUE_OFF);
	writel(pin_reg, gpio_dev->base + offset * 4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	return 0;
}

static void test_amd_gpio_direction_output_set_high(struct kunit *test)
{
	struct gpio_chip gc;
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0x0, mock_gpio_dev.base + 0 * 4);
	
	ret = amd_gpio_direction_output(&gc, 0, 1);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + 0 * 4);
	KUNIT_EXPECT_TRUE(test, val & BIT(OUTPUT_ENABLE_OFF));
	KUNIT_EXPECT_TRUE(test, val & BIT(OUTPUT_VALUE_OFF));
}

static void test_amd_gpio_direction_output_set_low(struct kunit *test)
{
	struct gpio_chip gc;
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0xFFFFFFFF, mock_gpio_dev.base + 1 * 4);
	
	ret = amd_gpio_direction_output(&gc, 1, 0);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + 1 * 4);
	KUNIT_EXPECT_TRUE(test, val & BIT(OUTPUT_ENABLE_OFF));
	KUNIT_EXPECT_FALSE(test, val & BIT(OUTPUT_VALUE_OFF));
}

static void test_amd_gpio_direction_output_multiple_offsets(struct kunit *test)
{
	struct gpio_chip gc;
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0x0, mock_gpio_dev.base + 2 * 4);
	writel(0x0, mock_gpio_dev.base + 3 * 4);
	
	ret = amd_gpio_direction_output(&gc, 2, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	ret = amd_gpio_direction_output(&gc, 3, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val2 = readl(mock_gpio_dev.base + 2 * 4);
	u32 val3 = readl(mock_gpio_dev.base + 3 * 4);
	
	KUNIT_EXPECT_TRUE(test, val2 & BIT(OUTPUT_ENABLE_OFF));
	KUNIT_EXPECT_TRUE(test, val2 & BIT(OUTPUT_VALUE_OFF));
	KUNIT_EXPECT_TRUE(test, val3 & BIT(OUTPUT_ENABLE_OFF));
	KUNIT_EXPECT_FALSE(test, val3 & BIT(OUTPUT_VALUE_OFF));
}

static struct kunit_case amd_gpio_direction_output_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_direction_output_set_high),
	KUNIT_CASE(test_amd_gpio_direction_output_set_low),
	KUNIT_CASE(test_amd_gpio_direction_output_multiple_offsets),
	{}
};

static struct kunit_suite amd_gpio_direction_output_test_suite = {
	.name = "amd_gpio_direction_output_test",
	.test_cases = amd_gpio_direction_output_test_cases,
};

kunit_test_suite(amd_gpio_direction_output_test_suite);