/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <keep.h>
#include <util.h>
#include <string.h>


/* Register definitions */
#define URXD  0x0  /* Receiver Register */
#define UTXD  0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define UTS   0xb4 /* UART Test Register (mx31) */
#define UMCR  0xb8 /* RS-485 Mode Control Register */

/* UART RX Data Registers Bit Fields.*/
#define  URXD_CHARRDY    (1<<15)
#define  URXD_ERR        (1<<14)
#define  URXD_OVRRUN     (1<<13)
#define  URXD_FRMERR     (1<<12)
#define  URXD_BRK        (1<<11)
#define  URXD_PRERR      (1<<10)
#define  URXD_RX_DATA    (0xFF)

/* UART Control Register1 Bit Fields.*/
#define  UCR1_ADEN       (1<<15) /* Auto dectect interrupt */
#define  UCR1_ADBR       (1<<14) /* Auto detect baud rate */
#define  UCR1_TRDYEN     (1<<13) /* Transmitter ready interrupt enable */
#define  UCR1_IDEN       (1<<12) /* Idle condition interrupt */
#define  UCR1_RRDYEN     (1<<9)	 /* Recv ready interrupt enable */
#define  UCR1_RDMAEN     (1<<8)	 /* Recv ready DMA enable */
#define  UCR1_IREN       (1<<7)	 /* Infrared interface enable */
#define  UCR1_TXMPTYEN   (1<<6)	 /* Transimitter empty interrupt enable */
#define  UCR1_RTSDEN     (1<<5)	 /* RTS delta interrupt enable */
#define  UCR1_SNDBRK     (1<<4)	 /* Send break */
#define  UCR1_TDMAEN     (1<<3)	 /* Transmitter ready DMA enable */
#define  UCR1_UARTCLKEN  (1<<2)	 /* UART clock enabled */
#define  UCR1_DOZE       (1<<1)	 /* Doze */
#define  UCR1_UARTEN     (1<<0)	 /* UART enabled */

/* UART Control Register2 Bit Fields.*/
#define UCR2_SRST          (1<<0)
#define UCR2_RXEN          (1<<1)
#define UCR2_TXEN          (1<<2)
#define UCR2_ATEN          (1<<3)
#define UCR2_RTSEN         (1<<4)
#define UCR2_WS            (1<<5)
#define UCR2_STPB          (1<<6)
#define UCR2_PROE          (1<<7)
#define UCR2_PREN          (1<<8)
#define UCR2_RTEC_MASK     (3<<9)
#define UCR2_RTEC_RISING   (0<<9)
#define UCR2_RTEC_FALLING  (1<<9)
#define UCR2_RTEC_BOTH     (2<<9)
#define UCR2_ESCEN         (1<<11)
#define UCR2_CTS           (1<<12)
#define UCR2_CTSC          (1<<13)
#define UCR2_IRTS          (1<<14)
#define UCR2_ESCI          (1<<15)

/* UART Control Register3 Bit Fields.*/
#define UCR3_ACIEN         (1<<0)
#define UCR3_INVT          (1<<1)
#define UCR3_RXDMUXSEL     (1<<2)
#define UCR3_DTRDEN        (1<<3)
#define UCR3_AWAKEN        (1<<4)
#define UCR3_AIRINTEN      (1<<5)
#define UCR3_RXDSEN        (1<<6)
#define UCR3_ADNIMP        (1<<7)
#define UCR3_RI            (1<<8)
#define UCR3_DCD           (1<<9)
#define UCR3_DSR           (1<<10)
#define UCR3_FRAERREN      (1<<11)
#define UCR3_PARERREN      (1<<12)
#define UCR3_DTREN         (1<<13)
#define UCR3_DPEC_MASK     (3<<14)
#define UCR3_DPEC_RISING   (0<<14)
#define UCR3_DPEC_FALLING  (1<<14)
#define UCR3_DPEC_BOTH     (2<<14)

/* UART Control Register3 Bit Fields.*/
#define UCR4_DREN          (1<<0)
#define UCR4_OREN          (1<<1)
#define UCR4_BKEN          (1<<2)
#define UCR4_TCEN          (1<<3)
#define UCR4_LPBYP         (1<<4)
#define UCR4_IRSC          (1<<5)
#define UCR4_IDDMAEN       (1<<6)
#define UCR4_WKEN          (1<<7)
#define UCR4_ENIRI         (1<<8)
#define UCR4_INVR          (1<<9)
#define UCR4_CTSTL_MASK    (0x3f<<10)
#define UCR4_CTSTL_SHIFT   10

/* UART Test Register Bit Fields.*/
#define  UTS_FRCPERR	 (1<<13) /* Force parity error */
#define  UTS_LOOP        (1<<12) /* Loop tx and rx */
#define  UTS_TXEMPTY	 (1<<6)	 /* TxFIFO empty */
#define  UTS_RXEMPTY	 (1<<5)	 /* RxFIFO empty */
#define  UTS_TXFULL	 (1<<4)	 /* TxFIFO full */
#define  UTS_RXFULL	 (1<<3)	 /* RxFIFO full */
#define  UTS_SOFTRST	 (1<<0)	 /* Software reset */

/* UART FIFO Control Register Bit Fields.*/
#define UFCR_RXTL_MASK     (0x3f<<0)
#define UFCR_RXTL_SHIFT    0
#define UFCR_RFDIV_6       (0<<7)
#define UFCR_RFDIV_5       (1<<7)
#define UFCR_RFDIV_4       (2<<7)
#define UFCR_RFDIV_3       (3<<7)
#define UFCR_RFDIV_2       (4<<7)
#define UFCR_RFDIV_1       (5<<7)
#define UFCR_RFDIV_7       (6<<7)
#define UFCR_TXTL_MASK     (0x3f<<10)
#define UFCR_TXTL_SHIFT    10

#define RTS_TRIGGER_LEVEL   30
#define TX_TRIGGER_LEVEL    2
#define RX_TRIGGER_LEVEL    1
#define RESET_TIMEOUT_COUNT 100

#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

typedef struct _imx_uart_regs {
    uint32_t ucr1;
    uint32_t ucr2;
    uint32_t ucr3;
    uint32_t ucr4;
    uint32_t usr1;
    uint32_t usr2;
    uint32_t ufcr;
    uint32_t ubir;
    uint32_t ubmr;
} imx_uart_regs;


static vaddr_t chip_to_base(struct serial_chip *chip)
{
	struct imx_uart_data *pd =
		container_of(chip, struct imx_uart_data, chip);

	return io_pa_or_va(&pd->base);
}

static void imx_uart_flush(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	while (!(read32(base + UTS) & UTS_TXEMPTY))
		;
}

static int imx_uart_getchar(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	while (read32(base + UTS) & UTS_RXEMPTY)
		;

	return (read32(base + URXD) & URXD_RX_DATA);
}

static bool imx_uart_have_rx_data(struct serial_chip *chip)
{
	vaddr_t base = chip_to_base(chip);

	if (read32(base + UTS) & UTS_RXEMPTY) {
		return false;
	}

	return true;
}

static void imx_uart_putc(struct serial_chip *chip, int ch)
{
	vaddr_t base = chip_to_base(chip);

	write32(ch, base + UTXD);

	/* Wait until sent */
	while (!(read32(base + UTS) & UTS_TXEMPTY))
		;
}

static const struct serial_ops imx_uart_ops = {
	.flush = imx_uart_flush,
	.getchar = imx_uart_getchar,
	.putc = imx_uart_putc,
	.have_rx_data = imx_uart_have_rx_data,
};
KEEP_PAGER(imx_uart_ops);

void imx_uart_init(struct imx_uart_data *pd, paddr_t base)
{
	pd->base.pa = base;
	pd->chip.ops = &imx_uart_ops;

	/*
	 * Do nothing, debug uart(uart0) share with normal world,
	 * everything for uart0 initialization is done in bootloader.
	 */
}

bool imx_uart_init_ex(struct imx_uart_data *pd, 
                      paddr_t base, 
                      struct imx_uart_config *uart_config)
{
	static uint32_t rfdiv_codes[] = {
		0,
		UFCR_RFDIV_1,
		UFCR_RFDIV_2,
		UFCR_RFDIV_3,
		UFCR_RFDIV_4,
		UFCR_RFDIV_5,
		UFCR_RFDIV_6,
		UFCR_RFDIV_7,
		};
	imx_uart_regs uart_reg_image;
	uint32_t rfdiv;
	uint32_t num;
	uint32_t denom;
	uint32_t scale;
	uint32_t ubmr_plus1;
	uint32_t ubir_plus1;
	uint32_t i;

	memset(&uart_reg_image, 0, sizeof(uart_reg_image));

	/*
	 * Disable UART and reset control registers
	 */
	write32(0, base + UCR1);
	write32(0, base + UCR3);
	write32(0, base + UCR4);
	write32(0, base + UMCR);

	/*
	 * Initiate a reset, and wait for completion
	 */
	write32(0, base + UCR2);
	for (i = 0; i < RESET_TIMEOUT_COUNT; ++i) {
		if ((read32(base + UTS) & UTS_SOFTRST) == 0) {
			break;
		}
	}
	if ((read32(base + UTS) & UTS_SOFTRST) != 0) {
		return false;
	}

	/*
	 * Enable UART
	 */
	uart_reg_image.ucr1 = UCR1_UARTEN;

	/*
	 * Set constant line control configuration:
	 * - 1 stop bit
	 * - no parity
	 * - 8 bits
	 * - no HW flow control
	 * 
	 * TODO: Add parameters to imx_uart_config
 	 */
	uart_reg_image.ucr2 =
		UCR2_CTS  | /* RTS asserted */
		UCR2_IRTS | /* CTS ignored */
		UCR2_TXEN | /* Enable transmitter */
		UCR2_RXEN | /* Enable receiver */
		UCR2_SRST | /* No Reset*/
		UCR2_WS;    /* 8 bits */

	uart_reg_image.ucr3 =
		UCR3_RI  |
		UCR3_DCD | /* CTS ignored */
		UCR3_DSR |
		UCR3_RXDMUXSEL;

	uart_reg_image.ucr4 = RTS_TRIGGER_LEVEL << UCR4_CTSTL_SHIFT;

	/*
	 * Set baud rate, calculate dividers
	 */

	rfdiv = uart_config->clock_hz / (16 * uart_config->baud_rate);
	if (rfdiv == 0) {
		/*
	 	 * Baud rate is too high
 	 	 */
		return false;
	}

	rfdiv = min(rfdiv, 7);
	num = uart_config->clock_hz;
	denom = 16 * rfdiv * uart_config->baud_rate;
	scale = (max(num, denom) + 65535) / 65536;
	ubmr_plus1 = num / scale;
	ubir_plus1 = denom / scale;

	if ((ubmr_plus1 == 0) || (ubir_plus1 == 0)) {
		/*
		 * Baud rate is too high
		 */
		return false;
	}

	uart_reg_image.ufcr =
		(RX_TRIGGER_LEVEL << UFCR_RXTL_SHIFT) |
		rfdiv_codes[rfdiv] |
		(TX_TRIGGER_LEVEL << UFCR_TXTL_SHIFT);

	uart_reg_image.ubir = ubir_plus1 - 1;
	uart_reg_image.ubmr = ubmr_plus1 - 1;

	/*
	 * Apply to HW.
	 */
	write32(uart_reg_image.ucr3, base + UCR3);
	write32(uart_reg_image.ucr4, base + UCR4);
	write32(uart_reg_image.ufcr, base + UFCR);
	write32(uart_reg_image.ubir, base + UBIR);
	write32(uart_reg_image.ubmr, base + UBMR);
	write32(uart_reg_image.ucr2, base + UCR2);
	write32(uart_reg_image.ucr1, base + UCR1);

	/*
	 * Purge FIFOs
	 */
	while ((read32(base + URXD) & URXD_CHARRDY) != 0)
	;
	while ((read32(base + UTS) & UTS_TXEMPTY) == 0)
	;

	/*
	 * For debug read back.
	 */
	uart_reg_image.ucr1 = read32(base + UCR1);
	uart_reg_image.ucr2 = read32(base + UCR2);
	uart_reg_image.ucr3 = read32(base + UCR3);
	uart_reg_image.ucr4 = read32(base + UCR4);
	uart_reg_image.usr1 = read32(base + USR1);
	uart_reg_image.usr2 = read32(base + USR2);
	uart_reg_image.ufcr = read32(base + UFCR);
	uart_reg_image.ubir = read32(base + UBIR);
	uart_reg_image.ubmr = read32(base + UBMR);

	pd->base.pa = base;
	pd->chip.ops = &imx_uart_ops;
	memcpy(&pd->config, uart_config, sizeof(*uart_config));

	return true;
}
