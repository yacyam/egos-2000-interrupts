/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: system support to C library function printf()
 */

#include "egos.h"
#include "print.h"
#include <unistd.h>

/* printf() is linked from the compiler's C library;
 * printf() constructs a string based on its arguments
 * and prints the string to the tty device by calling _write().
 */

static int is_kernel;

void _print_set_kernel()
{
    is_kernel = 1;
}

void _print_set_user()
{
    is_kernel = 0;
}

int _write(int file, char *ptr, int len)
{
    if (file != STDOUT_FILENO)
        return -1;

    if (is_kernel)
        return earth->tty_write_kernel(ptr, len);

    if (len > DEV_BUFF_SIZE)
    {
        grass->sys_tty_write(ptr, DEV_BUFF_SIZE);
        return DEV_BUFF_SIZE;
    }

    grass->sys_tty_write(ptr, len);
    return len; // Printf() Expects Amount of Input Written as Return Code
}

int _close(int file) { return -1; }
int _fstat(int file, void *stat) { return -1; }
int _lseek(int file, int ptr, int dir) { return -1; }
int _read(int file, void *ptr, int len) { return -1; }
int _isatty(int file) { return (file == STDOUT_FILENO); }
void _kill() {}
int _getpid() { return -1; }
void _exit(int status)
{
    grass->sys_exit(status);
    while (1)
        ;
}
