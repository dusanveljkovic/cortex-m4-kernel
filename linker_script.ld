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
    *(.vector_table)
  } > FLASH

  .text : 
  {
    *(.text)
    *(.text*)
  } > FLASH
  
  .rodata : 
  {
    *(.rodata)
    *(.rodata*)
  } > FLASH

  .data : 
  {
    _vma_data_start = .;
    *(.data)
    *(.data*)
    _vma_data_end = .;
  } > SRAM AT> FLASH

  _lma_data_start = LOADADDR(.data);

  .data : 
  {
    *(.bss)
    *(.bss*)
    *(COMMON)
  } > SRAM 
}
