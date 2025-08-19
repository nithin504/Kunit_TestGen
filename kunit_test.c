// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/serial_8250.h>
struct uart_port;
static int serial8250_clear_and_reinit_fifos_called = 0;
static int serial_port_in_called = 0;
static int test_fcr_enable_fifo = 0;
static int test_fcr_enable_fifo_lsr = 0;
int serial_port_in_mock(struct uart_port *p, int offset);
#define serial8250_clear_and_reinit_fifos(...) serial8250_clear_and_reinit_fifos_mock(__VA_ARGS__)
#define serial_port_in(...)	serial_port_in_mock(__VA_ARGS__)
#include "8250_dw.c"

static int trial_count=0;
static char buffer[16384];
int serial_port_in_mock(struct uart_port *p, int offset)
{
	printk("%s %d",__FILE__,__LINE__);
	serial_port_in_called++;
	trial_count++;
	if (test_fcr_enable_fifo_lsr)
		return UART_LSR;
	if (trial_count > 5)
		return ~UART_LCR_SPAR;
	return 0;
}
void serial8250_clear_and_reinit_fifos_mock(struct uart_8250_port *p)
{
	serial8250_clear_and_reinit_fifos_called++;
	if (test_fcr_enable_fifo)
		p->fcr |= UART_FCR_ENABLE_FIFO;
}

static void dw8250_fallback_dma_filter_test(struct kunit *test)
{
	int ret = dw8250_fallback_dma_filter(0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void dw8250_reset_control_assert_test(struct kunit *test)
{
	dw8250_reset_control_assert(&buffer);
	KUNIT_EXPECT_EQ(test, 0, 0);
}
static void dw8250_idma_filter_test(struct kunit *test)
{
	struct dma_chan chan;
	struct dma_device dev;
	void *param = (void *)0x1234;
	chan.device = &dev;
	chan.device->dev = param;
	int ret = dw8250_idma_filter(&chan, param);
	KUNIT_EXPECT_EQ(test, ret, 1);
	chan.device->dev = param + 1;
	ret = dw8250_idma_filter(&chan, param);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void clk_to_dw8250_data_test(struct kunit *test)
{
	struct notifier_block nb;
        struct dw8250_data *ret = clk_to_dw8250_data(&nb);
	//kunit_info(test, "RET %d %x %x",(u8*)ret-(u8*)&nb, ret, nb);
        KUNIT_EXPECT_EQ(test, (u64)((u8*)ret + 336), (u64)&nb);
}

static void dw8250_check_lcr_test(struct kunit *test)
{
	struct uart_port p;
	struct dw8250_data data;
	p.membase = buffer;
	p.regshift = 0;
	p.private_data = &data;
	test_fcr_enable_fifo_lsr = 0;
	trial_count = 0;
	serial_port_in_called = 0;
	dw8250_check_lcr(&p, UART_LCR, 0);

	trial_count = 0;
	dw8250_check_lcr(&p, UART_LCR, ~UART_LCR_SPAR);

	trial_count = 0;
	p.type = PORT_OCTEON;
	dw8250_check_lcr(&p, UART_LCR, ~UART_LCR_SPAR);

	trial_count = 0;
	p.type = UPIO_MEM32;
	dw8250_check_lcr(&p, UART_LCR, ~UART_LCR_SPAR);

	trial_count = 0;
	p.type = UPIO_MEM32BE;
	dw8250_check_lcr(&p, UART_LCR, ~UART_LCR_SPAR);

	data.uart_16550_compatible = 1;
	trial_count = 0;
	dw8250_check_lcr(&p, UART_LCR, ~UART_LCR_SPAR);

	data.uart_16550_compatible = 1;
	trial_count = 0;
	dw8250_check_lcr(&p, 0, 0);

	KUNIT_EXPECT_GT(test, serial_port_in_called, 0);

}
static void dw8250_serial_out38x_test(struct kunit *test) {
	struct uart_port p;
	struct dw8250_data data;
	p.membase = buffer;
	p.private_data = &data;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_out38x(&p,0,0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_out38x(&p,UART_LCR,0);
	KUNIT_EXPECT_GT(test, serial_port_in_called, 0);
}
static void dw8250_serial_in_test(struct kunit *test) {
	struct uart_port p;
	p.membase = buffer;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_in(&p, 0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
}
static void dw8250_serial_inq_test(struct kunit *test) {
	struct uart_port p;
	p.membase = buffer;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_inq(&p, 0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
}
static void dw8250_serial_outq_test(struct kunit *test) {
	struct uart_port p;
	p.membase = buffer;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_outq(&p, 0, 0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
}
static void dw8250_serial_in32be_test(struct kunit *test) {
	struct uart_port p;
	p.membase = buffer;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_in32be(&p, 0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
}
static void dw8250_serial_out32be_test(struct kunit *test) {
	struct uart_port p;
	p.membase = buffer;
	p.regshift = 0;
	trial_count = 0;
	serial_port_in_called=0;
	test_fcr_enable_fifo_lsr = 0;
	dw8250_serial_out32be(&p, 0, 0);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 0);
}
static void dw8250_rzn1_get_dmacr_burst_test(struct kunit *test) {
	int ret;
	ret = dw8250_rzn1_get_dmacr_burst(10);
	KUNIT_EXPECT_EQ(test, ret, RZN1_UART_xDMACR_8_WORD_BURST);
	ret = dw8250_rzn1_get_dmacr_burst(6);
	KUNIT_EXPECT_EQ(test, ret, RZN1_UART_xDMACR_4_WORD_BURST);
	ret = dw8250_rzn1_get_dmacr_burst(2);
	KUNIT_EXPECT_EQ(test, ret, RZN1_UART_xDMACR_1_WORD_BURST);
}
static void dw8250_prepare_tx_dma_test(struct kunit *test) {
	struct uart_8250_port data;
	struct uart_8250_dma dma;
	data.port.membase = buffer;
	data.port.regshift = 0;
	data.dma = &dma;
	dw8250_prepare_tx_dma(&data);
}
static void dw8250_prepare_rx_dma_test(struct kunit *test) {
	struct uart_8250_port data;
	struct uart_8250_dma dma;
	data.port.membase = buffer;
	data.port.regshift = 0;
	data.dma = &dma;
	dw8250_prepare_rx_dma(&data);
}
static void dw8250_force_idle_test(struct kunit *test)
{
	struct uart_port p;
	serial_port_in_called = 0;
	printk("%s %d",__FILE__,__LINE__);
	dw8250_force_idle(&p);
	KUNIT_EXPECT_EQ(test, serial8250_clear_and_reinit_fifos_called, 1);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 1);
	serial_port_in_called = 0;
	serial8250_clear_and_reinit_fifos_called = 0;
	test_fcr_enable_fifo = 1;
	printk("%s %d",__FILE__,__LINE__);
	dw8250_force_idle(&p);
	KUNIT_EXPECT_EQ(test, serial8250_clear_and_reinit_fifos_called, 1);
	KUNIT_EXPECT_EQ(test, serial_port_in_called, 1);
	test_fcr_enable_fifo = 0;
        serial_port_in_called = 0;
        serial8250_clear_and_reinit_fifos_called = 0;
        test_fcr_enable_fifo = 1;
	test_fcr_enable_fifo_lsr = 1;
	printk("%s %d",__FILE__,__LINE__);
        dw8250_force_idle(&p);
        KUNIT_EXPECT_EQ(test, serial8250_clear_and_reinit_fifos_called, 1);
        KUNIT_EXPECT_EQ(test, serial_port_in_called, 2);
}

static struct kunit_case dw8250_test_cases[] = {
	KUNIT_CASE(dw8250_fallback_dma_filter_test),
	KUNIT_CASE(clk_to_dw8250_data_test),
	KUNIT_CASE(dw8250_force_idle_test),
	KUNIT_CASE(dw8250_check_lcr_test),
	KUNIT_CASE(dw8250_serial_out38x_test),
	KUNIT_CASE(dw8250_serial_in_test),
	KUNIT_CASE(dw8250_serial_inq_test),
	KUNIT_CASE(dw8250_serial_outq_test),
	KUNIT_CASE(dw8250_serial_in32be_test),
	KUNIT_CASE(dw8250_serial_out32be_test),
	KUNIT_CASE(dw8250_rzn1_get_dmacr_burst_test),
	KUNIT_CASE(dw8250_prepare_tx_dma_test),
	KUNIT_CASE(dw8250_prepare_rx_dma_test),
	KUNIT_CASE(dw8250_idma_filter_test),
	KUNIT_CASE(dw8250_reset_control_assert_test),
	{}
};

static struct kunit_suite dw8250_test_suite = {
	.name = "dw8250-test",
	.test_cases = dw8250_test_cases,
};

kunit_test_suite(dw8250_test_suite);

