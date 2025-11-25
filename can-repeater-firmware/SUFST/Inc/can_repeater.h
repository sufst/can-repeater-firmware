/*
 * can_repeater.h
 *
 *  Created on: Nov 25, 2025
 *      Author: Adam
 */

#ifndef INC_CAN_REPEATER_H_
#define INC_CAN_REPEATER_H_

#include "stm32f1xx_hal.h"
#include "can_config.h"

// Configuration
#define CAN_QUEUE_SIZE 64

// Data Structures
typedef struct {
	CAN_RxHeaderTypeDef header;
	uint8_t data[8];
	uint8_t source_bus; // 1 = CAN1, 2 = CAN2
} CAN_Message_t;

typedef struct {
	CAN_Message_t buffer[CAN_QUEUE_SIZE];
	volatile uint16_t head;
	volatile uint16_t tail;
} CAN_Queue_t;

// Function Prototypes
void Repeater_Init(void);
void Repeater_Process(void);

#endif /* INC_CAN_REPEATER_H_ */
