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

  . = ALIGN(8);

  _heap_start = .;
  _heap_end = _main_stack_pointer_value - 0x1000;

  _end = .;
}
