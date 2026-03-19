#include "platform/pca9555_stm32f4.h"

#include <stdint.h>

#include <libopencm3/stm32/i2c.h>

#define PCA9555_I2C I2C1
#define PCA9555_ADDR 0x20U
#define PCA9555_REG_INPUT_PORT0 0x00U
#define PCA9555_REG_POLARITY_PORT0 0x04U
#define PCA9555_REG_CONFIG_PORT0 0x06U
#define PCA9555_POLL_DIVIDER 2000U

static struct app_context *g_app;
static uint32_t g_poll_divider;
static int g_bus_ready;

static void pca9555_write_pair(uint8_t start_register, uint8_t first, uint8_t second)
{
    const uint8_t payload[] = {start_register, first, second};
    i2c_transfer7(PCA9555_I2C, PCA9555_ADDR, payload, sizeof(payload), 0, 0);
}

static uint16_t pca9555_read_inputs(void)
{
    const uint8_t start_register = PCA9555_REG_INPUT_PORT0;
    uint8_t values[2] = {0xffU, 0xffU};

    i2c_transfer7(PCA9555_I2C, PCA9555_ADDR, &start_register, 1U, values, sizeof(values));
    return (uint16_t)values[0] | ((uint16_t)values[1] << 8);
}

void platform_pca9555_init(void)
{
    g_bus_ready = 0;
    g_poll_divider = 0U;
}

void platform_pca9555_bind_app(struct app_context *app)
{
    g_app = app;
}

void platform_pca9555_on_bus_activated(void)
{
    pca9555_write_pair(PCA9555_REG_POLARITY_PORT0, 0x00U, 0x00U);
    pca9555_write_pair(PCA9555_REG_CONFIG_PORT0, 0xffU, 0xffU);
    g_bus_ready = 1;
    g_poll_divider = 0U;
}

void platform_pca9555_on_bus_deactivated(void)
{
    g_bus_ready = 0;
    g_poll_divider = 0U;
}

void platform_pca9555_poll(void)
{
    if (g_app == 0 || g_app->mode != APP_MODE_PCA9555_KEYBOARD || g_bus_ready == 0) {
        g_poll_divider = 0U;
        return;
    }

    if (++g_poll_divider < PCA9555_POLL_DIVIDER) {
        return;
    }

    g_poll_divider = 0U;
    app_handle_pca9555_input_state(g_app, pca9555_read_inputs());
}
