ENTRY(_start)
MEMORY
{
  ROM (rx)  : ORIGIN = 0x20010000, LENGTH = 2M
  RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 512K
}
SECTIONS
{
  .text : { *(.text*) *(.rodata*) } > ROM
  .data :
  {
    _data_start = .;
    *(.data*)
    _data_end = .;
  } > RAM AT > ROM
  _data_lma = LOADADDR(.data);
  .bss :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    _bss_end = .;
  } > RAM
  .stack (NOLOAD) :
  {
    . = ALIGN(16);
    . += 0x2000;
    _stack_top = .;
  } > RAM
  _end = .;
}
