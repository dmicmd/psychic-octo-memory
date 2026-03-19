#include "core/app.h"
#include "platform/board.h"
#include "platform/pca9555_stm32f4.h"
#include "platform/shared_io_stm32f4.h"
#include "platform/uart_stm32f4.h"
#include "platform/usb_composite.h"

static struct app_context g_app;

static const struct app_callbacks callbacks = {
    .cdc_tx = usb_cdc_tx,
    .uart_write = platform_uart_write,
    .activate_io_profile = platform_shared_io_apply_profile,
    .mouse_move = usb_hid_mouse_move,
    .keyboard_report_tx = usb_hid_keyboard_report,
    .generic_report_tx = usb_hid_generic_tx,
    .request_bootloader = board_request_bootloader,
};

int main(void)
{
    board_init();
    platform_uart_init();
    platform_pca9555_init();
    platform_shared_io_init();
    app_init(&g_app, &callbacks);
    platform_uart_bind_app(&g_app);
    platform_pca9555_bind_app(&g_app);
    usb_composite_init(&g_app);

    while (1) {
        usb_composite_poll();
        platform_uart_poll();
        platform_pca9555_poll();
        board_poll();
    }
}
