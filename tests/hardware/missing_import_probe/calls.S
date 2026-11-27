/* Owned callers of deliberately absent imports. Neither import is provided. */
.syntax unified
.arch armv7-a
.text
.arm
.balign 4
.global call_missing_arm
.type call_missing_arm,%function
call_missing_arm:
    push {r4,lr}
    movw r1,#0x5678
    movt r1,#0x1234
    bl missing_arm
    pop {r4,pc}
.size call_missing_arm,.-call_missing_arm
.thumb
.balign 2
.global call_missing_thumb
.type call_missing_thumb,%function
.thumb_func
call_missing_thumb:
    push {r4,lr}
    movw r1,#0x5678
    movt r1,#0x1234
    bl missing_thumb
    pop {r4,pc}
.size call_missing_thumb,.-call_missing_thumb
.section .note.GNU-stack,"",%progbits
