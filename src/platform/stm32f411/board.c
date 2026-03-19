#include "platform/board.h"

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/rtc.h>

static void board_enable_backup_register_access(void)
{
    rcc_periph_clock_enable(RCC_PWR);
    pwr_disable_backup_domain_write_protect();

    if ((RCC_CSR & RCC_CSR_LSIRDY) == 0U) {
        RCC_CSR |= RCC_CSR_LSION;
        while ((RCC_CSR & RCC_CSR_LSIRDY) == 0U) {
        }
    }

    if ((RCC_BDCR & RCC_BDCR_RTCEN) == 0U) {
        RCC_BDCR &= ~(RCC_BDCR_RTCSEL_MASK << RCC_BDCR_RTCSEL_SHIFT);
        RCC_BDCR |= (RCC_BDCR_RTCSEL_LSI << RCC_BDCR_RTCSEL_SHIFT);
        RCC_BDCR |= RCC_BDCR_RTCEN;
    }
}

void board_init(void)
{
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_OTGFS);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
    gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);

    nvic_set_priority(NVIC_OTG_FS_IRQ, 1U << 4);
    nvic_enable_irq(NVIC_OTG_FS_IRQ);
    nvic_set_priority(NVIC_USART1_IRQ, 2U << 4);
    nvic_enable_irq(NVIC_USART1_IRQ);
}

void board_poll(void)
{
}

void board_request_bootloader(void)
{
    board_enable_backup_register_access();
    RTC_BKPXR(BOARD_BOOTLOADER_BKP_REGISTER) = BOARD_BOOTLOADER_MAGIC;
    pwr_enable_backup_domain_write_protect();
    scb_reset_system();
}
