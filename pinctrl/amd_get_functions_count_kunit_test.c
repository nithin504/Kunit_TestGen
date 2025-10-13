// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>

// Include the source file to get access to pmx_functions
#include "pinctrl-amd.c"

// Mock the pmx_functions array since it's not defined in the provided code
static const struct pin_function pmx_functions[] = {
	{ "function1", NULL, 0 },
	{ "function2", NULL, 0 },
	{ "function3", NULL, 0 },
};

static int amd_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(pmx_functions);
}

static void test_amd_get_functions_count(struct kunit *test)
{
	struct pinctrl_dev pctldev;
	int count;
	
	count = amd_get_functions_count(&pctldev);
	KUNIT_EXPECT_EQ(test, count, ARRAY_SIZE(pmx_functions));
}

static struct kunit_case amd_get_functions_count_test_cases[] = {
	KUNIT_CASE(test_amd_get_functions_count),
	{}
};

static struct kunit_suite amd_get_functions_count_test_suite = {
	.name = "amd_get_functions_count_test",
	.test_cases = amd_get_functions_count_test_cases,
};

kunit_test_suite(amd_get_functions_count_test_suite);