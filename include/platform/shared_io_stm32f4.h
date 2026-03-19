#ifndef PLATFORM_SHARED_IO_STM32F4_H
#define PLATFORM_SHARED_IO_STM32F4_H

#include "core/app.h"

void platform_shared_io_init(void);
void platform_shared_io_apply_profile(enum app_io_profile profile, const struct app_line_coding *coding);

#endif
