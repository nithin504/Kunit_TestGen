#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>

/* Mocked definitions based on observed usage */
#define PT_SYNC_REG 0x28

struct pt_gpio_chip {
	void __iomem *reg_base;
};

/* Function under test */
static void pt_gpio_free(struct gpio_chip *gc, unsigned offset)
{
	struct pt_gpio_chip *pt_gpio = gpiochip_get_data(gc);
	unsigned long flags;
	u32 using_pins;

	raw_spin_lock_irqsave(&gc->bgpio_lock, flags);

	using_pins = readl(pt_gpio->reg_base + PT_SYNC_REG);
	using_pins &= ~BIT(offset);
	writel(using_pins, pt_gpio->reg_base + PT_SYNC_REG);

	raw_spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	dev_dbg(gc->parent, "pt_gpio_free offset=%x\n", offset);
}

/* Test helpers */
static struct gpio_chip *create_mock_gpio_chip(struct kunit *test, void __iomem *reg_base)
{
	struct gpio_chip *gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
	struct pt_gpio_chip *pt_gpio = kunit_kzalloc(test, sizeof(*pt_gpio), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gc);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pt_gpio);

	pt_gpio->reg_base = reg_base;
	gc->bgpio_data = pt_gpio; /* Use bgpio_data instead of private */

	/* Initialize spinlock */
	spin_lock_init(&gc->bgpio_lock);

	return gc;
}

/* Test Cases */
static void test_pt_gpio_free_single_pin(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 5;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0xFFFFFFDF; /* BIT(5) cleared */

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	/* Setup initial register state */
	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	/* Call function under test */
	pt_gpio_free(gc, offset);

	/* Verify result */
	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_first_pin(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 0;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0xFFFFFFFE; /* BIT(0) cleared */

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	/* Setup initial register state */
	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	/* Call function under test */
	pt_gpio_free(gc, offset);

	/* Verify result */
	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_last_pin_u32(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	unsigned int offset = 31;
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0x7FFFFFFF; /* BIT(31) cleared */

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	/* Setup initial register state */
	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	/* Call function under test */
	pt_gpio_free(gc, offset);

	/* Verify result */
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

	/* Setup initial register state */
	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	/* Call function under test */
	pt_gpio_free(gc, offset);

	/* Verify result remains unchanged */
	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

static void test_pt_gpio_free_all_bits(struct kunit *test)
{
	void __iomem *fake_reg_base = kunit_kzalloc(test, 4096, GFP_KERNEL);
	struct gpio_chip *gc = create_mock_gpio_chip(test, fake_reg_base);
	u32 initial_value = 0xFFFFFFFF;
	u32 expected_value = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_reg_base);

	/* Setup initial register state */
	writel(initial_value, fake_reg_base + PT_SYNC_REG);

	/* Free all pins one by one */
	for (unsigned i = 0; i < 32; i++) {
		pt_gpio_free(gc, i);
	}

	/* Verify final result */
	KUNIT_EXPECT_EQ(test, readl(fake_reg_base + PT_SYNC_REG), expected_value);
}

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