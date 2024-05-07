/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: _enter of grass, context jump
 */
    .section .text
    .global grass_entry, ctx_jump
grass_entry:
    li sp,0x80003f80
    call main

ctx_jump:
    /* Read Back User SP */
    csrr sp, mscratch
    /* Restore RA of Interrupted Procedure */
    lw ra, 108(sp)
    /* Restore all Arguments Used in User Level Execution */
    lw a7, 104(sp)
    lw a6, 100(sp)
    lw a5, 96(sp)
    lw a4, 92(sp)
    lw a3, 88(sp)
    lw a2, 84(sp)
    lw a1, 80(sp)
    lw a0, 76(sp)
    /* Restore all Temporaries Used in User Level Execution */
    lw t6, 72(sp)
    lw t5, 68(sp)
    lw t4, 64(sp)
    lw t3, 60(sp)
    lw t2, 56(sp)
    lw t1, 52(sp)
    lw t0, 48(sp)
    /* Restore all Saved Registers */
    lw s11,44(sp)
    lw s10,40(sp)
    lw s9, 36(sp)
    lw s8, 32(sp)
    lw s7, 28(sp)
    lw s6, 24(sp)
    lw s5, 20(sp)
    lw s4, 16(sp)
    lw s3, 12(sp)
    lw s2, 8(sp)
    lw s1, 4(sp)
    lw s0, 0(sp)
    addi sp, sp, 116
    mret
