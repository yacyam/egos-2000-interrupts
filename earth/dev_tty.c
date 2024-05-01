/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a simple tty device driver
 * uart_getc() and uart_putc() are implemented in bus_uart.c
 * printf-related functions are linked from the compiler's C library
 */

#define LIBC_STDIO
#define UART0_RX_INTR 2
#define UART0_TX_INTR 1
#include "egos.h"
#include "character.h"
#include "print.h"
#include <stdio.h>
#include <stdarg.h>

struct tty_buff
{
    char buf[DEV_BUFF_SIZE];
    int size;
    int head;
    int tail;
};

struct tty_buff tty_read_buf;
struct tty_buff tty_write_buf;

#define rx_head tty_read_buf.head
#define rx_tail tty_read_buf.tail
#define rx_size tty_read_buf.size

#define tx_head tty_write_buf.head
#define tx_tail tty_write_buf.tail
#define tx_size tty_write_buf.size

int uart_pend_intr();
int uart_getc(int *c);
int uart_putc(int c);
void uart_init(long baud_rate);

void uart_txen();
void uart_txdis();

static int c;

void tty_buff_init()
{
    for (int i = 0; i < DEV_BUFF_SIZE; i++)
        tty_read_buf.buf[i] = tty_write_buf.buf[i] = 0;

    rx_size = tx_size = 0;
    rx_head = tx_head = 0;
    rx_tail = tx_tail = 0;
}

int tty_write_kernel(char *msg, int len)
{
    int rc;
    for (int i = 0; i < len; i++)
    {
        do
        {
            rc = uart_putc(msg[i]);
        } while (rc == -1);
    }
    return len; // Must Return Length of Message Written to _write()
}

void tty_write_uart()
{
    int c, rc = 0;

    while (tx_size > 0)
    {
        c = (int)tty_write_buf.buf[tx_head];
        rc = uart_putc(c);

        if (rc == -1)
        {
            uart_txen();
            break;
        }

        tx_head = (tx_head + 1) % DEV_BUFF_SIZE;
        tx_size--;
    };

    if (rc == 0)
        uart_txdis();
}

void tty_write_buff(char *msg, int len)
{
    for (int i = 0; i < len; i++)
    {
        tty_write_buf.buf[tx_tail] = msg[i];
        if (tx_size < DEV_BUFF_SIZE)
        {
            tx_tail = (tx_tail + 1) % DEV_BUFF_SIZE;
            tx_size++;
        }
    }
}

int tty_write(char *msg, int len)
{
    if (len > DEV_BUFF_SIZE)
        return -2; // Error, Retry with smaller request

    if (len > DEV_BUFF_SIZE - tx_size)
        return -1;

    /* Write Contents into Buffer */
    tty_write_buff(msg, len);
    /* Write Buffer into UART0 */
    tty_write_uart();

    return 0;
}

int tty_read_uart()
{
    uart_getc(&c);
    if (c == -1)
        return -1;

    /* Return to Kernel To Kill Killable Processes */
    if (c == SPECIAL_CTRL_C)
        return RET_SPECIAL_CHAR;

    do
    {
        tty_read_buf.buf[rx_tail] = (char)c;

        if (rx_size < DEV_BUFF_SIZE)
        {
            rx_tail = (rx_tail + 1) % DEV_BUFF_SIZE;
            rx_size++;
        }
    } while (uart_getc(&c) != -1);

    return 0;
}

int tty_read(char *ret_val)
{
    if (rx_size == 0)
        return -1;

    *ret_val = (char)tty_read_buf.buf[rx_head];

    tty_read_buf.buf[rx_head] = 0; // Flush Out Read Contents
    rx_head = (rx_head + 1) % DEV_BUFF_SIZE;
    rx_size--;
    return 0;
}

void tty_read_kernel(char *buf, int len)
{
    for (int i = 0; i < len; i++)
    {
        for (c = -1; c == -1; uart_getc(&c))
            ;
        buf[i] = (char)c;
    }
}

#define LOG(x, y)           \
    printf(x);              \
    va_list args;           \
    va_start(args, format); \
    vprintf(format, args);  \
    va_end(args);           \
    printf(y);

int tty_printf(const char *format, ...)
{
    LOG("", "")
    fflush(stdout);
}

int tty_info(const char *format, ...) { LOG("[INFO] ", "\r\n") }

int tty_fatal(const char *format, ...)
{
    LOG("\x1B[1;31m[FATAL] ", "\x1B[1;0m\r\n") /* red color */
    while (1)
        ;
}

int tty_success(const char *format, ...)
{
    LOG("\x1B[1;32m[SUCCESS] ", "\x1B[1;0m\r\n") /* green color */
}

int tty_critical(const char *format, ...)
{
    LOG("\x1B[1;33m[CRITICAL] ", "\x1B[1;0m\r\n") /* yellow color */
}

void tty_kernel_mode()
{
    _print_set_kernel();
}

void tty_user_mode()
{
    _print_set_user();
}

int tty_handle_intr()
{
    int rc, ip;
    ip = uart_pend_intr();

    if (ip & UART0_RX_INTR)
        rc = tty_read_uart();
    if (ip & UART0_TX_INTR)
        tty_write_uart();
    return rc;
}

void tty_init()
{
    /* 115200 is the UART baud rate */
    uart_init(115200);

    /* Wait for the tty device to be ready */
    for (int c = 0; c != -1; uart_getc(&c))
        ;

    tty_buff_init();
    tty_kernel_mode();

    earth->tty_read = tty_read;
    earth->tty_write = tty_write;
    earth->tty_read_kernel = tty_read_kernel;
    earth->tty_write_kernel = tty_write_kernel;

    earth->tty_kernel_mode = tty_kernel_mode;
    earth->tty_user_mode = tty_user_mode;

    earth->tty_printf = tty_printf;
    earth->tty_info = tty_info;
    earth->tty_fatal = tty_fatal;
    earth->tty_success = tty_success;
    earth->tty_critical = tty_critical;
}
