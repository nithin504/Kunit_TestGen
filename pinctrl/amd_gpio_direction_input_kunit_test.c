// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/io.h>

static int amd_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	unsigned long flags;
	u32 pin_reg;
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + offset * 4);
	pin_reg &= ~BIT(OUTPUT_ENABLE_OFF);
	writel(pin_reg, gpio_dev->base + offset * 4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	return 0;
}

#define OUTPUT_ENABLE_OFF 0

struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
};

static char test_mmio_buffer[4096];
static struct amd_gpio mock_gpio_dev;
static struct gpio_chip mock_gpio_chip;

static void test_amd_gpio_direction_input_success(struct kunit *test)
{
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0xFFFFFFFF, mock_gpio_dev.base + 0 * 4);
	
	ret = amd_gpio_direction_input(&mock_gpio_chip, 0);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + 0 * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(OUTPUT_ENABLE_OFF), 0);
}

static void test_amd_gpio_direction_input_multiple_offsets(struct kunit *test)
{
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0xFFFFFFFF, mock_gpio_dev.base + 1 * 4);
	writel(0xFFFFFFFF, mock_gpio_dev.base + 5 * 4);
	writel(0xFFFFFFFF, mock_gpio_dev.base + 10 * 4);
	
	ret = amd_gpio_direction_input(&mock_gpio_chip, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	ret = amd_gpio_direction_input(&mock_gpio_chip, 5);
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	ret = amd_gpio_direction_input(&mock_gpio_chip, 10);
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val1 = readl(mock_gpio_dev.base + 1 * 4);
	u32 val5 = readl(mock_gpio_dev.base + 5 * 4);
	u32 val10 = readl(mock_gpio_dev.base + 10 * 4);
	
	KUNIT_EXPECT_EQ(test, val1 & BIT(OUTPUT_ENABLE_OFF), 0);
	KUNIT_EXPECT_EQ(test, val5 & BIT(OUTPUT_ENABLE_OFF), 0);
	KUNIT_EXPECT_EQ(test, val10 & BIT(OUTPUT_ENABLE_OFF), 0);
}

static void test_amd_gpio_direction_input_already_disabled(struct kunit *test)
{
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	writel(0x00000000, mock_gpio_dev.base + 2 * 4);
	
	ret = amd_gpio_direction_input(&mock_gpio_chip, 2);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	u32 val = readl(mock_gpio_dev.base + 2 * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(OUTPUT_ENABLE_OFF), 0);
}

static struct kunit_case amd_gpio_direction_input_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_direction_input_success),
	KUNIT_CASE(test_amd_gpio_direction_input_multiple_offsets),
	KUNIT_CASE(test_amd_gpio_direction_input_already_disabled),
	{}
};

static struct kunit_suite amd_gpio_direction_input_test_suite = {
	.name = "amd_gpio_direction_input_test",
	.test_cases = amd_gpio_direction_input_test_cases,
};

kunit_test_suite(amd_gpio_direction_input_test_suite);