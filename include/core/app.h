#ifndef CORE_APP_H
#define CORE_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_FW_VERSION_MAJOR 0U
#define APP_FW_VERSION_MINOR 4U
#define APP_FW_VERSION_PATCH 0U
#define APP_GENERIC_REPORT_SIZE 32U

enum app_mode {
    APP_MODE_UART_BRIDGE = 0,
    APP_MODE_UART9_MOUSE = 1,
    APP_MODE_PCA9555_KEYBOARD = 2,
};

enum app_io_profile {
    APP_IO_PROFILE_NONE = 0,
    APP_IO_PROFILE_UART_BRIDGE,
    APP_IO_PROFILE_UART9_MOUSE,
    APP_IO_PROFILE_PCA9555_I2C,
};

enum app_uart_parity {
    APP_UART_PARITY_NONE = 0,
    APP_UART_PARITY_ODD = 1,
    APP_UART_PARITY_EVEN = 2,
};

enum app_generic_report_id {
    APP_GENERIC_REPORT_ID = 0x01,
};

enum app_generic_command {
    APP_GENERIC_CMD_SET_MODE = 0x01,
    APP_GENERIC_CMD_ENTER_BOOTLOADER = 0x02,
    APP_GENERIC_CMD_GET_SCENARIOS = 0x03,
    APP_GENERIC_CMD_GET_VERSION = 0x04,
};

enum app_generic_response {
    APP_GENERIC_RSP_STATUS = 0x80,
    APP_GENERIC_RSP_SCENARIOS = 0x81,
    APP_GENERIC_RSP_VERSION = 0x82,
};

enum app_status_flag {
    APP_STATUS_FLAG_NONE = 0x00,
    APP_STATUS_FLAG_INVALID_COMMAND = 0x01,
    APP_STATUS_FLAG_BOOTLOADER_UNAVAILABLE = 0x02,
};

struct app_line_coding {
    uint32_t baudrate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
};

struct app_callbacks {
    void (*cdc_tx)(const uint8_t *data, size_t len);
    void (*uart_write)(const uint8_t *data, size_t len);
    void (*activate_io_profile)(enum app_io_profile profile, const struct app_line_coding *coding);
    void (*mouse_move)(int8_t dx, int8_t dy);
    void (*keyboard_report_tx)(const uint8_t *report, size_t len);
    void (*generic_report_tx)(const uint8_t *report, size_t len);
    void (*request_bootloader)(void);
};

struct app_context {
    enum app_mode mode;
    struct app_line_coding line_coding;
    struct app_callbacks callbacks;
    int8_t pending_x;
    bool x_pending;
    uint16_t keyboard_pressed_mask;
};

void app_init(struct app_context *ctx, const struct app_callbacks *callbacks);
void app_handle_cdc_rx(struct app_context *ctx, const uint8_t *data, size_t len);
void app_handle_uart_rx_byte(struct app_context *ctx, uint8_t byte);
void app_handle_uart_rx_word(struct app_context *ctx, uint16_t word);
void app_handle_line_coding(struct app_context *ctx, const struct app_line_coding *coding);
void app_handle_generic_report(struct app_context *ctx, const uint8_t *report, size_t len);
void app_handle_pca9555_input_state(struct app_context *ctx, uint16_t raw_state);

#endif
