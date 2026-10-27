/* STM32F103xE：512 KB Flash，64 KB SRAM。丝印确认是 F103 后才用。 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
}

INCLUDE stm32f1_sections.ld
