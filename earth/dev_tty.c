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

int uart_pend_intr();
int uart_getc(int *c);
int uart_putc(int c);
void uart_init(long baud_rate);

void uart_txen();
void uart_txdis();

static int c, is_reading;
int tty_recv_intr() { return (is_reading) ? 0 : (uart_getc(&c) == 3); }

void tty_buff_init()
{
    for (int i = 0; i < DEV_BUFF_SIZE; i++)
    {
        tty_read_buf.buf[i] = 0;
        tty_write_buf.buf[i] = 0;
    }

    tty_read_buf.size = tty_write_buf.size = 0;
    tty_read_buf.head = tty_write_buf.head = 0;
    tty_read_buf.tail = tty_write_buf.tail = 0;
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
    return len;
}

void tty_write_uart()
{
    int c, head_ptr, size;
    int rc = 0;

    head_ptr = tty_write_buf.head;
    size = tty_write_buf.size;

    while (size > 0)
    {
        c = (int)tty_write_buf.buf[head_ptr];
        rc = uart_putc(c);

        if (rc == -1)
        {
            uart_txen();
            break;
        }

        head_ptr = (head_ptr + 1) % DEV_BUFF_SIZE;
        size--;
    };

    if (rc == 0)
    {
        uart_txdis();
    }

    tty_write_buf.head = head_ptr;
    tty_write_buf.size = size;
}

void tty_write_buff(char *msg, int len)
{
    int tail_ptr, size;
    tail_ptr = tty_write_buf.tail;
    size = tty_write_buf.size;

    for (int i = 0; i < len; i++)
    {
        tty_write_buf.buf[tail_ptr] = msg[i];
        if (size < DEV_BUFF_SIZE)
        {
            tail_ptr = (tail_ptr + 1) % DEV_BUFF_SIZE;
            size++;
        }
    }

    tty_write_buf.tail = tail_ptr;
    tty_write_buf.size = size;
}

int tty_write(char *msg, int len)
{
    if (len > DEV_BUFF_SIZE)
        return -2; // Error, Retry with smaller request

    if (len > DEV_BUFF_SIZE - tty_write_buf.size)
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

    int tail = tty_read_buf.tail;

    /* Put Special Character into Buffer for Subsequent Read by Kernel */
    if (c == SPECIAL_CTRL_C)
    {
        tty_read_buf.buf[tail] = (char)c;
        tty_read_buf.tail = (tail + 1) % DEV_BUFF_SIZE;
        tty_read_buf.size++;
        return RET_SPECIAL_CHAR;
    }

    do
    {
        tty_read_buf.buf[tail] = (char)c;

        if (tty_read_buf.size < DEV_BUFF_SIZE)
        {
            tail = (tail + 1) % DEV_BUFF_SIZE;
            tty_read_buf.size++;
        }
    } while (uart_getc(&c) != -1);

    tty_read_buf.tail = tail;
    return 0;
}

int tty_read(char *ret_val)
{
    if (tty_read_buf.size == 0)
        return -1;

    int head_ptr = tty_read_buf.head;
    *ret_val = (char)tty_read_buf.buf[head_ptr];

    tty_read_buf.buf[head_ptr] = 0;

    tty_read_buf.head = (head_ptr + 1) % DEV_BUFF_SIZE;

    tty_read_buf.size--;
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
    earth->tty_recv_intr = tty_recv_intr;

    earth->tty_kernel_mode = tty_kernel_mode;
    earth->tty_user_mode = tty_user_mode;

    earth->tty_printf = tty_printf;
    earth->tty_info = tty_info;
    earth->tty_fatal = tty_fatal;
    earth->tty_success = tty_success;
    earth->tty_critical = tty_critical;
}
