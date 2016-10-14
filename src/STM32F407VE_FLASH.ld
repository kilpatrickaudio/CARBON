/*
 * CARBON Linker Script
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2015: Kilpatrick Audio
 *
 * This file is part of CARBON.
 *
 * CARBON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CARBON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CARBON.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

ENTRY(Reset_Handler)

/* top of user stack */
//_estack = 0x2001FFFF;  /* top of RAM */
_estack = 0x1000FFFF;  /* top of CCMRAM */
_Min_Heap_Size = 0x200;  /* heap size  */
_Min_Stack_Size = 0x400; /* stack size */

/* memory layout */
MEMORY
{
VECTOR (rx)     : ORIGIN = 0x8000000, LENGTH = 16K
FLASH (rx)      : ORIGIN = 0x8004000, LENGTH = 512K-16K
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
CCMRAM (rw)      : ORIGIN = 0x10000000, LENGTH = 64K
}

SECTIONS
{
  /* startup code in flash */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* startup code */
    . = ALIGN(4);
  } >VECTOR
  
  /* program code and data */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text (code) */
    *(.text*)          /* .text* (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* global symbols */
  } >FLASH

  /* lookup table (const) memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)  /* .rodata sections */
    *(.rodata*)  /* .rodata* sections */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* init data  */
  _sidata = LOADADDR(.data);

  /* init RAM data */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* global symbol at start */
    *(.data)           /* .data */
    *(.data*)          /* .data* */

    . = ALIGN(4);
    _edata = .;        /* global symbol at end */
/* put init data in RAM */
/*  } >RAM AT> FLASH  */
/* put init data in CCMRAM */
  } >CCMRAM AT> FLASH 

  _siccmram = LOADADDR(.ccmram);

  /* CCM-RAM section */
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;  /* global symbol at start */
    *(.ccmram)
    *(.ccmram*)
    
    . = ALIGN(4);
    _eccmram = .;  /* global symbol at end */
  } >CCMRAM AT> FLASH

  
  /* uninit data section */
  . = ALIGN(4);
  .bss :
  {
    /* used by startup to init the .bss secion */
    _sbss = .;  /* global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;  /* global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* user heap */
  ._user_heap_stack :
  {
    . = ALIGN(4);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(4);
  } >RAM

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
