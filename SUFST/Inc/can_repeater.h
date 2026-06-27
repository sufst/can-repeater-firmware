/*
 * can_repeater.h
 *
 *  Created on: Nov 25, 2025
 *      Author: Adam
 */

#ifndef INC_CAN_REPEATER_H_
#define INC_CAN_REPEATER_H_

#include "can_config.h"
#include <stdlib.h>

// Buffer config
#define CAN_QUEUE_SIZE 512 // large buffer to absorb larger bursts when 1Mbps >> 500kbps

// --- Flash Storage Settings ---
#define MAX_FILTER_IDS      64          // Max IDs per list in RAM
#define CONFIG_FLASH_ADDR   0x0801F800  // Page 63 (Last page of STM32F105RB 128KB Flash)
#define CONFIG_MAGIC_WORD   0xABCD1234  // Used to check if Flash contains valid data

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
