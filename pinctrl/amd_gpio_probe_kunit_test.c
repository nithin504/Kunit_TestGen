
// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/gpio/driver.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/acpi.h>
#include <linux/spinlock.h>
#include <linux/err.h>

// Mock functions and variables needed for the probe function
static int mock_platform_get_irq_calls = 0;
static int mock_devm_kzalloc_calls = 0;
static int mock_devm_platform_get_and_ioremap_resource_calls = 0;
static int mock_devm_pinctrl_register_calls = 0;
static int mock_gpiochip_add_data_calls = 0;
static int mock_gpiochip_add_pin_range_calls = 0;
static int mock_devm_request_irq_calls = 0;
static int mock_acpi_register_wakeup_handler_calls = 0;
static int mock_amd_gpio_register_s2idle_ops_calls = 0;

static char mock_mmio_buffer[8192];
static struct resource mock_res = {
	.start = 0x1000,
	.end = 0x1FFF,
};

// Mock platform_device
static struct platform_device mock_pdev = {
	.name = "test-amd-gpio",
};

// Mock pinctrl_desc
static struct pinctrl_desc amd_pinctrl_desc = {
	.npins = 64,
};

// Mock groups
static const struct group_desc kerncz_groups[] = {
	{ .name = "test_group" },
};

// Forward declarations of functions that will be called
static int amd_gpio_get_direction(struct gpio_chip *gc, unsigned int offset) { return 0; }
static int amd_gpio_direction_input(struct gpio_chip *gc, unsigned int offset) { return 0; }
static int amd_gpio_direction_output(struct gpio_chip *gc, unsigned int offset, int value) { return 0; }
static int amd_gpio_get_value(struct gpio_chip *gc, unsigned int offset) { return 0; }
static void amd_gpio_set_value(struct gpio_chip *gc, unsigned int offset, int value) {}
static int amd_gpio_set_config(struct gpio_chip *gc, unsigned int offset, unsigned long config) { return 0; }
static void amd_gpio_dbg_show(struct seq_file *s, struct gpio_chip *gc) {}
static void amd_gpio_irq_init(struct amd_gpio *gpio_dev) {}
static int amd_gpio_irq_handler(int irq, void *dev_id) { return 0; }
static bool amd_gpio_check_wake(int irq, void *dev_id) { return false; }
static void amd_get_iomux_res(struct amd_gpio *gpio_dev) {}
static void amd_gpio_register_s2idle_ops(void) { mock_amd_gpio_register_s2idle_ops_calls++; }

// Mock IRQ chip
static struct irq_chip amd_gpio_irqchip = {
	.name = "test-amd-irqchip",
};

// Mock devm_kzalloc
void *devm_kzalloc(const struct device *dev, size_t size, gfp_t gfp)
{
	mock_devm_kzalloc_calls++;
	return kunit_kzalloc(NULL, size, gfp);
}

// Mock devm_platform_get_and_ioremap_resource
void __iomem *devm_platform_get_and_ioremap_resource(struct platform_device *pdev,
						     unsigned int index,
						     struct resource **res)
{
	mock_devm_platform_get_and_ioremap_resource_calls++;
	if (res)
		*res = &mock_res;
	return (void __iomem *)mock_mmio_buffer;
}

// Mock platform_get_irq
int platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	mock_platform_get_irq_calls++;
	return 16; // Return a valid IRQ number
}

// Mock devm_pinctrl_register
struct pinctrl_dev *devm_pinctrl_register(struct device *dev,
					  const struct pinctrl_desc *pctldesc,
					  void *driver_data)
{
	mock_devm_pinctrl_register_calls++;
	return (struct pinctrl_dev *)driver_data;
}

// Mock gpiochip_add_data
int gpiochip_add_data(struct gpio_chip *gc, void *data)
{
	mock_gpiochip_add_data_calls++;
	return 0;
}

// Mock gpiochip_add_pin_range
int gpiochip_add_pin_range(struct gpio_chip *gc, const char *pinctl_name,
			   unsigned int gpio_offset, unsigned int pin_offset,
			   unsigned int npins)
{
	mock_gpiochip_add_pin_range_calls++;
	return 0;
}

// Mock devm_request_irq
int devm_request_irq(struct device *dev, int irq, irq_handler_t handler,
		     unsigned long flags, const char *name, void *dev_id)
{
	mock_devm_request_irq_calls++;
	return 0;
}

// Mock acpi_register_wakeup_handler
int acpi_register_wakeup_handler(int gpe, bool (*handler)(int, void *), void *context)
{
	mock_acpi_register_wakeup_handler_calls++;
	return 0;
}

// Mock platform_set_drvdata
void platform_set_drvdata(struct platform_device *pdev, void *data) {}

// Mock gpiochip_remove
void gpiochip_remove(struct gpio_chip *gc) {}

// Mock dev_err
void dev_err(const struct device *dev, const char *fmt, ...) {}

// Mock dev_dbg
void dev_dbg(const struct device *dev, const char *fmt, ...) {}

// Include the actual function to test
static int amd_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct amd_gpio *gpio_dev;
	struct gpio_irq_chip *girq;

	gpio_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct amd_gpio), GFP_KERNEL);
	if (!gpio_dev)
		return -ENOMEM;

	raw_spin_lock_init(&gpio_dev->lock);

	gpio_dev->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(gpio_dev->base)) {
		dev_err(&pdev->dev, "Failed to get gpio io resource.\n");
		return PTR_ERR(gpio_dev->base);
	}

	gpio_dev->irq = platform_get_irq(pdev, 0);
	if (gpio_dev->irq < 0)
		return gpio_dev->irq;

#ifdef CONFIG_SUSPEND
	gpio_dev->saved_regs = devm_kcalloc(&pdev->dev, amd_pinctrl_desc.npins,
					    sizeof(*gpio_dev->saved_regs),
					    GFP_KERNEL);
	if (!gpio_dev->saved_regs)
		return -ENOMEM;
#endif

	gpio_dev->pdev = pdev;
	gpio_dev->gc.get_direction	= amd_gpio_get_direction;
	gpio_dev->gc.direction_input	= amd_gpio_direction_input;
	gpio_dev->gc.direction_output	= amd_gpio_direction_output;
	gpio_dev->gc.get			= amd_gpio_get_value;
	gpio_dev->gc.set			= amd_gpio_set_value;
	gpio_dev->gc.set_config		= amd_gpio_set_config;
	gpio_dev->gc.dbg_show		= amd_gpio_dbg_show;

	gpio_dev->gc.base		= -1;
	gpio_dev->gc.label			= pdev->name;
	gpio_dev->gc.owner			= THIS_MODULE;
	gpio_dev->gc.parent			= &pdev->dev;
	gpio_dev->gc.ngpio			= resource_size(res) / 4;

	gpio_dev->hwbank_num = gpio_dev->gc.ngpio / 64;
	gpio_dev->groups = kerncz_groups;
	gpio_dev->ngroups = ARRAY_SIZE(kerncz_groups);

	amd_pinctrl_desc.name = dev_name(&pdev->dev);
	amd_get_iomux_res(gpio_dev);
	gpio_dev->pctrl = devm_pinctrl_register(&pdev->dev, &amd_pinctrl_desc,
						gpio_dev);
	if (IS_ERR(gpio_dev->pctrl)) {
		dev_err(&pdev->dev, "Couldn't register pinctrl driver\n");
		return PTR_ERR(gpio_dev->pctrl);
	}

	/* Disable and mask interrupts */
	amd_gpio_irq_init(gpio_dev);

	girq = &gpio_dev->gc.irq;
	gpio_irq_chip_set_chip(girq, &amd_gpio_irqchip);
	/* This will let us handle the parent IRQ in the driver */
	girq->parent_handler = NULL;
	girq->num_parents = 0;
	girq->parents = NULL;
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_simple_irq;

	ret = gpiochip_add_data(&gpio_dev->gc, gpio_dev);
	if (ret)
		return ret;

	ret = gpiochip_add_pin_range(&gpio_dev->gc, dev_name(&pdev->dev),
				0, 0, gpio_dev->gc.ngpio);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add pin range\n");
		goto out2;
	}

	ret = devm_request_irq(&pdev->dev, gpio_dev->irq, amd_gpio_irq_handler,
			       IRQF_SHARED | IRQF_COND_ONESHOT, KBUILD_MODNAME, gpio_dev);
	if (ret)
		goto out2;

	platform_set_drvdata(pdev, gpio_dev);
	acpi_register_wakeup_handler(gpio_dev->irq, amd_gpio_check_wake, gpio_dev);
	amd_gpio_register_s2idle_ops();

	dev_dbg(&pdev->dev, "amd gpio driver loaded\n");
	return ret;

out2:
	gpiochip_remove(&gpio_dev->gc);

	return ret;
}

// Test cases
static void test_amd_gpio_probe_success(struct kunit *test)
{
	int ret;
	
	// Reset mock counters
	mock_devm_kzalloc_calls = 0;
	mock_devm_platform_get_and_ioremap_resource_calls = 0;
	mock_platform_get_irq_calls = 0;
	mock_devm_pinctrl_register_calls = 0;
	mock_gpiochip_add_data_calls = 0;
	mock_gpiochip_add_pin_range_calls = 0;
	mock_devm_request_irq_calls = 0;
	mock_acpi_register_wakeup_handler_calls = 0;
	mock_amd_gpio_register_s2idle_ops_calls = 0;

	ret = amd_gpio_probe(&mock_pdev);
	
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, mock_devm_kzalloc_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_devm_platform_get_and_ioremap_resource_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_platform_get_irq_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_devm_pinctrl_register_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_gpiochip_add_data_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_gpiochip_add_pin_range_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_devm_request_irq_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_acpi_register_wakeup_handler_calls, 1);
	KUNIT_EXPECT_EQ(test, mock_amd_gpio_register_s2idle_ops_calls, 1);
}

static void test_amd_gpio_probe_memory_allocation_failure(struct kunit *test)
{
	int ret;
	int original_mock_count = mock_devm_kzalloc_calls;
	
	// Force devm_kzalloc to return NULL
	void *(*original_devm_kzalloc)(const struct device *, size_t, gfp_t) = devm_kzalloc;
	devm_kzalloc = NULL;
	
	ret = amd_gpio_probe(&mock_pdev);
	
	// Restore original function
	devm_kzalloc = original_devm_kzalloc;
	
	KUNIT_EXPECT_EQ(test, ret, -ENOMEM);
}

static void test_amd_gpio_probe_ioremap_failure(struct kunit *test)
{
	int ret;
	
	// Force devm_platform_get_and_ioremap_resource to return error
	void __iomem *(*original_devm_platform_get_and_ioremap_resource)(struct platform_device *, unsigned int, struct resource **) = devm_platform_get_and_ioremap_resource;
	devm_platform_get_and_ioremap_resource = NULL;
	
	ret = amd_gpio_probe(&mock_pdev);
	
	// Restore original function
	devm_platform_get_and_ioremap_resource = original_devm_platform_get_and_ioremap_resource;
	
	KUNIT_EXPECT_TRUE(test, IS_ERR_VALUE(ret));
}

static void test_amd_gpio_probe_irq_failure(struct kunit *test)
{
	int ret;
	
	// Force platform_get_irq to return error
	int (*original_platform_get_irq)(struct platform_device *, unsigned int) = platform_get_irq;
	platform_get_irq = NULL;
	
	ret = amd_gpio_probe(&mock_pdev);
	
	// Restore original function
	platform_get_irq = original_platform_get_irq;
	
	KUNIT_EXPECT_LT(test, ret, 0);
}

static struct kunit_case amd_gpio_probe_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_probe_success),
	KUNIT_CASE(test_amd_gpio_probe_memory_allocation_failure),
	KUNIT_CASE(test_amd_gpio_probe_ioremap_failure),
	KUNIT_CASE(test_amd_gpio_probe_irq_failure),
	{}
};

static struct kunit_suite amd_gpio_probe_test_suite = {
	.name = "amd_gpio_probe_test",
	.test_cases = amd_gpio_probe_test_cases,
};

kunit_test_suite(amd_gpio_probe_test_suite);