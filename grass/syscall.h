#pragma once

#include "servers.h"

static struct syscall *sc = (struct syscall *)SYSCALL_ARG;

enum syscall_type
{
    SYS_UNUSED,
    SYS_RECV,
    SYS_SEND,
    TTY_READ,
    TTY_WRITE,
    SYS_NCALLS
};

struct sys_msg
{
    int sender;
    int receiver;
    char content[SYSCALL_MSG_LEN];
};

struct syscall
{
    enum syscall_type type; /* Type of the system call */
    struct sys_msg msg;     /* Data of the system call */
    int retval;             /* Return value of the system call */
};

void sys_exit(int status);
int sys_send(int pid, char *msg, int size);
int sys_recv(int *pid, char *buf, int size);
int sys_tty_read(char *c);
int sys_tty_write(char *msg, int len);
