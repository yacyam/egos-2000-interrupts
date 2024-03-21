/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: RISC-V interrupt and exception handling
 */

#include "egos.h"

#define PLIC_ENABLE_BASE 0x0C002000UL
#define PLIC_PRIORITY_BASE 0x0C000000UL
#define PLIC_PENDING_BASE 0x0C001000UL
#define PLIC_CLAIM_BASE 0x0C200004UL
#define PLIC_UART0_ID 3UL

/* These are two static variables storing
 * the addresses of the handler functions;
 * Initially, both variables are NULL */
static void (*intr_handler)(int);
static void (*excp_handler)(int);

/* Register handler functions by modifying the static variables */
int intr_register(void (*_handler)(int)) { intr_handler = _handler; }
int excp_register(void (*_handler)(int)) { excp_handler = _handler; }

/* Device Interrupt Access Functions */
int tty_read_uart();

void trap_entry_vm(); /* This wrapper function is defined in earth.S */
void trap_entry_start();
void trap_entry()
{
    int mcause;

    asm("csrr %0, mcause" : "=r"(mcause));

    int id = mcause & 0x3FF;

    if (mcause & (1 << 31))
        (intr_handler) ? intr_handler(id) : FATAL("trap_entry: interrupt handler not registered");
    else
        (excp_handler) ? excp_handler(id) : FATAL("trap_entry: exception handler not registered");
}

int trap_external()
{
    int intr_cause = REGW(PLIC_CLAIM_BASE, 0);
    int rc;

    CRITICAL("INTR CAUSE: %d", intr_cause);

    switch (intr_cause)
    {
    case PLIC_UART0_ID:
        rc = tty_read_uart();
        break;
    }

    REGW(PLIC_CLAIM_BASE, 0) = intr_cause; // Interrupt Pending Bit is Cleared
    return rc;
}

void intr_init()
{
    earth->intr_register = intr_register;
    earth->excp_register = excp_register;
    earth->trap_external = trap_external;

    /* Setup the interrupt/exception entry function */
    if (earth->translation == PAGE_TABLE)
    {
        asm("csrw mtvec, %0" ::"r"(trap_entry_vm));
        INFO("Use direct mode and put the address of trap_entry_vm() to mtvec");
    }
    else
    {
        asm("csrw mtvec, %0" ::"r"(trap_entry_start));
        INFO("Use direct mode and put the address of trap_entry() to mtvec");
    }

    /* Enable the machine-mode timer and software interrupts */
    int mstatus, mie;
    asm("csrr %0, mie" : "=r"(mie));
    asm("csrw mie, %0" ::"r"(mie | 0x888));
    asm("csrr %0, mstatus" : "=r"(mstatus));
    asm("csrw mstatus, %0" ::"r"(mstatus | 0x80));

    /* Enable External Interrupts on PLIC */
    REGW(PLIC_ENABLE_BASE, 0) |= (1 << PLIC_UART0_ID);
    REGW(PLIC_PRIORITY_BASE, 4 * PLIC_UART0_ID) |= 1; // Set UART0 Priority to 1
    REGW(PLIC_PENDING_BASE, 0) = 0;                   // Reset Pending Interrupts
}
