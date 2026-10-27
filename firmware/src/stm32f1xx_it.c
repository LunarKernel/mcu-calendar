/* 中断入口。按 app/app.h 的分层约定，中断里只做累加时基一类的极少几件事。
 * 没在这里定义的中断走启动文件的 Default_Handler（死循环），调试器停下来能看到停在哪。 */
#include "stm32f1xx_hal.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}
