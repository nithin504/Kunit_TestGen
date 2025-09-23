#include <kunit/test.h>
#include <linux/io.h>
#include "pinctrl-amd.c" // Include for coverage

#define MOCK_BASE_ADDR 0x1000
#define INTERNAL_GPIO0_DEBOUNCE 0x2 // Example bitmask
#define DB_TYPE_REMOVE_GLITCH 0x1
#define DB_CNTRL_OFF 28
#define DB_TMR_OUT_MASK 0xFF
#define DB_TMR_LARGE_OFF 9
#define DB_CNTRL_MASK 0x7

static struct amd_gpio gpio_dev;
static char debounce_test_buffer[8192];

static void test_amd_gpio_set_debounce_less_than_61(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;

	// Clear WAKE_INT_MASTER_REG to avoid INTERNAL_GPIO0_DEBOUNCE
	*((u32 *)(debounce_test_buffer + WAKE_INT_MASTER_REG)) = 0;

	// Set debounce < 61 to hit the first range
	ret = amd_gpio_set_debounce(&gpio_dev, 1, 50);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_zero_offset(struct kunit *test)
{
	u32 val = INTERNAL_GPIO0_DEBOUNCE;
	u32 *reg = (u32 *)(debounce_test_buffer + WAKE_INT_MASTER_REG);
	*reg = val;
	
	// amd_gpio_set_debounce(&gpio_dev, 0, 1000); missing part from image
	// KUNIT_EXPECT_EQ(test, ret, 0); missing part from image
}

static void test_amd_gpio_set_debounce_valid_range(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 1, 2000); // falls in 3rd range
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_max_range(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 2, 300000); // falls in 5th range
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_invalid_range(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 3, 2000000); // exceeds max
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void test_amd_gpio_set_debounce_zero_debounce(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 4, 0); // disables debounce
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_second_range(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 5, 100); // hits 2nd range
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_fourth_range(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	ret = amd_gpio_set_debounce(&gpio_dev, 6, 10000); // hits 4th range
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_amd_gpio_set_debounce_internal_gpio0_debounce(struct kunit *test)
{
	int ret;
	gpio_dev.base = debounce_test_buffer;
	
	// Set WAKE_INT_MASTER_REG to include INTERNAL_GPIO0_DEBOUNCE
	*((u32 *)(debounce_test_buffer + WAKE_INT_MASTER_REG)) = INTERNAL_GPIO0_DEBOUNCE;
	
	// Provide a non-zero debounce value, which should be overridden to 0
	ret = amd_gpio_set_debounce(&gpio_dev, 0, 100);
	
	// Expect success
	KUNIT_EXPECT_EQ(test, ret, 0);
	
	// Optionally, verify that debounce logic was skipped (e.g., by checking register value)
}

static struct kunit_case gpio_debounce_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_set_debounce_less_than_61),
	KUNIT_CASE(test_amd_gpio_set_debounce_zero_offset),
	KUNIT_CASE(test_amd_gpio_set_debounce_max_range),
	KUNIT_CASE(test_amd_gpio_set_debounce_invalid_range),
	KUNIT_CASE(test_amd_gpio_set_debounce_zero_debounce),
	KUNIT_CASE(test_amd_gpio_set_debounce_second_range),
	KUNIT_CASE(test_amd_gpio_set_debounce_fourth_range),
	KUNIT_CASE(test_amd_gpio_set_debounce_internal_gpio0_debounce),
	{}
};

static struct kunit_suite gpio_debounce_test_suite = {
	.name = "gpio_debounce_test",
	.test_cases = gpio_debounce_test_cases,
};

kunit_test_suite(gpio_debounce_test_suite);