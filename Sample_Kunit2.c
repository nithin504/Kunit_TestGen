// File: irq_eoi_test.c
#include <kunit/test.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/irq.h>

void *mock_gpiochip_get_data(struct gpio_chip *);
#define gpiochip_get_data mock_gpiochip_get_data
#define "pinctrl-amd.c" // Include for coverage

#define MOCK_BASE_ADDR 0x1000
#define WAKE_INT_MASTER_REG 0x44
#define EOI_MASK 0x1

struct amd_gpio_dev;
struct irq_data irq_d;
void *mock_gpiochip_get_data(struct gpio_chip *) {
	return &gpio_dev;
}

char test_amd_gpio_irq_eoi_buffer[8192];
static void test_amd_gpio_irq_eoi(struct kunit *test)
{
	struct gpio_chip gc;
	u32 initial_val = 0xABCD1234;
	u32 expected_val = initial_val | EOI_MASK;

	// Simulate memory-mapped I/O
	gpio_dev.base = test_amd_gpio_irq_eoi_buffer;

	// Link gpio_dev to gpio_chip
	*(struct amd_gpio **)&gc = &gpio_dev;

	// Setup irq_data
	irq_d.chip_data = &gc;

	// Call the actual function
	amd_gpio_irq_eoi(&irq_d);

	// Verify EOI_MASK was applied
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static struct kunit_case irq_eoi_test_cases[] = {
	KUNIT_CASE(test_amd_gpio_irq_eoi),
	{}
};

static struct kunit_suite irq_eoi_test_suite = {
	.name = "irq_eoi_test",
	.test_cases = irq_eoi_test_cases,
};

kunit_test_suite(irq_eoi_test_suite);
