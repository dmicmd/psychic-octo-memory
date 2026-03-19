#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/app.h"

static struct {
    enum app_io_profile last_profile;
    struct app_line_coding last_profile_coding;
    unsigned int profile_apply_count;
    uint8_t last_uart_tx[16];
    size_t last_uart_tx_len;
    int8_t mouse_dx;
    int8_t mouse_dy;
    unsigned int bootloader_requests;
    uint8_t last_generic_report[APP_GENERIC_REPORT_SIZE];
    size_t last_generic_report_len;
    uint8_t last_keyboard_report[8];
    size_t last_keyboard_report_len;
} state;

static void reset_state(void)
{
    memset(&state, 0, sizeof(state));
}

static void test_uart_write(const uint8_t *data, size_t len)
{
    assert(len <= sizeof(state.last_uart_tx));
    memcpy(state.last_uart_tx, data, len);
    state.last_uart_tx_len = len;
}

static void test_activate_io_profile(enum app_io_profile profile, const struct app_line_coding *coding)
{
    state.last_profile = profile;
    state.last_profile_coding = *coding;
    state.profile_apply_count += 1U;
}

static void test_mouse_move(int8_t dx, int8_t dy)
{
    state.mouse_dx = dx;
    state.mouse_dy = dy;
}

static void test_keyboard_report(const uint8_t *report, size_t len)
{
    assert(len == sizeof(state.last_keyboard_report));
    memcpy(state.last_keyboard_report, report, len);
    state.last_keyboard_report_len = len;
}

static void test_generic_report(const uint8_t *report, size_t len)
{
    assert(len == sizeof(state.last_generic_report));
    memcpy(state.last_generic_report, report, len);
    state.last_generic_report_len = len;
}

static void test_request_bootloader(void)
{
    state.bootloader_requests += 1U;
}

int main(void)
{
    struct app_context ctx;
    const struct app_callbacks callbacks = {
        .uart_write = test_uart_write,
        .activate_io_profile = test_activate_io_profile,
        .mouse_move = test_mouse_move,
        .keyboard_report_tx = test_keyboard_report,
        .generic_report_tx = test_generic_report,
        .request_bootloader = test_request_bootloader,
    };

    reset_state();
    app_init(&ctx, &callbacks);
    assert(ctx.mode == APP_MODE_UART_BRIDGE);
    assert(state.last_profile == APP_IO_PROFILE_UART_BRIDGE);
    assert(state.last_profile_coding.baudrate == 115200U);
    assert(state.last_generic_report_len == APP_GENERIC_REPORT_SIZE);
    assert(state.last_generic_report[1] == APP_GENERIC_RSP_STATUS);
    assert(state.last_generic_report[2] == APP_MODE_UART_BRIDGE);
    assert(state.last_generic_report[3] == APP_STATUS_FLAG_NONE);

    {
        const uint8_t get_scenarios[] = {APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_GET_SCENARIOS};
        app_handle_generic_report(&ctx, get_scenarios, sizeof(get_scenarios));
        assert(state.last_generic_report[1] == APP_GENERIC_RSP_SCENARIOS);
        assert(state.last_generic_report[2] == 3U);
        assert(state.last_generic_report[3] == APP_MODE_UART_BRIDGE);
        assert(state.last_generic_report[4] == APP_MODE_UART_BRIDGE);
        assert(state.last_generic_report[5] == APP_MODE_UART9_MOUSE);
        assert(state.last_generic_report[6] == APP_MODE_PCA9555_KEYBOARD);
    }

    {
        const uint8_t get_version[] = {APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_GET_VERSION};
        app_handle_generic_report(&ctx, get_version, sizeof(get_version));
        assert(state.last_generic_report[1] == APP_GENERIC_RSP_VERSION);
        assert(state.last_generic_report[2] == APP_FW_VERSION_MAJOR);
        assert(state.last_generic_report[3] == APP_FW_VERSION_MINOR);
        assert(state.last_generic_report[4] == APP_FW_VERSION_PATCH);
    }

    {
        const uint8_t set_mouse[] = {APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_SET_MODE, APP_MODE_UART9_MOUSE};
        app_handle_generic_report(&ctx, set_mouse, sizeof(set_mouse));
        assert(ctx.mode == APP_MODE_UART9_MOUSE);
        assert(state.last_profile == APP_IO_PROFILE_UART9_MOUSE);
        assert(state.last_generic_report[1] == APP_GENERIC_RSP_STATUS);
        assert(state.last_generic_report[2] == APP_MODE_UART9_MOUSE);
    }

    app_handle_uart_rx_word(&ctx, 0x0105U);
    app_handle_uart_rx_word(&ctx, 0x00FDU);
    assert(state.mouse_dx == 5);
    assert(state.mouse_dy == -3);

    {
        const uint8_t enter_bootloader[] = {APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_ENTER_BOOTLOADER};
        app_handle_generic_report(&ctx, enter_bootloader, sizeof(enter_bootloader));
        assert(state.bootloader_requests == 1U);
    }

    {
        const struct app_line_coding coding = {
            .baudrate = 230400U,
            .stop_bits = 2U,
            .parity = APP_UART_PARITY_EVEN,
            .data_bits = 8U,
        };
        const uint8_t bridge_bytes[] = {'O', 'K'};

        app_handle_line_coding(&ctx, &coding);
        assert(state.last_profile == APP_IO_PROFILE_UART9_MOUSE);
        assert(state.last_profile_coding.baudrate == 230400U);

        app_handle_generic_report(&ctx,
            (const uint8_t[]){APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_SET_MODE, APP_MODE_UART_BRIDGE},
            3U);
        app_handle_cdc_rx(&ctx, bridge_bytes, sizeof(bridge_bytes));
        assert(state.last_uart_tx_len == sizeof(bridge_bytes));
        assert(memcmp(state.last_uart_tx, bridge_bytes, sizeof(bridge_bytes)) == 0);
        assert(state.last_profile == APP_IO_PROFILE_UART_BRIDGE);
        assert(state.last_profile_coding.baudrate == 230400U);
        assert(state.last_profile_coding.stop_bits == 2U);
        assert(state.last_profile_coding.parity == APP_UART_PARITY_EVEN);
    }

    {
        const uint8_t set_keyboard[] = {APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_SET_MODE, APP_MODE_PCA9555_KEYBOARD};
        app_handle_generic_report(&ctx, set_keyboard, sizeof(set_keyboard));
        assert(ctx.mode == APP_MODE_PCA9555_KEYBOARD);
        assert(state.last_profile == APP_IO_PROFILE_PCA9555_I2C);
        assert(state.last_generic_report[1] == APP_GENERIC_RSP_STATUS);
        assert(state.last_generic_report[2] == APP_MODE_PCA9555_KEYBOARD);

        app_handle_pca9555_input_state(&ctx, 0xFFFEU);
        assert(state.last_keyboard_report_len == sizeof(state.last_keyboard_report));
        assert(state.last_keyboard_report[2] == 0x04U);

        app_handle_pca9555_input_state(&ctx, 0xFFFDU);
        assert(state.last_keyboard_report[2] == 0x1EU);

        app_handle_generic_report(&ctx,
            (const uint8_t[]){APP_GENERIC_REPORT_ID, APP_GENERIC_CMD_SET_MODE, APP_MODE_UART_BRIDGE},
            3U);
        assert(state.last_keyboard_report[2] == 0x00U);
        assert(state.last_profile == APP_IO_PROFILE_UART_BRIDGE);
    }

    {
        const uint8_t invalid[] = {APP_GENERIC_REPORT_ID, 0x7FU};
        app_handle_generic_report(&ctx, invalid, sizeof(invalid));
        assert(state.last_generic_report[1] == APP_GENERIC_RSP_STATUS);
        assert(state.last_generic_report[3] == APP_STATUS_FLAG_INVALID_COMMAND);
    }

    return 0;
}
