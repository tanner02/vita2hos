.syntax unified
.arm
.section .text
.global module_start, module_info
text_base:
module_start:
    adr r2, module_start
    ldr r3, =0x81000000
    cmp r2, r3
    movne r0, #4
    bne exit_stub
    ldr r1, =data_seed
    ldr r2, [r1]
    ldr r3, =0x12345678
    cmp r2, r3
    movne r0, #1
    bne exit_stub
    ldr r1, =bss_word
    ldr r2, [r1]
    cmp r2, #0
    movne r0, #2
    bne exit_stub
    mov r2, #0x5a
    str r2, [r1]
    ldr r0, [r1]
    cmp r0, r2
    movne r0, #3
    moveq r0, #0
    b exit_stub
exit_stub:
    mvn r0, #0
    bx lr
    nop
.ltorg
.balign 4
module_info:
    .hword 0, 0x0101
module_name:
    .asciz "vita2hos_fixed_probe"
    .space 27 - (. - module_name)
    .byte 0
    .word 0
    .word 0, 0
    .word imports - text_base, imports_end - text_base
    .word 0
    .word 0, 0, 0
    .word module_start - text_base, 0xffffffff
    .word 0, 0, 0, 0
    .word 0x03600000
imports:
    .hword 0x34, 1, 0, 1, 0, 0
    .word 0, 0xcae9ace6, library_name, 0
    .word function_nids, function_entries, 0, 0, 0, 0
imports_end:
function_nids:
    .word 0x7595d9aa
function_entries:
    .word exit_stub
library_name:
    .asciz "SceLibKernel"
.section .data
.balign 4
data_seed:
    .word 0x12345678
.section .bss
.balign 4
bss_word:
    .space 4
