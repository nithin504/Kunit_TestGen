// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#define PT_SYNC_REG 0x28

/* Mock struct representing driver-specific GPIO context */
struct pt_gpio_chip {
	void __iomem *reg_base;
};

/* Function under test (simulated version for KUnit) */
static void pt_gpio_free(struct gpio_chip *gc, unsigned int offset)
{
	struct pt_gpio_chip *pt_gpio = gpiochip_get_data(gc);
	unsigned long flags;
	u32 using_pins;

	spin_lock_irqsave(&gc->bgpio_lock, flags);

	using_pins = readl(pt_gpio->reg_base + PT_SYNC_REG);
	using_pins &= ~BIT(offset);
	writel(using_pins, pt_gpio->reg_base + PT_SYNC_REG);

	spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	dev_dbg(gc->parent, "pt_gpio_free offset=%u\n", offset);
}

/* Mocked gpio_chip allocator */
static struct gpio_chip *create_mock_gpio_chip(struct kunit *test, void __iomem *reg_base)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;

	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
	pt_gpio = kunit_kzalloc(test, sizeof(*pt_gpio), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gc);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pt_gpio);

	pt_gpio->reg_base = reg_base;

	/*
	 * Modern gpio_chip uses a private data pointer through
	 * gpiochip_set_data() / gpiochip_get_data(). We mock this behavior
	 * here by directly assigning bgpio_data if available, else using
	 * private pointer.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
	gpiochip_set_data(gc, pt_gpio);
#else
	gc->data = pt_gpio;
#endif

	spin_lock_init(&gc->bgpio_lock);

	return gc;
}

/* ---------- Test Cases ---------- */

static void test_pt_gpio_free_single_pin(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 5;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0xFFFFFFDF; /* BIT(5) cleared */

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	writel(initial_value, fake_reg_base + PT_SYNC_REG);
	pt_gpio_free(gc, offset);

	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_first_pin(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 0;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0xFFFFFFFE;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	writel(initial_value, fake_reg_base + PT_SYNC_REG);
	pt_gpio_free(gc, offset);

	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_last_pin_u32(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 31;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0x7FFFFFFF;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	writel(initial_value, fake_reg_base + PT_SYNC_REG);
	pt_gpio_free(gc, offset);

	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_on_cleared_pin(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 10;
	u32 initial_value = 0xFFFFF000;
	u32 expected_value = 0xFFFFF000; /* Already cleared */

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	writel(initial_value, fake_reg_base + PT_SYNC_REG);
	pt_gpio_free(gc, offset);

	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_all_bits(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	for (unsigned int i = 0; i < 32; i++)
		pt_gpio_free(gc, i);

	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

/* ---------- Suite Definition ---------- */

static struct kunit_case pt_gpio_free_test_cases[] = {
	KUNIT_CASE(test_pt_gpio_free_single_pin),
	KUNIT_CASE(test_pt_gpio_free_first_pin),
	KUNIT_CASE(test_pt_gpio_free_last_pin_u32),
	KUNIT_CASE(test_pt_gpio_free_on_cleared_pin),
	KUNIT_CASE(test_pt_gpio_free_all_bits),
	{}
};

static struct kunit_suite pt_gpio_free_test_suite = {
	.name = "pt_gpio_free",
	.test_cases = pt_gpio_free_test_cases,
};

kunit_test_suite(pt_gpio_free_test_suite);

MODULE_LICENSE("GPL");
