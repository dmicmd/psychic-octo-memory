#include <stdint.h>

#include "core/modes.h"
#include "scenario_registry.h"

static void app_send_generic_status(struct app_context *ctx, uint8_t flags)
{
    uint8_t report[APP_GENERIC_REPORT_SIZE] = {0U};

    report[0] = APP_GENERIC_REPORT_ID;
    report[1] = APP_GENERIC_RSP_STATUS;
    report[2] = (uint8_t)ctx->mode;
    report[3] = flags;

    if (ctx->callbacks.generic_report_tx != 0) {
        ctx->callbacks.generic_report_tx(report, sizeof(report));
    }
}

void app_set_mode(struct app_context *ctx, enum app_mode mode)
{
    const struct app_scenario_ops *previous = app_scenario_current(ctx);
    const struct app_scenario_ops *next = app_scenario_find(mode);

    if (previous != 0 && previous->id != next->id && previous->on_leave != 0) {
        previous->on_leave(ctx);
    }

    ctx->mode = next->id;
    ctx->pending_x = 0;
    ctx->x_pending = false;

    if (ctx->callbacks.activate_io_profile != 0) {
        ctx->callbacks.activate_io_profile(next->io_profile, &ctx->line_coding);
    }

    if (next->on_enter != 0) {
        next->on_enter(ctx);
    }

    app_send_generic_status(ctx, APP_STATUS_FLAG_NONE);
}
