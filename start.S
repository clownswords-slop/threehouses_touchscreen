.section .text.start,"ax"
.global _start
.hidden _start
_start:
    b 1f
    .word __nx_mod0 - _start
1:  ret
.section .rodata.mod0,"a"
.align 2
.global __nx_mod0
.hidden __nx_mod0
.hidden __bss_start__
.hidden __bss_end__
.hidden _DYNAMIC
__nx_mod0:
    .ascii "MOD0"
    .word _DYNAMIC - __nx_mod0
    .word __bss_start__ - __nx_mod0
    .word __bss_end__ - __nx_mod0
    .word 0
    .word 0
    .word __nx_module_runtime - __nx_mod0
.section .bss.modobj,"aw",@nobits
.align 3
.hidden __nx_module_runtime
__nx_module_runtime:
    .space 0x100
