#ifndef PLATFORM_BOARD_H
#define PLATFORM_BOARD_H

#include <stdint.h>

#define BOARD_BOOTLOADER_MAGIC 0xB00720ADu
#define BOARD_BOOTLOADER_BKP_REGISTER 0u

void board_init(void);
void board_poll(void);
void board_request_bootloader(void);

#endif
