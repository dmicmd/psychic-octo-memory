#include "platform/shared_io_stm32f4.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "platform/pca9555_stm32f4.h"

#define SHARED_GPIO_PORT GPIOB
#define SHARED_TX_SCL GPIO6
#define SHARED_RX_SDA GPIO7
#define SHARED_USART USART1
#define SHARED_I2C I2C1

static enum app_io_profile g_current_profile = APP_IO_PROFILE_NONE;

static void shared_io_configure_idle_pins(void)
{
    gpio_mode_setup(SHARED_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SHARED_TX_SCL | SHARED_RX_SDA);
}

static void shared_io_disable_i2c(void)
{
    platform_pca9555_on_bus_deactivated();
    i2c_peripheral_disable(SHARED_I2C);
}

static void shared_io_disable_uart(void)
{
    usart_disable(SHARED_USART);
}

static void shared_io_configure_uart_pins(void)
{
    gpio_mode_setup(SHARED_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SHARED_TX_SCL | SHARED_RX_SDA);
    gpio_set_output_options(SHARED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SHARED_TX_SCL | SHARED_RX_SDA);
    gpio_set_af(SHARED_GPIO_PORT, GPIO_AF7, SHARED_TX_SCL | SHARED_RX_SDA);
}

static void shared_io_configure_i2c_pins(void)
{
    gpio_mode_setup(SHARED_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, SHARED_TX_SCL | SHARED_RX_SDA);
    gpio_set_output_options(SHARED_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, SHARED_TX_SCL | SHARED_RX_SDA);
    gpio_set_af(SHARED_GPIO_PORT, GPIO_AF4, SHARED_TX_SCL | SHARED_RX_SDA);
}

static uint32_t shared_uart_stopbits(const struct app_line_coding *coding)
{
    return (coding->stop_bits == 2U) ? USART_STOPBITS_2 : USART_STOPBITS_1;
}

static uint32_t shared_uart_parity(const struct app_line_coding *coding)
{
    switch (coding->parity) {
    case APP_UART_PARITY_ODD:
        return USART_PARITY_ODD;
    case APP_UART_PARITY_EVEN:
        return USART_PARITY_EVEN;
    default:
        return USART_PARITY_NONE;
    }
}

static void shared_io_enable_uart_bridge(const struct app_line_coding *coding)
{
    shared_io_disable_i2c();
    shared_io_configure_uart_pins();
    rcc_periph_clock_enable(RCC_USART1);

    usart_disable(SHARED_USART);
    usart_set_baudrate(SHARED_USART, coding->baudrate);
    usart_set_mode(SHARED_USART, USART_MODE_TX_RX);
    usart_set_stopbits(SHARED_USART, shared_uart_stopbits(coding));
    usart_set_databits(SHARED_USART, coding->data_bits);
    usart_set_parity(SHARED_USART, shared_uart_parity(coding));
    usart_set_flow_control(SHARED_USART, USART_FLOWCONTROL_NONE);
    usart_enable_rx_interrupt(SHARED_USART);
    usart_enable(SHARED_USART);
}

static void shared_io_enable_uart9(const struct app_line_coding *coding)
{
    shared_io_disable_i2c();
    shared_io_configure_uart_pins();
    rcc_periph_clock_enable(RCC_USART1);

    usart_disable(SHARED_USART);
    usart_set_baudrate(SHARED_USART, coding->baudrate);
    usart_set_mode(SHARED_USART, USART_MODE_TX_RX);
    usart_set_stopbits(SHARED_USART, USART_STOPBITS_1);
    usart_set_databits(SHARED_USART, 9U);
    usart_set_parity(SHARED_USART, USART_PARITY_NONE);
    usart_set_flow_control(SHARED_USART, USART_FLOWCONTROL_NONE);
    usart_enable_rx_interrupt(SHARED_USART);
    usart_enable(SHARED_USART);
}

static void shared_io_enable_i2c(void)
{
    shared_io_disable_uart();
    shared_io_configure_i2c_pins();
    rcc_periph_clock_enable(RCC_I2C1);

    i2c_peripheral_disable(SHARED_I2C);
    i2c_reset(SHARED_I2C);
    i2c_set_speed(SHARED_I2C, i2c_speed_sm_100k, 42U);
    i2c_set_own_7bit_slave_address(SHARED_I2C, 0U);
    i2c_peripheral_enable(SHARED_I2C);

    platform_pca9555_on_bus_activated();
}

void platform_shared_io_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOB);
    shared_io_configure_idle_pins();
}

void platform_shared_io_apply_profile(enum app_io_profile profile, const struct app_line_coding *coding)
{
    switch (profile) {
    case APP_IO_PROFILE_UART_BRIDGE:
        shared_io_enable_uart_bridge(coding);
        break;
    case APP_IO_PROFILE_UART9_MOUSE:
        shared_io_enable_uart9(coding);
        break;
    case APP_IO_PROFILE_PCA9555_I2C:
        shared_io_enable_i2c();
        break;
    case APP_IO_PROFILE_NONE:
    default:
        shared_io_disable_i2c();
        shared_io_disable_uart();
        shared_io_configure_idle_pins();
        break;
    }

    g_current_profile = profile;
    (void)g_current_profile;
}
