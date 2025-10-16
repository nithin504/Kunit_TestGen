// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmuc.h>
#include "amd_gpio.h"
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "pinctrl-amd.c"
struct amd_gpio {
	void __iomem *iomux_base;
	struct platform_device *pdev;
};

struct pinmux_function {
	const char * const *groups;
	const unsigned int ngroups;
};

#define MAX_FUNCTIONS 10
static struct pinmux_function pmx_functions[MAX_FUNCTIONS];

static struct amd_gpio *mock_gpio_dev;
static struct platform_device mock_pdev;
static struct pinctrl_dev mock_pctrldev;

static void *mock_pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev)
{
	return mock_gpio_dev;
}

#define pinctrl_dev_get_drvdata mock_pinctrl_dev_get_drvdata

static int amd_get_groups(struct pinctrl_dev *pctrldev, unsigned int selector,
			  const char * const **groups,
			  unsigned int * const num_groups)
{
	struct amd_gpio *gpio_dev = pinctrl_dev_get_drvdata(pctrldev);

	if (!gpio_dev->iomux_base) {
		dev_err(&gpio_dev->pdev->dev, "iomux function %d group not supported\n", selector);
		return -EINVAL;
	}

	*groups = pmx_functions[selector].groups;
	*num_groups = pmx_functions[selector].ngroups;
	return 0;
}

static void test_amd_get_groups_null_iomux_base(struct kunit *test)
{
	const char * const *groups;
	unsigned int num_groups;
	int ret;

	mock_gpio_dev->iomux_base = NULL;
	ret = amd_get_groups(&mock_pctrldev, 0, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void test_amd_get_groups_valid_selector(struct kunit *test)
{
	const char * const *groups;
	unsigned int num_groups;
	const char * const test_groups[] = {"group1", "group2"};
	int ret;

	mock_gpio_dev->iomux_base = (void __iomem *)0x1234;
	pmx_functions[0].groups = test_groups;
	pmx_functions[0].ngroups = 2;

	ret = amd_get_groups(&mock_pctrldev, 0, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, groups, test_groups);
	KUNIT_EXPECT_EQ(test, num_groups, 2U);
}

static void test_amd_get_groups_edge_selector(struct kunit *test)
{
	const char * const *groups;
	unsigned int num_groups;
	const char * const test_groups[] = {"group_last"};
	int ret;

	mock_gpio_dev->iomux_base = (void __iomem *)0x1234;
	pmx_functions[MAX_FUNCTIONS - 1].groups = test_groups;
	pmx_functions[MAX_FUNCTIONS - 1].ngroups = 1;

	ret = amd_get_groups(&mock_pctrldev, MAX_FUNCTIONS - 1, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, groups, test_groups);
	KUNIT_EXPECT_EQ(test, num_groups, 1U);
}

static void test_amd_get_groups_uninitialized_entry(struct kunit *test)
{
	const char * const *groups;
	unsigned int num_groups;
	int ret;

	mock_gpio_dev->iomux_base = (void __iomem *)0x1234;
	pmx_functions[5].groups = NULL;
	pmx_functions[5].ngroups = 0;

	ret = amd_get_groups(&mock_pctrldev, 5, &groups, &num_groups);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NULL(test, groups);
	KUNIT_EXPECT_EQ(test, num_groups, 0U);
}

static int amd_get_groups_test_init(struct kunit *test)
{
	mock_gpio_dev = kunit_kzalloc(test, sizeof(*mock_gpio_dev), GFP_KERNEL);
	if (!mock_gpio_dev)
		return -ENOMEM;

	mock_gpio_dev->pdev = &mock_pdev;
	device_initialize(&mock_pdev.dev);

	return 0;
}

static struct kunit_case amd_get_groups_test_cases[] = {
	KUNIT_CASE(test_amd_get_groups_null_iomux_base),
	KUNIT_CASE(test_amd_get_groups_valid_selector),
	KUNIT_CASE(test_amd_get_groups_edge_selector),
	KUNIT_CASE(test_amd_get_groups_uninitialized_entry),
	{}
};

static struct kunit_suite amd_get_groups_test_suite = {
	.name = "amd_get_groups_test",
	.init = amd_get_groups_test_init,
	.test_cases = amd_get_groups_test_cases,
};

kunit_test_suite(amd_get_groups_test_suite);
