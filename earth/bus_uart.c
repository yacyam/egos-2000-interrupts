/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: definitions for UART0 in FE310
 * see chapter18 of the SiFive FE310-G002 Manual
 */

#include "egos.h"
#include "bus_gpio.c"

#define UART0_BASE 0x10013000UL
#define UART0_TXDATA 0UL
#define UART0_RXDATA 4UL
#define UART0_TXCTRL 8UL
#define UART0_RXCTRL 12UL
#define UART0_IE 16UL
#define UART0_IP 20UL
#define UART0_DIV 24UL

void uart_init(long baud_rate)
{
    REGW(UART0_BASE, UART0_DIV) = CPU_CLOCK_RATE / baud_rate - 1;
    REGW(UART0_BASE, UART0_TXCTRL) |= 1;
    REGW(UART0_BASE, UART0_RXCTRL) |= 1;

    /* UART0 send/recv are mapped to GPIO pin16 and pin17 */
    REGW(GPIO0_BASE, GPIO0_IOF_ENABLE) |= (1 << 16) | (1 << 17);

    /* Increase UART0 Write Watermark to 3 */
    REGW(UART0_BASE, UART0_TXCTRL) |= 0x30000;

    /* Enable UART0 Interrupts for Writes and Reads */
    REGW(UART0_BASE, UART0_IE) |= 2;
}

int uart_pend_intr()
{
    return REGW(UART0_BASE, UART0_IP);
}

void uart_txen()
{
    REGW(UART0_BASE, UART0_IE) |= 0x1;
}

void uart_txdis()
{
    REGW(UART0_BASE, UART0_IE) &= ~(0x1);
}

int uart_getc(int *c)
{
    int ch = REGW(UART0_BASE, UART0_RXDATA);
    return *c = (ch & (1 << 31)) ? -1 : (ch & 0xFF); /* Bit 31 Indicates if Empty */
}

int uart_putc(int c)
{
    int is_full = (REGW(UART0_BASE, UART0_TXDATA) & (1 << 31));

    if (is_full)
        return -1;

    REGW(UART0_BASE, UART0_TXDATA) = c;
    return 0;
}
