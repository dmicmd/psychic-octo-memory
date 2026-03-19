# STM32F411 composite USB firmware scaffold

This repository contains a portable firmware scaffold for **STM32F411** using **libopencm3** and **GNU Make**.

## Features

- Composite USB device:
  - USB CDC ACM (virtual COM port)
  - HID keyboard
  - HID mouse
  - HID generic (vendor-defined)
- Generic HID command channel for:
  - selecting application scenarios
  - querying the available scenario list
  - querying firmware version
  - requesting bootloader handoff
- Portable application/core logic isolated from STM32/libopencm3 specifics.
- Shared-pin transport design:
  - **PB6/PB7 are shared** between UART and I2C
  - firmware activates **either UART or I2C**, never both simultaneously
  - scenario switching drives peripheral ownership of these pins automatically
- Implemented scenarios:
  1. **UART bridge**: bridge the physical UART on PB6/PB7 to the USB CDC ACM port, inheriting baud rate, parity and stop bits from the virtual COM port line coding.
  2. **UART9 mouse**: use a 9-bit UART stream on PB6/PB7 to generate mouse movement:
     - bit 9 = 1 -> low 8 bits are `X`
     - bit 9 = 0 -> low 8 bits are `Y`
  3. **PCA9555 keyboard**: switch PB6/PB7 into I2C mode, poll a PCA9555, treat all 16 lines as active-low keys, and emit HID keyboard reports.

## Layout

- `include/core`, `src/core`: platform-independent application logic.
- `src/core/scenario_registry.c`: table-driven scenario registry; new scenarios are added primarily by registering one more scenario descriptor.
- `include/platform`, `src/platform/stm32f411`: STM32F411 + libopencm3 glue.
- `src/platform/stm32f411/shared_io_stm32f4.c`: owner/pinmux management for the shared PB6/PB7 transport pins.
- `tests`: host-side checks for the portable core.
- `linker`: linker script.
- `.gitlab-ci.yml`: CI pipeline for reproducible builds.

## HID generic protocol

### Output report `0x01`

- byte 0: report id = `0x01`
- byte 1: command id
- byte 2: argument 0

Commands:

- `0x01`: set mode
  - `arg0 = 0x00`: UART bridge
  - `arg0 = 0x01`: UART9 mouse
  - `arg0 = 0x02`: PCA9555 keyboard
- `0x02`: enter bootloader
  - firmware writes `BOARD_BOOTLOADER_MAGIC` to `RTC_BKPXR(0)` and resets the MCU
- `0x03`: get scenarios
- `0x04`: get firmware version

### Input report `0x01`

The generic HID IN report is 32 bytes and uses the following response types:

- `0x80` (`STATUS`)
  - byte 2: current mode
  - byte 3: status flags
- `0x81` (`SCENARIOS`)
  - byte 2: scenario count
  - byte 3: current mode
  - bytes 4..: scenario IDs
- `0x82` (`VERSION`)
  - byte 2: major
  - byte 3: minor
  - byte 4: patch

## Shared PB6/PB7 transport rules

- `UART bridge` and `UART9 mouse` own PB6/PB7 as `USART1 TX/RX`.
- `PCA9555 keyboard` owns PB6/PB7 as `I2C1 SCL/SDA`.
- The shared-IO layer is the only platform component allowed to retarget PB6/PB7.
- New scenarios should declare the required IO profile instead of manually reconfiguring pins.

## PCA9555 keyboard mode

- Device address defaults to `0x20` in the current board glue.
- Both 8-bit ports are treated as **active-low inputs**.
- All 16 inputs are mapped to HID keycodes.
- The keycodes are intentionally arbitrary for now and can be replaced later with a real layout.
- The current implementation sends standard 8-byte boot-keyboard reports, so only the first six simultaneously pressed keys are transmitted.

## Build

### Prerequisites

- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `make`
- `git`
- `python3` (optional, for helper tooling)

### Fetch libopencm3

```bash
mkdir -p lib
if [ ! -d lib/libopencm3 ]; then
  git clone --depth 1 https://github.com/libopencm3/libopencm3.git lib/libopencm3
fi
make -C lib/libopencm3
```

### Build firmware

```bash
make
```

Outputs are written to `build/`.

### Run portable core tests

```bash
make test-host
```

## Notes

- The transport/application split is designed so the logic in `src/core` can be reused with a different STM32 or even another MCU family by replacing only the platform layer.
- The table-driven scenario registry is there specifically so adding a new scenario mostly means adding one new descriptor plus any scenario-specific platform source, instead of editing switch-cascades all over the codebase.
