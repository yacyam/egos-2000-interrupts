/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: grass layer initialization
 * Initialize timer and the process control block;
 * Spawn the first kernel process, GPID_PROCESS (pid=1).
 */

#include "egos.h"
#include "process.h"
#include "syscall.h"

struct grass *grass = (void *)APPS_STACK_TOP;
struct earth *earth = (void *)GRASS_STACK_TOP;

void kernel_init();

static int sys_proc_read(int block_no, char *dst)
{
    return earth->disk_read(SYS_PROC_EXEC_START + block_no, 1, dst);
}

int main()
{
    CRITICAL("Enter the grass layer");

    kernel_init();

    /* Initialize the grass interface functions */
    grass->proc_alloc = proc_alloc;
    grass->proc_free = proc_free;
    grass->proc_set_ready = proc_set_ready;

    grass->sys_exit = sys_exit;
    grass->sys_send = sys_send;
    grass->sys_recv = sys_recv;
    grass->sys_tty_read = sys_tty_read;
    grass->sys_tty_write = sys_tty_write;

    /* Register interrupt and exception handlers */
    earth->intr_register(intr_entry);
    earth->excp_register(excp_entry);

    /* Load the first kernel process GPID_PROCESS */
    INFO("Load kernel process #%d: sys_proc", GPID_PROCESS);
    elf_load(GPID_PROCESS, sys_proc_read, 0, 0);
    proc_set_running(proc_alloc());
    earth->mmu_switch(GPID_PROCESS);

    earth->tty_user_mode();

    /* Jump to the entry of process GPID_PROCESS */
    asm("mv a0, %0" ::"r"(APPS_ARG));
    asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
    asm("mret");
}
