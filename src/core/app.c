#include <stddef.h>
#include <stdint.h>

#include "core/app.h"
#include "core/modes.h"
#include "scenario_registry.h"

static enum app_mode decode_mode(uint8_t raw)
{
    return app_scenario_find((enum app_mode)raw)->id;
}

static void app_send_generic_report(struct app_context *ctx, const uint8_t *report, size_t len)
{
    if (ctx->callbacks.generic_report_tx != 0) {
        ctx->callbacks.generic_report_tx(report, len);
    }
}

static void app_send_status(struct app_context *ctx, uint8_t flags)
{
    uint8_t report[APP_GENERIC_REPORT_SIZE] = {0U};

    report[0] = APP_GENERIC_REPORT_ID;
    report[1] = APP_GENERIC_RSP_STATUS;
    report[2] = (uint8_t)ctx->mode;
    report[3] = flags;
    app_send_generic_report(ctx, report, sizeof(report));
}

static void app_send_scenarios(struct app_context *ctx)
{
    uint8_t report[APP_GENERIC_REPORT_SIZE] = {0U};

    report[0] = APP_GENERIC_REPORT_ID;
    report[1] = APP_GENERIC_RSP_SCENARIOS;
    report[2] = (uint8_t)app_scenario_count();
    report[3] = (uint8_t)ctx->mode;

    for (size_t i = 0; i < app_scenario_count(); ++i) {
        const struct app_scenario_ops *scenario = app_scenario_at(i);
        if (scenario != 0) {
            report[4U + i] = (uint8_t)scenario->id;
        }
    }

    app_send_generic_report(ctx, report, sizeof(report));
}

static void app_send_version(struct app_context *ctx)
{
    uint8_t report[APP_GENERIC_REPORT_SIZE] = {0U};

    report[0] = APP_GENERIC_REPORT_ID;
    report[1] = APP_GENERIC_RSP_VERSION;
    report[2] = APP_FW_VERSION_MAJOR;
    report[3] = APP_FW_VERSION_MINOR;
    report[4] = APP_FW_VERSION_PATCH;
    app_send_generic_report(ctx, report, sizeof(report));
}

void app_init(struct app_context *ctx, const struct app_callbacks *callbacks)
{
    ctx->mode = APP_MODE_UART_BRIDGE;
    ctx->line_coding.baudrate = 115200U;
    ctx->line_coding.stop_bits = 0U;
    ctx->line_coding.parity = APP_UART_PARITY_NONE;
    ctx->line_coding.data_bits = 8U;
    ctx->callbacks = *callbacks;
    ctx->pending_x = 0;
    ctx->x_pending = false;
    ctx->keyboard_pressed_mask = 0U;

    app_set_mode(ctx, APP_MODE_UART_BRIDGE);
}

void app_handle_cdc_rx(struct app_context *ctx, const uint8_t *data, size_t len)
{
    const struct app_scenario_ops *scenario = app_scenario_current(ctx);

    if (scenario->on_cdc_rx != 0) {
        scenario->on_cdc_rx(ctx, data, len);
    }
}

void app_handle_uart_rx_byte(struct app_context *ctx, uint8_t byte)
{
    const struct app_scenario_ops *scenario = app_scenario_current(ctx);

    if (scenario->on_uart_rx_byte != 0) {
        scenario->on_uart_rx_byte(ctx, byte);
    }
}

void app_handle_uart_rx_word(struct app_context *ctx, uint16_t word)
{
    const struct app_scenario_ops *scenario = app_scenario_current(ctx);

    if (scenario->on_uart_rx_word != 0) {
        scenario->on_uart_rx_word(ctx, word);
    }
}

void app_handle_line_coding(struct app_context *ctx, const struct app_line_coding *coding)
{
    ctx->line_coding = *coding;
    if (ctx->callbacks.activate_io_profile != 0) {
        ctx->callbacks.activate_io_profile(app_scenario_current(ctx)->io_profile, &ctx->line_coding);
    }
}

void app_handle_generic_report(struct app_context *ctx, const uint8_t *report, size_t len)
{
    if (len < 2U || report[0] != APP_GENERIC_REPORT_ID) {
        return;
    }

    switch (report[1]) {
    case APP_GENERIC_CMD_SET_MODE:
        if (len < 3U) {
            app_send_status(ctx, APP_STATUS_FLAG_INVALID_COMMAND);
            return;
        }
        app_set_mode(ctx, decode_mode(report[2]));
        break;
    case APP_GENERIC_CMD_ENTER_BOOTLOADER:
        if (ctx->callbacks.request_bootloader != 0) {
            ctx->callbacks.request_bootloader();
        } else {
            app_send_status(ctx, APP_STATUS_FLAG_BOOTLOADER_UNAVAILABLE);
        }
        break;
    case APP_GENERIC_CMD_GET_SCENARIOS:
        app_send_scenarios(ctx);
        break;
    case APP_GENERIC_CMD_GET_VERSION:
        app_send_version(ctx);
        break;
    default:
        app_send_status(ctx, APP_STATUS_FLAG_INVALID_COMMAND);
        break;
    }
}

void app_handle_pca9555_input_state(struct app_context *ctx, uint16_t raw_state)
{
    const struct app_scenario_ops *scenario = app_scenario_current(ctx);

    if (scenario->on_pca9555_state != 0) {
        scenario->on_pca9555_state(ctx, raw_state);
    }
}
