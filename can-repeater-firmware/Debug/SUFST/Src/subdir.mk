################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../SUFST/Src/can_repeater.c 

OBJS += \
./SUFST/Src/can_repeater.o 

C_DEPS += \
./SUFST/Src/can_repeater.d 


# Each subdirectory must supply rules for building sources it contributes
SUFST/Src/%.o SUFST/Src/%.su SUFST/Src/%.cyclo: ../SUFST/Src/%.c SUFST/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F105xC -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Adam/OneDrive - University of Southampton/Documents/SUFST/can-repeater-firmware/can-repeater-firmware/SUFST" -I"C:/Users/Adam/OneDrive - University of Southampton/Documents/SUFST/can-repeater-firmware/can-repeater-firmware/SUFST/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-SUFST-2f-Src

clean-SUFST-2f-Src:
	-$(RM) ./SUFST/Src/can_repeater.cyclo ./SUFST/Src/can_repeater.d ./SUFST/Src/can_repeater.o ./SUFST/Src/can_repeater.su

.PHONY: clean-SUFST-2f-Src

