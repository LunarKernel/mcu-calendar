/* STM32F101xE：512 KB Flash，48 KB SRAM。
 * 芯片丝印未定前的默认目标：F103xE 是它的超集，按它链接的固件在两种芯片上都能跑。 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 48K
}

INCLUDE stm32f1_sections.ld
