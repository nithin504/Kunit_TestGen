#include <kunit/test.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define PT_SYNC_REG       0x00
#define PT_CLOCKRATE_REG  0x04

static void pt_gpio_probe_test(struct kunit *test)
{
    struct platform_device *pdev;
    struct pt_gpio *pt_gpio;
    void __iomem *fake_reg_base;

    // Allocate dummy platform device
    pdev = kunit_alloc_platform_device(test);
    KUNIT_ASSERT_NOT_NULL(test, pdev);

    // Allocate pt_gpio and fake reg_base
    pt_gpio = kzalloc(sizeof(*pt_gpio), GFP_KERNEL);
    KUNIT_ASSERT_NOT_NULL(test, pt_gpio);

    // Allocate fake memory-mapped I/O region
    fake_reg_base = kzalloc(8, GFP_KERNEL); // Enough for 2 registers
    KUNIT_ASSERT_NOT_NULL(test, fake_reg_base);

    pt_gpio->reg_base = fake_reg_base;

    // Simulate platform_set_drvdata
    platform_set_drvdata(pdev, pt_gpio);

    // Simulate register writes
    writel(0, pt_gpio->reg_base + PT_SYNC_REG);
    writel(0, pt_gpio->reg_base + PT_CLOCKRATE_REG);

    // Validate register values
    KUNIT_EXPECT_EQ(test, readl(pt_gpio->reg_base + PT_SYNC_REG), 0u);
    KUNIT_EXPECT_EQ(test, readl(pt_gpio->reg_base + PT_CLOCKRATE_REG), 0u);

    // Cleanup
    kfree(fake_reg_base);
    kfree(pt_gpio);
}
static struct kunit_case pt_gpio_test_cases[] = {
    KUNIT_CASE(pt_gpio_probe_test),
    {}
};

static struct kunit_suite pt_gpio_test_suite = {
    .name = "pt-gpio-test",
    .test_cases = pt_gpio_test_cases,
};

kunit_test_suite(pt_gpio_test_suite);
