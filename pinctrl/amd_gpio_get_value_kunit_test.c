// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/io.h>

static int amd_gpio_get_value(struct gpio_chip *gc, unsigned offset)
{
	u32 pin_reg;
	unsigned long flags;
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + offset * 4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	return !!(pin_reg & BIT(0));
}

#define PIN_STS_OFF 0

struct amd_gpio {
	void __iomem *base;
	raw_spinlock_t lock;
};

static char test_buffer[4096];
static struct amd_gpio mock_gpio_dev;
static struct gpio_chip mock_gpio_chip;

static void test_amd_gpio_get_value_pin_reg_set(struct kunit *test)
{
	int ret;
	u32 *reg;

	mock_gpio_dev.base = test_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	reg = (u32 *)(test_buffer + 0 * 4);
	*reg = BIT(PIN_STS_OFF);

	ret = amd_gpio_get_value(&mock_gpio_chip, 0);
	KUNIT_EXPECT_EQ(test, ret, 1);
}

static void test_amd_gpio_get_value_pin_reg_clear(struct kunit *test)
{
	int ret;
	u32 *reg;

	mock_gpio_dev.base = test_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	reg = (u32 *)(test_buffer + 1 * 4);
	*reg = 0;

	ret = amd_gpio_get_value(&mock_gpio_chip, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_get_value_multiple_offsets(struct kunit *test)
{
	int ret;
	u32 *reg1, *reg2;

	mock_gpio_dev.base = test_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	reg1 = (u32 *)(test_buffer + 2 * 4);
	*reg1 = BIT(PIN_STS_OFF);
	
	reg2 = (u32 *)(test_buffer + 3 * 4);
	*reg2 = 0;

	ret = amd_gpio_get_value(&mock_gpio_chip, 2);
	KUNIT_EXPECT_EQ(test, ret, 1);
	
	ret = amd_gpio_get_value(&mock_gpio_chip, 3);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_get_value_non_bit_pin_reg(struct kunit *test)
{
	int ret;
	u32 *reg;

	mock_gpio_dev.base = test_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	
	reg = (u32 *)(test_buffer + 4 * 4);
	*reg = 0x12345678;

	ret = amd_gpio_get_value(&mock_gpio_chip, 4);
	KUNIT_EXPECT_EQ(test, ret, !!(0x12345678 & BIT(PIN_STS_OFF)));
}

static struct kunit_case amd_gpio_get_value_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_get_value_pin_reg_set),
	KUNIT_CASE(test_amd_gpio_get_value_pin_reg_clear),
	KUNIT_CASE(test_amd_gpio_get_value_multiple_offsets),
	KUNIT_CASE(test_amd_gpio_get_value_non_bit_pin_reg),
	{}
};

static struct kunit_suite amd_gpio_get_value_test_suite = {
	.name = "amd_gpio_get_value_test",
	.test_cases = amd_gpio_get_value_test_cases,
};

kunit_test_suite(amd_gpio_get_value_test_suite);