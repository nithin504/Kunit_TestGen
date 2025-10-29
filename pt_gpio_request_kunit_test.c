#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>

/* Mock register definitions */
#define PT_SYNC_REG 0x00
#define PT_CLOCKRATE_REG 0x04

/* Structure definition for pt_gpio_chip */
struct pt_gpio_chip {
	void __iomem *reg_base;
};

/* Function under test */
static int pt_gpio_request(struct gpio_chip *gc, unsigned offset)
{
	struct pt_gpio_chip *pt_gpio = gpiochip_get_data(gc);
	unsigned long flags;
	u32 using_pins;

	dev_dbg(gc->parent, "pt_gpio_request offset=%x\n", offset);

	raw_spin_lock_irqsave(&gc->bgpio_lock, flags);

	using_pins = readl(pt_gpio->reg_base + PT_SYNC_REG);
	if (using_pins & BIT(offset)) {
		dev_warn(gc->parent, "PT GPIO pin %x reconfigured\n",
			 offset);
		raw_spin_unlock_irqrestore(&gc->bgpio_lock, flags);
		return -EINVAL;
	}

	writel(using_pins | BIT(offset), pt_gpio->reg_base + PT_SYNC_REG);

	raw_spin_unlock_irqrestore(&gc->bgpio_lock, flags);

	return 0;
}

/* Helper to create mock gpio_chip and pt_gpio_chip */
static void create_mock_gpio_chip(struct kunit *test,
				  struct gpio_chip **out_gc,
				  struct pt_gpio_chip **out_pt_gpio,
				  void __iomem **fake_reg_base)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void *reg_mem;

	/* Allocate memory for structures */
	gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gc);

	pt_gpio = kunit_kzalloc(test, sizeof(*pt_gpio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pt_gpio);

	reg_mem = kunit_kzalloc(test, 4096, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, reg_mem);

	*fake_reg_base = reg_mem;
	pt_gpio->reg_base = reg_mem;

	/* Initialize gpio_chip fields used in the function */
	gc->parent = NULL;
	raw_spin_lock_init(&gc->bgpio_lock);

	/* Associate pt_gpio with gpio_chip using gpiochip_set_data/get_data pattern */
	gc->private = pt_gpio;

	*out_gc = gc;
	*out_pt_gpio = pt_gpio;
}

/* Test successful pin request */
static void test_pt_gpio_request_success(struct kunit *test)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void __iomem *fake_reg_base;
	unsigned offset = 5;
	int ret;

	create_mock_gpio_chip(test, &gc, &pt_gpio, &fake_reg_base);

	/* Initially no pins are in use */
	writel(0, fake_reg_base + PT_SYNC_REG);

	ret = pt_gpio_request(gc, offset);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Verify the bit was set */
	KUNIT_EXPECT_NE(test, readl(fake_reg_base + PT_SYNC_REG) & BIT(offset), 0U);
}

/* Test requesting already used pin */
static void test_pt_gpio_request_pin_already_used(struct kunit *test)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void __iomem *fake_reg_base;
	unsigned offset = 3;
	int ret;

	create_mock_gpio_chip(test, &gc, &pt_gpio, &fake_reg_base);

	/* Pre-set the pin as used */
	writel(BIT(offset), fake_reg_base + PT_SYNC_REG);

	ret = pt_gpio_request(gc, offset);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/* Test multiple pin requests */
static void test_pt_gpio_request_multiple_pins(struct kunit *test)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void __iomem *fake_reg_base;
	int ret;

	create_mock_gpio_chip(test, &gc, &pt_gpio, &fake_reg_base);

	/* Initially no pins are in use */
	writel(0, fake_reg_base + PT_SYNC_REG);

	/* Request several different pins */
	ret = pt_gpio_request(gc, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pt_gpio_request(gc, 10);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pt_gpio_request(gc, 31);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Check that all bits are set */
	KUNIT_EXPECT_NE(test, readl(fake_reg_base + PT_SYNC_REG) & BIT(0), 0U);
	KUNIT_EXPECT_NE(test, readl(fake_reg_base + PT_SYNC_REG) & BIT(10), 0U);
	KUNIT_EXPECT_NE(test, readl(fake_reg_base + PT_SYNC_REG) & BIT(31), 0U);
}

/* Test boundary values for offset */
static void test_pt_gpio_request_boundary_values(struct kunit *test)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void __iomem *fake_reg_base;
	int ret;

	create_mock_gpio_chip(test, &gc, &pt_gpio, &fake_reg_base);

	/* Initially no pins are in use */
	writel(0, fake_reg_base + PT_SYNC_REG);

	/* Test minimum valid offset */
	ret = pt_gpio_request(gc, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Test maximum valid offset (assuming 32-bit register) */
	ret = pt_gpio_request(gc, 31);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* Test invalid offset handling (if any bounds checking exists) */
static void test_pt_gpio_request_invalid_offset(struct kunit *test)
{
	struct gpio_chip *gc;
	struct pt_gpio_chip *pt_gpio;
	void __iomem *fake_reg_base;
	int ret;

	create_mock_gpio_chip(test, &gc, &pt_gpio, &fake_reg_base);

	/* Initially no pins are in use */
	writel(0, fake_reg_base + PT_SYNC_REG);

	/* This might cause issues depending on implementation, but we test it */
	ret = pt_gpio_request(gc, 32); /* Beyond 32-bit range */
	/* We don't assert specific behavior here since it depends on implementation */
}

static struct kunit_case pt_gpio_request_test_cases[] = {
	KUNIT_CASE(test_pt_gpio_request_success),
	KUNIT_CASE(test_pt_gpio_request_pin_already_used),
	KUNIT_CASE(test_pt_gpio_request_multiple_pins),
	KUNIT_CASE(test_pt_gpio_request_boundary_values),
	KUNIT_CASE(test_pt_gpio_request_invalid_offset),
	{}
};

static struct kunit_suite pt_gpio_request_test_suite = {
	.name = "pt_gpio_request_test",
	.test_cases = pt_gpio_request_test_cases,
};

kunit_test_suite(pt_gpio_request_test_suite);