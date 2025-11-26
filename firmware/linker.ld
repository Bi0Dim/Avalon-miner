/**
 * @file linker.ld
 * @brief Linker script для прошивки Avalon 1126 Pro
 * 
 * Предназначен для ARM Cortex-M4 микроконтроллера.
 * Требует адаптации под конкретную плату.
 */

/* Размер памяти (адаптировать под конкретный MCU) */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Entry point */
ENTRY(Reset_Handler)

/* Определение стека */
_stack_size = 0x2000;  /* 8KB */
_stack_end = ORIGIN(RAM) + LENGTH(RAM);
_stack_start = _stack_end - _stack_size;

SECTIONS
{
    /* Вектор прерываний и код */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Инициализированные данные */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    
    _sidata = LOADADDR(.data);

    /* Неинициализированные данные (BSS) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Стек */
    .stack :
    {
        . = ALIGN(8);
        . = . + _stack_size;
        . = ALIGN(8);
    } > RAM

    /* Проверка переполнения памяти */
    ASSERT(. <= _stack_start, "RAM overflow")
}

/* Функции для startup кода */
PROVIDE(_estack = _stack_end);
