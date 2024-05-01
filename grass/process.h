#pragma once

#include "elf.h"
#include "disk.h"

#define NUM_REGS 32

enum
{
    PROC_UNUSED,
    PROC_LOADING, /* allocated and wait for loading elf binary */
    PROC_READY,   /* finished loading elf and wait for first running */
    PROC_RUNNING,
    PROC_RUNNABLE,
    PROC_REQUESTING
};

struct process
{
    int pid;
    int status;
    int killable;
    void *sp, *mepc; /* process context = stack pointer (sp)
                      * + machine exception program counter (mepc) */
};

#define MAX_NPROCESS 16
extern int proc_curr_idx;
extern struct process proc_set[MAX_NPROCESS];
#define curr_pid proc_set[proc_curr_idx].pid
#define curr_status proc_set[proc_curr_idx].status

void intr_entry(int);
void excp_entry(int);

int proc_alloc();
void proc_free(int);
void proc_set_ready(int);
void proc_set_running(int);
void proc_set_runnable(int);
void proc_set_requesting(int);

void ctx_entry(void);
void ctx_jump();
