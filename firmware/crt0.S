.section .text
.global _start
_start:
    la   sp, _stack_top
    la   t0, _data_lma
    la   t1, _data_start
    la   t2, _data_end
data_loop:
    bge  t1, t2, data_done
    lw   t3, 0(t0)
    sw   t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j    data_loop
data_done:
    la   t0, _bss_start
    la   t1, _bss_end
bss_loop:
    bge  t0, t1, bss_done
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    bss_loop
bss_done:
    call main
1:
    j 1b
