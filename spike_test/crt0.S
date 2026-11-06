# Runtime minimo bare-metal p/ Spike (HTIF): habilita FPU e vetor, chama main, sai via tohost.
    .section .text.init
    .globl _start
_start:
    li   t0, 0x6600          # mstatus.FS = 3, mstatus.VS = 3
    csrs mstatus, t0
    la   sp, _stack_top
    call main
    slli a0, a0, 1
    ori  a0, a0, 1           # (codigo << 1) | 1  => Spike encerra com este codigo
    la   t0, tohost
    sd   a0, 0(t0)
1:  j    1b

    .section .tohost, "aw", @progbits
    .align 6
    .globl tohost
tohost:   .dword 0
    .align 6
    .globl fromhost
fromhost: .dword 0
