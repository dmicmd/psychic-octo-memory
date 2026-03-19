/* Minimal linker script for STM32F411xE devices (512K Flash, 128K RAM). */
MEMORY
{
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(ram) + LENGTH(ram);

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
    } > rom

    .ARM.extab :
    {
        *(.ARM.extab*)
    } > rom

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > rom

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > ram AT > rom

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > ram

    . = ALIGN(4);
    _end = .;
}
