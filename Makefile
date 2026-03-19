PROJECT := stm32f411-composite-hid
BUILD_DIR := build
TARGET := $(BUILD_DIR)/$(PROJECT)

OPENCM3_DIR ?= lib/libopencm3
LDSCRIPT := linker/stm32f411xE.ld
DEVICE := stm32/f4

PREFIX ?= arm-none-eabi
CC := $(PREFIX)-gcc
OBJCOPY := $(PREFIX)-objcopy
SIZE := $(PREFIX)-size

CSTD := -std=c11
OPT := -Os
WARNINGS := -Wall -Wextra -Wshadow -Wundef -Wdouble-promotion -Wformat=2
MCUFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
DEFS := -DSTM32F411xE
INCLUDES := -Iinclude -I$(OPENCM3_DIR)/include
CFLAGS := $(CSTD) $(OPT) $(WARNINGS) $(MCUFLAGS) $(DEFS) $(INCLUDES) -ffunction-sections -fdata-sections
LDFLAGS := $(MCUFLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -Wl,--print-memory-usage
LDLIBS := -L$(OPENCM3_DIR)/lib -lopencm3_$(subst /,_,$(DEVICE))

SRCS := \
	src/main.c \
	src/core/app.c \
	src/core/modes.c \
	src/core/scenario_registry.c \
	src/platform/stm32f411/startup.c \
	src/platform/stm32f411/board.c \
	src/platform/stm32f411/usb_composite.c \
	src/platform/stm32f411/uart_stm32f4.c \
	src/platform/stm32f411/pca9555_stm32f4.c \
	src/platform/stm32f411/shared_io_stm32f4.c

OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

HOST_CC ?= gcc
HOST_CFLAGS := -std=c11 -Wall -Wextra -Werror -Iinclude
HOST_TEST := $(BUILD_DIR)/host/test_core


.PHONY: all clean libopencm3 test-host

all: $(TARGET).elf $(TARGET).bin

libopencm3:
	$(MAKE) -C $(OPENCM3_DIR)

$(TARGET).elf: $(OBJS) $(OPENCM3_DIR)/lib/libopencm3_$(subst /,_,$(DEVICE)).a | $(BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -Obinary $< $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(HOST_TEST): tests/test_core.c src/core/app.c src/core/modes.c src/core/scenario_registry.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@

test-host: $(HOST_TEST)
	$(HOST_TEST)

clean:
	rm -rf $(BUILD_DIR)
