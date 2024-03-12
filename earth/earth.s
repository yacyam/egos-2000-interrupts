/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: boot loader
 * i.e., the first instructions executed by the CPU when boot up
 */
    .section .image.placeholder
    .section .text.enter
    .global earth_entry, trap_entry_vm, trap_entry_start
earth_entry:
    /* Disable machine interrupt */
    li t0, 0x8
    csrc mstatus, t0

    /* Call main() of earth.c */
    li sp, 0x80003f80
    call main

trap_entry_start:
    /* Swap User SP and Reg File Pointer */
    csrrw sp, mscratch, sp
    /* Save RA of Interrupted Procedure */
    sw ra, 108(sp)
    /* Save all Arguments Used in User Level Execution */
    sw a7, 104(sp)
    sw a6, 100(sp)
    sw a5, 96(sp)
    sw a4, 92(sp)
    sw a3, 88(sp)
    sw a2, 84(sp)
    sw a1, 80(sp)
    sw a0, 76(sp)
    /* Save all Temporaries Used in User Level Execution */
    sw t6, 72(sp)
    sw t5, 68(sp)
    sw t4, 64(sp)
    sw t3, 60(sp)
    sw t2, 56(sp)
    sw t1, 52(sp)
    sw t0, 48(sp)
    /* Save all Saved Registers for Idempotency */
    sw s11,44(sp)
    sw s10,40(sp)
    sw s9, 36(sp)
    sw s8, 32(sp)
    sw s7, 28(sp)
    sw s6, 24(sp)
    sw s5, 20(sp)
    sw s4, 16(sp)
    sw s3, 12(sp)
    sw s2, 8(sp)
    sw s1, 4(sp)
    sw s0, 0(sp)
    csrr t0, mscratch /* Load Back User SP */
    sw t0, 112(sp) /* Store User SP in PCB */
    li sp, 0x80003f80 /* Move SP to Top of Single Kernel Stack */
    j trap_entry

trap_entry_vm:
    csrw mscratch, t0

    /* Set mstatus.MPRV to enable page table translation in M mode */
    /* If mstatus.MPP is U mode, set to S mode for kernel privilege */
    li t0, 0x20800
    csrs mstatus, t0

    /* Jump to trap_entry() without modifying any registers */
    csrr t0, mscratch
    j trap_entry
