// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/io.h>

// Mock struct amd_gpio to match the function under test
struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
};

// Copy of the function to test
static int amd_gpio_get_direction(struct gpio_chip *gc, unsigned offset)
{
	unsigned long flags;
	u32 pin_reg;
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + offset * 4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	if (pin_reg & BIT(OUTPUT_ENABLE_OFF))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

// Test constants
#define OUTPUT_ENABLE_OFF 11
#define GPIO_LINE_DIRECTION_OUT 1
#define GPIO_LINE_DIRECTION_IN 0

static char test_mmio_buffer[4096];
static struct amd_gpio mock_gpio_dev;
static struct gpio_chip mock_gpio_chip;

static void test_amd_gpio_get_direction_output(struct kunit *test)
{
	int ret;
	unsigned int offset = 0;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	// Set pin register to have OUTPUT_ENABLE bit set
	writel(BIT(OUTPUT_ENABLE_OFF), mock_gpio_dev.base + offset * 4);
	
	ret = amd_gpio_get_direction(&mock_gpio_chip, offset);
	KUNIT_EXPECT_EQ(test, ret, GPIO_LINE_DIRECTION_OUT);
}

static void test_amd_gpio_get_direction_input(struct kunit *test)
{
	int ret;
	unsigned int offset = 1;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	// Set pin register to have OUTPUT_ENABLE bit cleared
	writel(0x0, mock_gpio_dev.base + offset * 4);
	
	ret = amd_gpio_get_direction(&mock_gpio_chip, offset);
	KUNIT_EXPECT_EQ(test, ret, GPIO_LINE_DIRECTION_IN);
}

static void test_amd_gpio_get_direction_multiple_offsets(struct kunit *test)
{
	int ret;
	
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	// Test offset 2: output
	writel(BIT(OUTPUT_ENABLE_OFF), mock_gpio_dev.base + 2 * 4);
	ret = amd_gpio_get_direction(&mock_gpio_chip, 2);
	KUNIT_EXPECT_EQ(test, ret, GPIO_LINE_DIRECTION_OUT);
	
	// Test offset 3: input
	writel(0x0, mock_gpio_dev.base + 3 * 4);
	ret = amd_gpio_get_direction(&mock_gpio_chip, 3);
	KUNIT_EXPECT_EQ(test, ret, GPIO_LINE_DIRECTION_IN);
}

static struct kunit_case amd_gpio_get_direction_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_get_direction_output),
	KUNIT_CASE(test_amd_gpio_get_direction_input),
	KUNIT_CASE(test_amd_gpio_get_direction_multiple_offsets),
	{}
};

static struct kunit_suite amd_gpio_get_direction_test_suite = {
	.name = "amd_gpio_get_direction_test",
	.test_cases = amd_gpio_get_direction_test_cases,
};

kunit_test_suite(amd_gpio_get_direction_test_suite);