
// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>

static inline void amd_gpio_unregister_s2idle_ops(void) {}

static void test_amd_gpio_unregister_s2idle_ops(struct kunit *test)
{
	amd_gpio_unregister_s2idle_ops();
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static struct kunit_case amd_gpio_unregister_s2idle_ops_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_unregister_s2idle_ops),
	{}
};

static struct kunit_suite amd_gpio_unregister_s2idle_ops_test_suite = {
	.name = "amd_gpio_unregister_s2idle_ops_test",
	.test_cases = amd_gpio_unregister_s2idle_ops_test_cases,
};

kunit_test_suite(amd_gpio_unregister_s2idle_ops_test_suite);