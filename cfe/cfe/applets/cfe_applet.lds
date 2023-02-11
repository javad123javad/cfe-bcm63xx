OUTPUT_ARCH(mips)
ENTRY(__start)
SECTIONS
{
  . = 0x80000000;
  .text : 
    {
     _ftext = . ;
    *(.init)
     eprol  =  .;
    *(.text)
    *(.fini)
    *(.rodata)
    *(.rodata.str1.4)
    *(.gnu.linkonce.r.*)
     _etext  =  .;
   }

  .data :
   {
    _fdata = ALIGN(16) ;
    *(.data)
    CONSTRUCTORS
    . = ALIGN(16);
    _gp = . + 0x8000;
    *(.sdata)
   }
   . = ALIGN(16);
   _edata  =  .;
   _fbss = .;
  .sbss : {
    *(.sbss)
    *(.scommon)
  }
  .bss : {
    *(.bss)
    *(COMMON)
  }
  . = ALIGN(16);
   _end = .;
}
