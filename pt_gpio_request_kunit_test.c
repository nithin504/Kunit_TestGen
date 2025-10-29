#include <kunit/test.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include "gpio-amdpt.c"
// Mock register definitions
#define PTSYNCREG 0x00
#define PTCLOCKRATEREG 0x04

// Structure definition for ptgpiochip
struct ptgpiochip {
    void __iomem *reg_base;
};

// Function under test
static int ptgpio_request(struct gpio_chip *gc, unsigned offset)
{
    struct ptgpiochip *ptgpio = gpiochip_get_data(gc);
    unsigned long flags;
    u32 using_pins;

    // Acquire lock
    raw_spin_lock_irqsave(&gc->bgpio_lock, flags);

    using_pins = readl(ptgpio->reg_base + PTSYNCREG);

    if (using_pins & BIT(offset)) {
        raw_spin_unlock_irqrestore(&gc->bgpio_lock, flags);
        return -EINVAL;
    }

    writel(using_pins | BIT(offset), ptgpio->reg_base + PTSYNCREG);

    raw_spin_unlock_irqrestore(&gc->bgpio_lock, flags);
    return 0;
}

// Helper to create mock gpiochip and ptgpiochip
static void create_mock_gpiochip(struct kunit *test,
                                struct gpio_chip **out_gc,
                                struct ptgpiochip **out_ptgpio,
                                void __iomem **out_fakeregbase)
{
    struct gpio_chip *gc;
    struct ptgpiochip *ptgpio;
    void *regmem;

    gc = kunit_kzalloc(test, sizeof(*gc), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gc);

    ptgpio = kunit_kzalloc(test, sizeof(*ptgpio), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ptgpio);

    regmem = kunit_kzalloc(test, 4096, GFP_KERNEL); // Fake register memory
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmem);

    *out_fakeregbase = regmem;
    ptgpio->reg_base = regmem;

    gc->parent = NULL;
    raw_spin_lock_init(&gc->bgpio_lock);

    gpiochip_set_data(gc, ptgpio);

    *out_gc = gc;
    *out_ptgpio = ptgpio;
}

// Test successful pin request
static void test_ptgpio_request_success(struct kunit *test)
{
    struct gpio_chip *gc;
    struct ptgpiochip *ptgpio;
    void __iomem *fakeregbase;
    unsigned offset = 5;
    int ret;

    create_mock_gpiochip(test, &gc, &ptgpio, &fakeregbase);
    writel(0, fakeregbase + PTSYNCREG);

    ret = ptgpio_request(gc, offset);

    KUNIT_EXPECT_EQ(test, ret, 0);
    KUNIT_EXPECT_NE(test, readl(fakeregbase + PTSYNCREG) & BIT(offset), 0U);
}

// Additional tests (already present in your file) should be similarly updated

static struct kunit_case ptgpio_request_test_cases[] = {
    KUNIT_CASE(test_ptgpio_request_success),
    // Add other test cases here
};

static struct kunit_suite ptgpio_request_test_suite = {
    .name = "ptgpio_request_test",
    .test_cases = ptgpio_request_test_cases,
};

kunit_test_suite(ptgpio_request_test_suite);
