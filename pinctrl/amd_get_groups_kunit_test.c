
// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>
#include <linux/errno.h>

// Include the source file to test
#include "pinctrl-amd.c"

// Mock structures and variables
static struct amd_gpio mock_gpio_dev;
static struct platform_device mock_pdev;
static char mock_iomux_base[4096];
static const char *test_groups[] = {"test_group1", "test_group2"};
static struct amd_function test_pmx_functions[] = {
	{.groups = test_groups, .ngroups = 2}
};

// Mock pinctrl_dev_get_drvdata
void *set_mock_pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev);
#define pinctrl_dev_get_drvdata set_mock_pinctrl_dev_get_drvdata

// Mock dev_err
#define dev_err(dev, fmt, ...) ((void)0)

void *set_mock_pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev)
{
	return &mock_gpio_dev;
}

static void test_amd_get_groups_success(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	const char * const *groups;
	unsigned int num_groups;
	int ret;

	mock_gpio_dev.iomux_base = mock_iomux_base;
	mock_gpio_dev.pdev = &mock_pdev;

	ret = amd_get_groups(&dummy_pctldev, 0, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, groups, test_groups);
	KUNIT_EXPECT_EQ(test, num_groups, 2);
}

static void test_amd_get_groups_no_iomux(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	const char * const *groups;
	unsigned int num_groups;
	int ret;

	mock_gpio_dev.iomux_base = NULL;
	mock_gpio_dev.pdev = &mock_pdev;

	ret = amd_get_groups(&dummy_pctldev, 0, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static struct kunit_case amd_get_groups_test_cases[] = {
	KUNIT_CASE(test_amd_get_groups_success),
	KUNIT_CASE(test_amd_get_groups_no_iomux),
	{}
};

static struct kunit_suite amd_get_groups_test_suite = {
	.name = "amd_get_groups_test",
	.test_cases = amd_get_groups_test_cases,
};

kunit_test_suite(amd_get_groups_test_suite);