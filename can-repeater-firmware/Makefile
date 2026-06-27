TARGET = can-repeater-firmware
BUILD_DIR = build

PREFIX = arm-none-eabi-
CC  = $(PREFIX)gcc
AS  = $(PREFIX)gcc
CP  = $(PREFIX)objcopy
SZ  = $(PREFIX)size

CPU = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft

C_DEFS = -DUSE_HAL_DRIVER -DSTM32F105xC \
-DUSE_HAL_DRIVER \
-DSTM32F105xC

C_INCLUDES = \
-I  Core/Inc \
-I  Drivers/STM32F1xx_HAL_Driver/Inc \
-I  Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-I  Drivers/CMSIS/Device/ST/STM32F1xx/Include \
-I  Drivers/CMSIS/Include \
-I  SUFST/Inc \
-ICore/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
-IDrivers/CMSIS/Include

C_SOURCES = \
Core/Src/main.c \
Core/Src/gpio.c \
Core/Src/can.c \
Core/Src/usart.c \
Core/Src/stm32f1xx_it.c \
Core/Src/stm32f1xx_hal_msp.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_usart.c \
Core/Src/system_stm32f1xx.c \
Core/Src/sysmem.c \
Core/Src/syscalls.c

ASM_SOURCES = Core/Startup/startup_stm32f105rbtx.s

LDSCRIPT = STM32F105XX_FLASH.ld
LDFLAGS = $(CPU) -specs=nano.specs -T$(LDSCRIPT) -lc -lm -lnosys \
  -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

CFLAGS = $(CPU) $(C_DEFS) $(C_INCLUDES) -Wall -fdata-sections -ffunction-sections

OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	$(SZ) $<

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CPU) -x assembler-with-cpp -o $@ $<

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $@

flash: $(BUILD_DIR)/$(TARGET).bin
	st-flash write $< 0x8000000

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all flash clean
