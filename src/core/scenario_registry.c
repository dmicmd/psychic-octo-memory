#include <stddef.h>
#include <stdint.h>

#include "scenario_registry.h"

static const uint8_t keyboard_keycodes[16] = {
    0x04U, 0x1EU, 0x0CU, 0x27U,
    0x1AU, 0x14U, 0x08U, 0x2DU,
    0x1FU, 0x0BU, 0x06U, 0x15U,
    0x17U, 0x1DU, 0x10U, 0x28U,
};

static void bridge_on_cdc_rx(struct app_context *ctx, const uint8_t *data, size_t len)
{
    if (ctx->callbacks.uart_write != 0) {
        ctx->callbacks.uart_write(data, len);
    }
}

static void bridge_on_uart_rx_byte(struct app_context *ctx, uint8_t byte)
{
    if (ctx->callbacks.cdc_tx != 0) {
        ctx->callbacks.cdc_tx(&byte, 1U);
    }
}

static void mouse_on_uart_rx_word(struct app_context *ctx, uint16_t word)
{
    if (ctx->callbacks.mouse_move == 0) {
        return;
    }

    if ((word & 0x0100U) != 0U) {
        ctx->pending_x = (int8_t)(word & 0x00ffU);
        ctx->x_pending = true;
        return;
    }

    if (ctx->x_pending) {
        ctx->callbacks.mouse_move(ctx->pending_x, (int8_t)(word & 0x00ffU));
        ctx->x_pending = false;
    }
}

static void keyboard_emit_report(struct app_context *ctx, uint16_t pressed_mask)
{
    uint8_t report[8] = {0U};
    size_t key_index = 2U;

    for (size_t bit = 0; bit < 16U; ++bit) {
        if ((pressed_mask & (uint16_t)(1U << bit)) == 0U) {
            continue;
        }

        if (key_index >= sizeof(report)) {
            break;
        }

        report[key_index++] = keyboard_keycodes[bit];
    }

    if (ctx->callbacks.keyboard_report_tx != 0) {
        ctx->callbacks.keyboard_report_tx(report, sizeof(report));
    }
}

static void keyboard_on_enter(struct app_context *ctx)
{
    ctx->keyboard_pressed_mask = 0U;
}

static void keyboard_on_leave(struct app_context *ctx)
{
    static const uint8_t empty_report[8] = {0U};

    ctx->keyboard_pressed_mask = 0U;
    if (ctx->callbacks.keyboard_report_tx != 0) {
        ctx->callbacks.keyboard_report_tx(empty_report, sizeof(empty_report));
    }
}

static void keyboard_on_pca9555_state(struct app_context *ctx, uint16_t raw_state)
{
    const uint16_t pressed_mask = (uint16_t)(~raw_state);

    if (ctx->callbacks.keyboard_report_tx == 0) {
        return;
    }

    if (pressed_mask == ctx->keyboard_pressed_mask) {
        return;
    }

    ctx->keyboard_pressed_mask = pressed_mask;
    keyboard_emit_report(ctx, pressed_mask);
}

static const struct app_scenario_ops scenarios[] = {
    {
        .id = APP_MODE_UART_BRIDGE,
        .io_profile = APP_IO_PROFILE_UART_BRIDGE,
        .on_cdc_rx = bridge_on_cdc_rx,
        .on_uart_rx_byte = bridge_on_uart_rx_byte,
    },
    {
        .id = APP_MODE_UART9_MOUSE,
        .io_profile = APP_IO_PROFILE_UART9_MOUSE,
        .on_uart_rx_word = mouse_on_uart_rx_word,
    },
    {
        .id = APP_MODE_PCA9555_KEYBOARD,
        .io_profile = APP_IO_PROFILE_PCA9555_I2C,
        .on_enter = keyboard_on_enter,
        .on_leave = keyboard_on_leave,
        .on_pca9555_state = keyboard_on_pca9555_state,
    },
};

const struct app_scenario_ops *app_scenario_find(enum app_mode id)
{
    for (size_t i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); ++i) {
        if (scenarios[i].id == id) {
            return &scenarios[i];
        }
    }

    return &scenarios[0];
}

const struct app_scenario_ops *app_scenario_current(const struct app_context *ctx)
{
    return app_scenario_find(ctx->mode);
}

size_t app_scenario_count(void)
{
    return sizeof(scenarios) / sizeof(scenarios[0]);
}

const struct app_scenario_ops *app_scenario_at(size_t index)
{
    if (index >= app_scenario_count()) {
        return 0;
    }

    return &scenarios[index];
}
