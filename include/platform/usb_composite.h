#ifndef PLATFORM_USB_COMPOSITE_H
#define PLATFORM_USB_COMPOSITE_H

#include <stddef.h>
#include <stdint.h>

#include "core/app.h"

void usb_composite_init(struct app_context *app);
void usb_composite_poll(void);
void usb_cdc_tx(const uint8_t *data, size_t len);
void usb_hid_mouse_move(int8_t dx, int8_t dy);
void usb_hid_keyboard_report(const uint8_t *report, size_t len);
void usb_hid_generic_tx(const uint8_t *report, size_t len);

#endif
