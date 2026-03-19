#include "platform/uart_stm32f4.h"

#include <libopencm3/stm32/usart.h>

#include "core/app.h"

#define PLATFORM_UART USART1

static struct app_context *g_app;

void platform_uart_init(void)
{
}

void platform_uart_write(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        usart_send_blocking(PLATFORM_UART, data[i]);
    }
}

void platform_uart_poll(void)
{
}

void usart1_isr(void)
{
    if ((USART_SR(PLATFORM_UART) & USART_SR_RXNE) == 0U || g_app == 0) {
        return;
    }

    if (g_app->mode == APP_MODE_UART9_MOUSE) {
        app_handle_uart_rx_word(g_app, usart_recv(PLATFORM_UART));
    } else {
        app_handle_uart_rx_byte(g_app, (uint8_t)usart_recv(PLATFORM_UART));
    }
}

void platform_uart_bind_app(struct app_context *app)
{
    g_app = app;
}
