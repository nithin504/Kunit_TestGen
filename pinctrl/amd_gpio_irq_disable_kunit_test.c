// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/irq.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>
#include <linux/io.h>

// Mock structures and variables
static struct amd_gpio mock_gpio_dev;
static char test_mmio_buffer[4096];
static raw_spinlock_t mock_lock;

// Mock functions
struct gpio_chip *irq_data_get_irq_chip_data(struct irq_data *d)
{
	return (struct gpio_chip *)d->hwirq;
}

struct amd_gpio *gpiochip_get_data(struct gpio_chip *gc)
{
	return &mock_gpio_dev;
}

void gpiochip_disable_irq(struct gpio_chip *gc, unsigned int offset)
{
	// Mock implementation
}

// Include the original function to test
static void amd_gpio_irq_disable(struct irq_data *d)
{
	u32 pin_reg;
	unsigned long flags;
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct amd_gpio *gpio_dev = gpiochip_get_data(gc);

	raw_spin_lock_irqsave(&gpio_dev->lock, flags);
	pin_reg = readl(gpio_dev->base + (d->hwirq)*4);
	pin_reg &= ~BIT(INTERRUPT_ENABLE_OFF);
	pin_reg &= ~BIT(INTERRUPT_MASK_OFF);
	writel(pin_reg, gpio_dev->base + (d->hwirq)*4);
	raw_spin_unlock_irqrestore(&gpio_dev->lock, flags);

	gpiochip_disable_irq(gc, d->hwirq);
}

// Test cases
static void test_amd_gpio_irq_disable_basic(struct kunit *test)
{
	struct irq_data test_irq_data;
	
	// Initialize test data
	test_irq_data.hwirq = 0;
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = mock_lock;
	
	// Initialize MMIO buffer with some value
	writel(0xFFFFFFFF, mock_gpio_dev.base);
	
	// Call the function under test
	amd_gpio_irq_disable(&test_irq_data);
	
	// Verify the register was modified correctly
	u32 result = readl(mock_gpio_dev.base);
	KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_ENABLE_OFF), 0);
	KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_MASK_OFF), 0);
}

static void test_amd_gpio_irq_disable_multiple_offsets(struct kunit *test)
{
	struct irq_data test_irq_data;
	
	// Test with different hardware IRQ offsets
	for (unsigned int offset = 0; offset < 4; offset++) {
		test_irq_data.hwirq = offset;
		mock_gpio_dev.base = test_mmio_buffer;
		mock_gpio_dev.lock = mock_lock;
		
		// Initialize MMIO buffer for this offset
		writel(0xFFFFFFFF, mock_gpio_dev.base + offset * 4);
		
		// Call the function under test
		amd_gpio_irq_disable(&test_irq_data);
		
		// Verify the register was modified correctly
		u32 result = readl(mock_gpio_dev.base + offset * 4);
		KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_ENABLE_OFF), 0);
		KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_MASK_OFF), 0);
	}
}

static void test_amd_gpio_irq_disable_already_disabled(struct kunit *test)
{
	struct irq_data test_irq_data;
	
	// Initialize test data
	test_irq_data.hwirq = 0;
	mock_gpio_dev.base = test_mmio_buffer;
	mock_gpio_dev.lock = mock_lock;
	
	// Initialize MMIO buffer with interrupts already disabled
	writel(0x00000000, mock_gpio_dev.base);
	
	// Call the function under test
	amd_gpio_irq_disable(&test_irq_data);
	
	// Verify the register remains unchanged
	u32 result = readl(mock_gpio_dev.base);
	KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_ENABLE_OFF), 0);
	KUNIT_EXPECT_EQ(test, result & BIT(INTERRUPT_MASK_OFF), 0);
}

static struct kunit_case amd_gpio_irq_disable_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_irq_disable_basic),
	KUNIT_CASE(test_amd_gpio_irq_disable_multiple_offsets),
	KUNIT_CASE(test_amd_gpio_irq_disable_already_disabled),
	{}
};

static struct kunit_suite amd_gpio_irq_disable_test_suite = {
	.name = "amd_gpio_irq_disable_test",
	.test_cases = amd_gpio_irq_disable_test_cases,
};

kunit_test_suite(amd_gpio_irq_disable_test_suite);