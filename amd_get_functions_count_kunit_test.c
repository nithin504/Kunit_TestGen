#include <kunit/test.h>
#include <linux/pinctrl/pinctrl.h>
#include "pinctrl-amd.c"

static void test_amd_get_functions_count(struct kunit *test)
{
	struct pinctrl_dev *dummy_pctldev = NULL;
	int count;

	count = amd_get_functions_count(dummy_pctldev);

	KUNIT_EXPECT_EQ(test, count, (int)ARRAY_SIZE(pmx_functions));
}

static struct kunit_case amd_pinctrl_test_cases[] = {
	KUNIT_CASE(test_amd_get_functions_count),
	{}
};

static struct kunit_suite amd_pinctrl_test_suite = {
	.name = "amd_pinctrl_function_count",
	.test_cases = amd_pinctrl_test_cases,
};

kunit_test_suite(amd_pinctrl_test_suite);