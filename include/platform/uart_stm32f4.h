#ifndef PLATFORM_UART_STM32F4_H
#define PLATFORM_UART_STM32F4_H

#include <stddef.h>
#include <stdint.h>

#include "core/app.h"

void platform_uart_init(void);
void platform_uart_write(const uint8_t *data, size_t len);
void platform_uart_poll(void);
void platform_uart_bind_app(struct app_context *app);

#endif
