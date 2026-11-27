.syntax unified
.text
.arm
.balign 4
.global probe_read_tp_arm
.type probe_read_tp_arm, %function
probe_read_tp_arm:
    mrc p15, 0, r0, c13, c0, 3
    bx lr
.size probe_read_tp_arm, .-probe_read_tp_arm
.thumb
.balign 2
.global probe_read_tp_thumb
.type probe_read_tp_thumb, %function
.thumb_func
probe_read_tp_thumb:
    mrc p15, 0, r0, c13, c0, 3
    bx lr
.size probe_read_tp_thumb, .-probe_read_tp_thumb
.section .note.GNU-stack,"",%progbits
