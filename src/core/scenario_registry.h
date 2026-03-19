#ifndef CORE_SCENARIO_REGISTRY_H
#define CORE_SCENARIO_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "core/app.h"

struct app_scenario_ops {
    enum app_mode id;
    enum app_io_profile io_profile;
    void (*on_enter)(struct app_context *ctx);
    void (*on_leave)(struct app_context *ctx);
    void (*on_cdc_rx)(struct app_context *ctx, const uint8_t *data, size_t len);
    void (*on_uart_rx_byte)(struct app_context *ctx, uint8_t byte);
    void (*on_uart_rx_word)(struct app_context *ctx, uint16_t word);
    void (*on_pca9555_state)(struct app_context *ctx, uint16_t raw_state);
};

const struct app_scenario_ops *app_scenario_find(enum app_mode id);
const struct app_scenario_ops *app_scenario_current(const struct app_context *ctx);
size_t app_scenario_count(void);
const struct app_scenario_ops *app_scenario_at(size_t index);

#endif
