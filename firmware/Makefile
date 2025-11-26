# Makefile для прошивки Avalon 1126 Pro
# Quaxis Solo Miner Firmware

# Компилятор (cross-compile для ARM)
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

# Директории
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Флаги компиляции
CFLAGS = -Wall -Wextra -Werror
CFLAGS += -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard
CFLAGS += -I$(INC_DIR)
CFLAGS += -DNDEBUG

# Флаги для отладки
DEBUG_CFLAGS = -Wall -Wextra -g -O0 -I$(INC_DIR)

# Флаги линковки
LDFLAGS = -T linker.ld
LDFLAGS += -nostartfiles

# Исходные файлы
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Цель по умолчанию
TARGET = quaxis-a1126

.PHONY: all clean debug

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET).bin

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: all

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "Размер прошивки:"
	@ls -lh $@

clean:
	rm -rf $(BUILD_DIR)

# Зависимости
$(BUILD_DIR)/main.o: $(INC_DIR)/config.h $(INC_DIR)/protocol.h $(INC_DIR)/a1126_driver.h $(INC_DIR)/network.h
$(BUILD_DIR)/sha256.o: $(INC_DIR)/sha256.h
$(BUILD_DIR)/a1126_driver.o: $(INC_DIR)/a1126_driver.h $(INC_DIR)/spi.h $(INC_DIR)/sha256.h
$(BUILD_DIR)/network.o: $(INC_DIR)/network.h $(INC_DIR)/protocol.h
$(BUILD_DIR)/spi.o: $(INC_DIR)/spi.h
