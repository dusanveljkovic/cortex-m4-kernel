_main_stack_pointer_value = 0x20020000;

MEMORY 
{
  FLASH(rx) : ORIGIN = 0x08000000, LENGTH = 512K
  SRAM(rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS 
{
  .vector_table : 
  {
    . = ALIGN(4);
    *(.vector_table)
  } > FLASH

  .text : 
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    . = ALIGN(4);
  } > FLASH
  
  .rodata : 
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  .data : 
  {
    . = ALIGN(4);
    _vma_data_start = .;

    *(.data)
    *(.data*)

    . = ALIGN(4);
    _vma_data_end = .;
  } > SRAM AT> FLASH

  _lma_data_start = LOADADDR(.data);

  .bss : 
  {
    *(.bss)
    *(.bss*)
    *(COMMON)
  } > SRAM 

  /* HEAP
   * MPU region: 
   * base = 0x20010000
   * size = 64KiB
   * end = 0x20020000
   */

  . = 0x2001000;
  .heap (NOLOAD) :
  {
    . = ALIGN(64K);

    _heap_start = .;

    . += 64K;

    _heap_end = .;
  } > SRAM

  _end = .;
}
