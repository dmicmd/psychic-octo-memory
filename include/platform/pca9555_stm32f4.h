#ifndef PLATFORM_PCA9555_STM32F4_H
#define PLATFORM_PCA9555_STM32F4_H

#include "core/app.h"

void platform_pca9555_init(void);
void platform_pca9555_bind_app(struct app_context *app);
void platform_pca9555_on_bus_activated(void);
void platform_pca9555_on_bus_deactivated(void);
void platform_pca9555_poll(void);

#endif
