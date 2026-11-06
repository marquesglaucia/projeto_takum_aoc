OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
  . = 0x80000000;
  .text   : { *(.text.init) *(.text*) }
  . = ALIGN(0x1000);
  .tohost : { *(.tohost) }
  .rodata : { *(.rodata*) *(.srodata*) }
  .data   : { *(.data*) *(.sdata*) }
  .bss    : { *(.bss*) *(.sbss*) *(COMMON) }
  . = ALIGN(16);
  . = . + 0x4000;
  _stack_top = .;
}
