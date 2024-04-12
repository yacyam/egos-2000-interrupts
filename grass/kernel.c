/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: Kernel ≈ 3 handlers
 *     proc_yield() handles timer interrupt for process scheduling
 *     excp_entry() handles faults such as unauthorized memory access
 *     proc_syscall() handles system calls for inter-process communication
 */

#include "egos.h"
#include "process.h"
#include "syscall.h"
#include <string.h>

#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11

void excp_entry(int id)
{
    /* Student's code goes here (system call and memory exception). */

    /* If id is for system call, handle the system call and return */

    /* Otherwise, kill the process if curr_pid is a user application */

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}

#define INTR_ID_SOFT 3
#define INTR_ID_TIMER 7
#define INTR_ID_EXTERNAL 11

static void proc_yield();
static void proc_syscall();
static void proc_external();
static void (*kernel_entry)();

struct kernel_msg
{
    int in_use;
    int sender;
    int receiver;
    char msg[SYSCALL_MSG_LEN];
};

struct kernel_msg *KERNEL_MSG_BUFF =
    (struct kernel_msg *)(GRASS_STACK_TOP - GRASS_STACK_SIZE + sizeof(struct grass));

static int proc_tty_read(struct syscall *sc);
static int proc_tty_write(struct syscall *sc);
static void syscall_ret();

int proc_curr_idx;
struct process proc_set[MAX_NPROCESS];

void intr_entry(int id)
{
    if (id == INTR_ID_TIMER && curr_pid < GPID_SHELL)
    {
        /* Do not interrupt kernel processes since IO can be stateful */
        earth->timer_reset();
        return;
    }

    if (earth->tty_recv_intr() && curr_pid >= GPID_USER_START)
    {
        /* User process killed by ctrl+c interrupt */
        INFO("process %d killed by interrupt", curr_pid);
        asm("csrw mepc, %0" ::"r"(0x800500C));
        return;
    }

    if (id == INTR_ID_SOFT)
        kernel_entry = proc_syscall;
    else if (id == INTR_ID_TIMER)
        kernel_entry = proc_yield;
    else if (id == INTR_ID_EXTERNAL)
        kernel_entry = proc_external;
    // FATAL("intr_entry: got unknown interrupt %d", id);

    ctx_entry();
}

void ctx_entry()
{
    int mepc, sp;
    asm("csrr %0, mepc" : "=r"(mepc));
    asm("csrr %0, mscratch" : "=r"(sp));
    proc_set[proc_curr_idx].mepc = (void *)mepc;
    proc_set[proc_curr_idx].sp = (void *)sp;

    /* kernel_entry() is either proc_yield() or proc_syscall() */
    kernel_entry();

    /* Switch back to the user application stack */
    mepc = (int)proc_set[proc_curr_idx].mepc;
    sp = (int)proc_set[proc_curr_idx].sp;
    asm("csrw mepc, %0" ::"r"(mepc));
    asm("csrw mscratch, %0" ::"r"(sp));

    earth->tty_user_mode();
    ctx_jump();
}

int external_handle()
{
    int rc;
    int orig_proc_idx = proc_curr_idx;
    earth->trap_external();

    for (int i = 0; i < MAX_NPROCESS; i++)
    {
        if (proc_set[i].status == PROC_REQUESTING)
        {
            proc_curr_idx = i;
            earth->mmu_switch(proc_set[i].pid);
            syscall_ret();
        }
    }

    proc_curr_idx = orig_proc_idx;
    earth->mmu_switch(curr_pid);

    return 0;
}

void proc_wait()
{
    int mie;
    asm("csrr %0, mie" : "=r"(mie));
    asm("csrw mie, %0" ::"r"(mie & ~(0x88))); // Invert Timer and Software Interrupt Bits

    asm("wfi");

    asm("csrw mie, %0" ::"r"(mie | 0x88));
    external_handle();
}

static void proc_yield()
{
    /* Find the next runnable process */
    int next_idx = -1;
    while (next_idx == -1)
    {
        for (int i = 1; i <= MAX_NPROCESS; i++)
        {
            int s = proc_set[(proc_curr_idx + i) % MAX_NPROCESS].status;
            if (s == PROC_READY || s == PROC_RUNNING || s == PROC_RUNNABLE)
            {
                next_idx = (proc_curr_idx + i) % MAX_NPROCESS;
                break;
            }
        }

        if (next_idx == -1)
            proc_wait();
    }

    if (curr_status == PROC_RUNNING)
        proc_set_runnable(curr_pid);

    /* Switch to the next runnable process and reset timer */
    proc_curr_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->timer_reset();

    /* Student's code goes here (switch privilege level). */

    /* Modify mstatus.MPP to enter machine or user mode during mret
     * depending on whether curr_pid is a grass server or a user app
     */

    /* Student's code ends here. */

    /* Call the entry point for newly created process */
    if (curr_status == PROC_READY)
    {
        earth->tty_user_mode();
        proc_set_running(curr_pid);
        /* Prepare argc and argv */
        asm("mv a0, %0" ::"r"(APPS_ARG));
        asm("mv a1, %0" ::"r"(APPS_ARG + 4));
        /* Enter application code entry using mret */
        asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
        asm("mret");
    }

    proc_set_running(curr_pid);
}

static int y_send(struct syscall *sc)
{
    if (KERNEL_MSG_BUFF->in_use == 1)
    {
        external_handle();
        return -1;
    }

    KERNEL_MSG_BUFF->in_use = 1;
    KERNEL_MSG_BUFF->sender = curr_pid; // receiver may not know who is sending
    KERNEL_MSG_BUFF->receiver = sc->msg.receiver;

    memcpy(KERNEL_MSG_BUFF->msg, sc->msg.content, sizeof(sc->msg.content));

    external_handle(); // may be a process sending message to be killed
    return 0;
}

static int y_recv(struct syscall *sc)
{
    if (KERNEL_MSG_BUFF->in_use == 0 || KERNEL_MSG_BUFF->receiver != curr_pid)
    {
        external_handle();
        return -1;
    }

    KERNEL_MSG_BUFF->in_use = 0;

    memcpy(sc->msg.content, KERNEL_MSG_BUFF->msg, sizeof(sc->msg.content));
    sc->msg.sender = KERNEL_MSG_BUFF->sender;

    return 0;
}

static int proc_tty_read(struct syscall *sc)
{
    char *c;
    memcpy(&c, sc->msg.content, sizeof(char *));

    return earth->tty_read(c);
}

static int proc_tty_write(struct syscall *sc)
{
    char *msg;
    int len;

    memcpy(&msg, sc->msg.content, sizeof(char *));
    memcpy(&len, sc->msg.content + sizeof(msg), sizeof(int));
    return earth->tty_write(msg, len);
}

static void syscall_ret()
{
    struct syscall *sc = (struct syscall *)SYSCALL_ARG;
    int rc = -1;

    int type = sc->type;
    sc->retval = 0;
    sc->type = SYS_UNUSED;
    *((int *)0x2000000) = 0;

    proc_set_requesting(curr_pid);

    switch (type)
    {
    case SYS_RECV:
        rc = y_recv(sc);
        break;
    case SYS_SEND:
        rc = y_send(sc);
        break;
    case TTY_READ:
        rc = proc_tty_read(sc);
        break;
    case TTY_WRITE:
        rc = proc_tty_write(sc);
        break;
    }

    if (rc == 0)
        proc_set_runnable(curr_pid);
    else
        sc->type = type;
}

static void proc_syscall()
{
    syscall_ret();
    proc_yield();
}

static void proc_external()
{
    external_handle();
    proc_yield();
}

void kernel_init()
{
    KERNEL_MSG_BUFF->in_use = 0;
}