#include <kunit/test.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

// Mock pinctrl_dev_get_drvdata
void *set_mock_pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev);
#define pinctrl_dev_get_drvdata set_mock_pinctrl_dev_get_drvdata

#include "pinctrl-amd.c"

#define TEST_PIN_INDEX 0
static char test_mmio_buffer[4096];
static struct amd_gpio mock_gpio_dev;

void *set_mock_pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev)
{
	return &mock_gpio_dev;
}

// Debounce case (calls amd_gpio_set_debounce)
static void test_amd_pinconf_set_debounce(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	unsigned long configs[] = {
		pinconf_to_config_packed(PIN_CONFIG_INPUT_DEBOUNCE, 0x5)
	};

	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);

	// Write a dummy value to simulate initial register state
	writel(0x0, mock_gpio_dev.base + TEST_PIN_INDEX * 4);

	int ret = amd_pinconf_set(&dummy_pctldev, TEST_PIN_INDEX, configs, ARRAY_SIZE(configs));
	KUNIT_EXPECT_EQ(test, ret, 0);
}

// Pull-down case
static void test_amd_pinconf_set_pull_down(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	unsigned long configs[] = {
		pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_DOWN, 1)
	};

	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	writel(0xFFFFFFFF, mock_gpio_dev.base + TEST_PIN_INDEX * 4);

	int ret = amd_pinconf_set(&dummy_pctldev, TEST_PIN_INDEX, configs, ARRAY_SIZE(configs));
	KUNIT_EXPECT_EQ(test, ret, 0);

	u32 val = readl(mock_gpio_dev.base + TEST_PIN_INDEX * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(PULL_DOWN_ENABLE_OFF), BIT(PULL_DOWN_ENABLE_OFF));
}

// Pull-up case
static void test_amd_pinconf_set_pull_up(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	unsigned long configs[] = {
		pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1)
	};

	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	writel(0xFFFFFFFF, mock_gpio_dev.base + TEST_PIN_INDEX * 4);

	int ret = amd_pinconf_set(&dummy_pctldev, TEST_PIN_INDEX, configs, ARRAY_SIZE(configs));
	KUNIT_EXPECT_EQ(test, ret, 0);

	u32 val = readl(mock_gpio_dev.base + TEST_PIN_INDEX * 4);
	KUNIT_EXPECT_EQ(test, val & BIT(PULL_UP_ENABLE_OFF), BIT(PULL_UP_ENABLE_OFF));
}

// Drive strength case
static void test_amd_pinconf_set_drive_strength(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	unsigned long configs[] = {
		pinconf_to_config_packed(PIN_CONFIG_DRIVE_STRENGTH, 0x3)
	};

	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	writel(0x0, mock_gpio_dev.base + TEST_PIN_INDEX * 4);

	int ret = amd_pinconf_set(&dummy_pctldev, TEST_PIN_INDEX, configs, ARRAY_SIZE(configs));
	KUNIT_EXPECT_EQ(test, ret, 0);

	u32 val = readl(mock_gpio_dev.base + TEST_PIN_INDEX * 4);
	KUNIT_EXPECT_EQ(test, (val >> DRV_STRENGTH_SEL_OFF) & DRV_STRENGTH_SEL_MASK, 0x3);
}

// Invalid param case
static void test_amd_pinconf_set_invalid_param(struct kunit *test)
{
	struct pinctrl_dev dummy_pctldev;
	unsigned long configs[] = {
		pinconf_to_config_packed((enum pin_config_param)0xFFFF, 0)
	};

	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);
	mock_gpio_dev.pdev = NULL;

	int ret = amd_pinconf_set(&dummy_pctldev, TEST_PIN_INDEX, configs, ARRAY_SIZE(configs));
	KUNIT_EXPECT_EQ(test, ret, -ENOTSUPP);
}

static struct kunit_case amd_pinconf_set_test_cases[] = {
	KUNIT_CASE(test_amd_pinconf_set_debounce),
	KUNIT_CASE(test_amd_pinconf_set_pull_down),
	KUNIT_CASE(test_amd_pinconf_set_pull_up),
	KUNIT_CASE(test_amd_pinconf_set_drive_strength),
	KUNIT_CASE(test_amd_pinconf_set_invalid_param),
	{}
};

static struct kunit_suite amd_pinconf_set_test_suite = {
	.name = "amd_pinconf_set_test",
	.test_cases = amd_pinconf_set_test_cases,
};

kunit_test_suite(amd_pinconf_set_test_suite);