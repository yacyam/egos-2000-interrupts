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
#define TTY_BUFF_SIZE 128
#define UART0_RX_INTR 2
#define UART0_TX_INTR 1
#include "egos.h"
#include <stdio.h>
#include <stdarg.h>

struct tty_buff
{
    char buf[TTY_BUFF_SIZE];
    int size;
    int head;
    int tail;
};

struct tty_buff tty_read_buf;
struct tty_buff tty_write_buf;

int is_initializing = 1;

int uart_intrp();
int uart_getc(int *c);
int uart_putc(int c);
void uart_init(long baud_rate);

void uart_txen();
void uart_txdis();

static int c, is_reading;
int tty_recv_intr() { return (is_reading) ? 0 : (uart_getc(&c) == 3); }

void tty_buff_init()
{
    for (int i = 0; i < TTY_BUFF_SIZE; i++)
    {
        tty_read_buf.buf[i] = 0;
        tty_write_buf.buf[i] = 0;
    }

    tty_read_buf.size = tty_write_buf.size = 0;
    tty_read_buf.head = tty_write_buf.head = 0;
    tty_read_buf.tail = tty_write_buf.tail = 0;
}

void tty_write_initial(char *msg, int len)
{
    int rc;
    for (int i = 0; i < len; i++)
    {
        do
        {
            rc = uart_putc(msg[i]);
        } while (rc == RET_FAIL);
    }
}

void tty_write_uart()
{
    int c, head_ptr, size;
    int rc = RET_SUCCESS;

    head_ptr = tty_write_buf.head;
    size = tty_write_buf.size;

    while (size > 0)
    {
        c = (int)tty_write_buf.buf[head_ptr];
        rc = uart_putc(c);

        if (rc == RET_FAIL)
        {
            uart_txen();
            break;
        }

        head_ptr = (head_ptr + 1) % TTY_BUFF_SIZE;
        size--;
    };

    if (rc == RET_SUCCESS)
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
        if (size < TTY_BUFF_SIZE)
        {
            tail_ptr = (tail_ptr + 1) % TTY_BUFF_SIZE;
            size++;
        }
    }

    tty_write_buf.tail = tail_ptr;
    tty_write_buf.size = size;
}

int tty_write(char *msg, int len)
{
    if (is_initializing)
    {
        tty_write_initial(msg, len);
        return RET_SUCCESS;
    }

    if (len > TTY_BUFF_SIZE)
        return RET_ERR;

    if (len > TTY_BUFF_SIZE - tty_write_buf.size)
        return RET_FAIL;

    /* Write Contents into Buffer */
    tty_write_buff(msg, len);
    /* Write Buffer into UART0 */
    tty_write_uart();

    return RET_SUCCESS;
}

int tty_read_uart()
{
    uart_getc(&c);
    if (c == -1)
        return -1;

    int tail_ptr = tty_read_buf.tail;

    do
    {
        tty_read_buf.buf[tail_ptr] = (char)c;

        if (tty_read_buf.size < TTY_BUFF_SIZE)
        {
            tail_ptr = (tail_ptr + 1) % TTY_BUFF_SIZE;
            tty_read_buf.size++;
        }
    } while (uart_getc(&c) != -1);

    tty_read_buf.tail = tail_ptr;
    return 0;
}

int tty_read(char *ret_val)
{
    if (tty_read_buf.size == 0)
        return -1;

    int head_ptr = tty_read_buf.head;
    *ret_val = (char)tty_read_buf.buf[head_ptr];

    tty_read_buf.buf[head_ptr] = 0;

    tty_read_buf.head = (head_ptr + 1) % TTY_BUFF_SIZE;

    tty_read_buf.size--;
    return 0;
}

int tty_read_initial(char *buf, int len)
{
    for (int i = 0; i < len - 1; i++)
    {
        for (c = -1; c == -1; uart_getc(&c))
            ;
        buf[i] = (char)c;

        switch (c)
        {
        case 0x03: /* Ctrl+C    */
            buf[0] = 0;
        case 0x0d: /* Enter     */
            buf[i] = is_reading = 0;
            printf("\r\n");
            return c == 0x03 ? 0 : i;
        case 0x7f: /* Backspace */
            c = 0;
            if (i)
                printf("\b \b");
            i = i ? i - 2 : i - 1;
        }

        if (c)
            printf("%c", c);
        fflush(stdout);
    }

    buf[len - 1] = 0;
    return len - 1;
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

int tty_initializing()
{
    return is_initializing;
}

int tty_set_initializing(int state)
{
    is_initializing = state;
    return RET_SUCCESS;
}

int tty_handle_intr()
{
    int rc, ip;
    ip = uart_intrp();

    is_initializing = 1;
    // tty_critical("ip: %d\n", ip);
    is_initializing = 0;

    if (ip & UART0_RX_INTR)
        tty_read_uart();
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

    earth->tty_read = tty_read;
    earth->tty_read_initial = tty_read_initial;
    earth->tty_write = tty_write;
    earth->tty_recv_intr = tty_recv_intr;

    earth->tty_initializing = tty_initializing;
    earth->tty_set_initializing = tty_set_initializing;

    earth->tty_printf = tty_printf;
    earth->tty_info = tty_info;
    earth->tty_fatal = tty_fatal;
    earth->tty_success = tty_success;
    earth->tty_critical = tty_critical;
}
