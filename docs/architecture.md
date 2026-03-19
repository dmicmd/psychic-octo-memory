# Architecture

## Goals

- Keep business logic portable.
- Keep STM32F411/libopencm3 specifics confined to the platform layer.
- Make USB mode control and firmware introspection explicit through a vendor-defined HID interface.
- Make new scenarios additive by using a registry + IO-profile model.

## Layering

### `src/core`

Pure application logic:

- scenario registry and dispatch
- CDC-to-UART bridge behavior
- UART9-to-mouse behavior
- PCA9555 input-word to HID keyboard report mapping
- interpretation of HID generic commands and responses

This layer depends only on `include/core/*` and callback interfaces. It does not include libopencm3 headers.

### `src/platform/stm32f411`

Board/MCU implementation:

- clocks and GPIO configuration
- USB descriptors and endpoint handlers
- USART1 handling on shared PB6/PB7
- I2C1/PCA9555 polling on shared PB6/PB7
- shared pinmux / transport ownership for PB6/PB7
- startup code and vector table

Porting to another STM32 should primarily require replacing this directory and possibly the linker script.

## Scenario model

Each scenario is represented by a descriptor in `src/core/scenario_registry.c`.
A descriptor declares:

- stable scenario ID
- required IO profile
- optional enter/leave hooks
- optional handlers for CDC RX, UART byte RX, UART word RX, and PCA9555 state updates

As a result, adding a new scenario usually means:

1. add one descriptor to the scenario registry
2. implement only the handlers that scenario needs
3. if necessary, add one new platform input source and route it into the core

The rest of the firmware continues to use generic dispatch.

## Shared PB6/PB7 transport model

PB6/PB7 are intentionally treated as a **single shared transport resource**.

- `APP_IO_PROFILE_UART_BRIDGE` -> USART1 on PB6/PB7
- `APP_IO_PROFILE_UART9_MOUSE` -> USART1 on PB6/PB7
- `APP_IO_PROFILE_PCA9555_I2C` -> I2C1 on PB6/PB7

The shared-IO platform layer owns:

- peripheral enable/disable
- PB6/PB7 alternate-function switching
- profile-specific low-level reconfiguration

Scenarios only request an IO profile; they do not manipulate GPIO or peripheral registers directly.

## Data flow

### Scenario 0: UART bridge

1. Host configures the CDC ACM port.
2. USB CDC line coding is translated into `struct app_line_coding`.
3. Core asks the platform to activate the UART bridge IO profile.
4. CDC RX bytes are forwarded to UART TX.
5. UART RX bytes are forwarded to CDC IN.

### Scenario 1: UART9 mouse

1. Host selects the mode through HID generic output report `0x01`.
2. Core asks the platform to activate the UART9 IO profile on PB6/PB7.
3. Each received UART word is interpreted as either X or Y based on bit 9.
4. Once an X/Y pair is assembled, the core emits a mouse movement callback.

### Scenario 2: PCA9555 keyboard

1. Host selects the PCA9555 keyboard scenario through HID generic.
2. Core asks the platform to activate the I2C IO profile on PB6/PB7.
3. STM32F411 platform code polls the PCA9555 over I2C.
4. Raw 16-bit active-low input state is passed into the portable core.
5. Core maps pressed bits to HID keycodes and emits a boot-keyboard report.

### Bootloader request

1. Host sends the HID generic `ENTER_BOOTLOADER` command.
2. Core delegates the request through a platform callback.
3. STM32F411 platform code writes `BOARD_BOOTLOADER_MAGIC` to `RTC_BKPXR(0)` and performs a system reset.
4. A future bootloader can inspect the backup register early after reset and decide whether to stay in boot mode.

### Firmware introspection

- `GET_SCENARIOS` returns the list of currently compiled-in scenario IDs.
- `GET_VERSION` returns a simple semantic version tuple from the portable core.

## Extensibility

Suggested next steps:

- replace the temporary random PCA9555 keymap with a board layout file
- add debouncing or interrupt-driven PCA9555 scanning
- add host-side utilities for HID generic control
- add per-scenario configuration payloads in generic HID commands
- optionally add CMake generation alongside Make
