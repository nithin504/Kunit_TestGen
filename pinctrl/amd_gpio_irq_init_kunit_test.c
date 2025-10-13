```c
// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/io.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/spinlock.h>

#define WAKE_CNTRL_OFF_S0I3 0
#define WAKE_CNTRL_OFF_S3 1
#define WAKE_CNTRL_OFF_S4 2

struct amd_gpio {
	struct pinctrl_dev *pctrl;
	void __iomem *base;
	raw_spinlock_t lock;
};

struct pinctrl_desc {
	unsigned int npins;
	struct pinctrl_pin_desc *pins;
};

struct pin_desc {
	int number;
};

static struct pin_desc mock_pin_descs[2];
static struct pinctrl_pin_desc mock_pins[2];
static struct pinctrl_desc mock_desc;
static char mmio_buffer[4096];
static struct amd_gpio mock_gpio_dev;

static const struct pin_desc *pin_desc_get(struct pinctrl_dev *pctldev, int pin)
{
	if (pin < mock_desc.npins)
		return &mock_pin_descs[pin];
	return NULL;
}

static void amd_gpio_irq_init(struct amd_gpio *gpio_dev)
{
	const struct pinctrl_desc *desc = gpio_dev->pctrl->desc;
	unsigned long flags;
	u32 pin_reg, mask;
	int i;

	mask = BIT(WAKE_CNTRL_OFF_S0I3) | BIT(WAKE_CNTRL_OFF_S3) |
		BIT(WAKE_CNTRL_OFF_S4);

	for (i = 0; i < desc->npins; i++) {
		int pin = desc->pins[i].number;
		const struct pin_desc *pd = pin_desc_get(gpio_dev->pctrl, pin);

		if (!pd)
			continue;

		raw_spin_lock_irqsave(&gpio_dev->lock, flags);

		pin_reg = readl(gpio_dev->base + pin * 4);
		pin_reg &= ~mask;
		writel(pin_reg, gpio_dev->base + pin * 4);

		raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);
	}
}

static void test_amd_gpio_irq_init_normal_case(struct kunit *test)
{
	mock_desc.npins = 2;
	mock_desc.pins = mock_pins;
	mock_pins[0].number = 0;
	mock_pins[1].number = 1;
	mock_pin_descs[0].number = 0;
	mock_pin_descs[1].number = 1;

	mock_gpio_dev.pctrl = (struct pinctrl_dev *)&mock_desc;
	mock_gpio_dev.base = (void __iomem *)mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);

	// Initialize test registers with mask bits set
	writel(0xFFFFFFFF, mock_gpio_dev.base + 0 * 4);
	writel(0xFFFFFFFF, mock_gpio_dev.base + 1 * 4);

	amd_gpio_irq_init(&mock_gpio_dev);

	// Verify mask bits were cleared
	u32 reg0 = readl(mock_gpio_dev.base + 0 * 4);
	u32 reg1 = readl(mock_gpio_dev.base + 1 * 4);
	u32 expected_mask = BIT(WAKE_CNTRL_OFF_S0I3) | BIT(WAKE_CNTRL_OFF_S3) | BIT(WAKE_CNTRL_OFF_S4);

	KUNIT_EXPECT_EQ(test, reg0 & expected_mask, 0);
	KUNIT_EXPECT_EQ(test, reg1 & expected_mask, 0);
}

static void test_amd_gpio_irq_init_missing_pin_desc(struct kunit *test)
{
	mock_desc.npins = 2;
	mock_desc.pins = mock_pins;
	mock_pins[0].number = 0;
	mock_pins[1].number = 1;
	mock_pin_descs[0].number = 0;
	// Intentionally don't set mock_pin_descs[1] to simulate missing pin_desc

	mock_gpio_dev.pctrl = (struct pinctrl_dev *)&mock_desc;
	mock_gpio_dev.base = (void __iomem *)mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);

	// Initialize test registers with mask bits set
	writel(0xFFFFFFFF, mock_gpio_dev.base + 0 * 4);
	writel(0xFFFFFFFF, mock_gpio_dev.base + 1 * 4);

	amd_gpio_irq_init(&mock_gpio_dev);

	// Only pin 0 should have been processed (pin 1 has no pin_desc)
	u32 reg0 = readl(mock_gpio_dev.base + 0 * 4);
	u32 reg1 = readl(mock_gpio_dev.base + 1 * 4);
	u32 expected_mask = BIT(WAKE_CNTRL_OFF_S0I3) | BIT(WAKE_CNTRL_OFF_S3) | BIT(WAKE_CNTRL_OFF_S4);

	KUNIT_EXPECT_EQ(test, reg0 & expected_mask, 0);
	KUNIT_EXPECT_EQ(test, reg1 & expected_mask, expected_mask); // Should remain unchanged
}

static void test_amd_gpio_irq_init_single_pin(struct kunit *test)
{
	mock_desc.npins = 1;
	mock_desc.pins = mock_pins;
	mock_pins[0].number = 5;
	mock_pin_descs[0].number = 5;

	mock_gpio_dev.pctrl = (struct pinctrl_dev *)&mock_desc;
	mock_gpio_dev.base = (void __iomem *)mmio_buffer;
	mock_gpio_dev.lock = __RAW_SPIN_LOCK_UNLOCKED(mock_gpio_dev.lock);

	// Initialize test register with mask bits set
	writel(0xFFFFFFFF, mock_gpio_dev.base + 5 * 4);

	amd_gpio_irq_init(&mock_gpio_dev);

	// Verify mask bits were cleared for pin 5
	u32 reg = readl(mock_gpio_dev.base + 5 * 4);
	u32 expected_mask = BIT(WAKE_CNTRL_OFF_S0I3) | BIT(WAKE_CNTRL_OFF_S3) | BIT(WAKE_CNTRL_OFF_S4);

	KUNIT_EXPECT_EQ(test, reg & expected_mask, 0);
}

static struct kunit_case amd_gpio_irq_init_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_irq_init_normal_case),
	KUNIT_CASE(test_amd_gpio_irq_init_missing_pin_desc),
	KUNIT_CASE(test_amd_gpio_irq_init_single_pin),
	{}
};

static struct kunit_suite amd_gpio_irq_init_test_suite = {
	.name = "amd_gpio_irq_init_test",
	.test_cases = amd_gpio_irq_init_test_cases,
};

kunit_test_suite(amd_gpio_irq_init_test_suite);
```