/*
 * can_repeater.c
 *
 *  Created on: Nov 25, 2025
 *      Author: Adam
 */

#include "can_repeater.h"
#include "main.h"
#include "can_config.h"
#include <string.h>

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static CAN_Queue_t q_can1_to_can2;
static CAN_Queue_t q_can2_to_can1;


// Filter Storage
static uint32_t LIST_1_TO_2[] = FILTER_IDS_CAN1_TO_CAN2;
static const size_t CNT_1_TO_2 = sizeof(LIST_1_TO_2) / sizeof(LIST_1_TO_2[0]);

static uint32_t LIST_2_TO_1[] = FILTER_IDS_CAN2_TO_CAN1;
static const size_t CNT_2_TO_1 = sizeof(LIST_2_TO_1) / sizeof(LIST_2_TO_1[0]);

// Compare function for qsort
static int compare_ids(const void * a, const void * b) {
    return ( *(uint32_t*)a - *(uint32_t*)b );
}

// Binary Search
static int Id_Is_In_List(uint32_t id, uint32_t *list, size_t count) {
    int low = 0;
    int high = count - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (list[mid] == id) {
            return 1; // Found
        }
        if (list[mid] < id)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return 0; // Not Found
}

static void Queue_Push(CAN_Queue_t *q, CAN_RxHeaderTypeDef *header, uint8_t *data, uint8_t source) {
    uint16_t next_head = (q->head + 1) % CAN_QUEUE_SIZE;
    if (next_head != q->tail) {
        q->buffer[q->head].header = *header;
        q->buffer[q->head].source_bus = source;

        uint8_t *dst = q->buffer[q->head].data;
        for (int i = 0; i < 8; i++) {
        	dst[i] = data[i];
        }

        q->head = next_head;
    } else {
    	// BUFFER FULL
    	HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET);
    }
}

static int Queue_Pop(CAN_Queue_t *q, CAN_Message_t *msg) {
    if (q->head == q->tail) return 0;
    *msg = q->buffer[q->tail];
    q->tail = (q->tail + 1) % CAN_QUEUE_SIZE;
    return 1;
}

static int Should_Forward(CAN_RxHeaderTypeDef *header, uint8_t source_bus) {
	// If only want to forward specific IDs check header->StdId here

    uint32_t msg_id;
    int found = 0;

    // Unify STD or EXT IDs
    msg_id = (header->IDE == CAN_ID_STD) ? header->StdId : header->ExtId;

    // Select list based on source bus
    if (source_bus == 1) {
        found = Id_Is_In_List(msg_id, LIST_1_TO_2, CNT_1_TO_2);
    } else {
        found = Id_Is_In_List(msg_id, LIST_2_TO_1, CNT_2_TO_1);
    }

    // Decision
    #if REPEATER_MODE_ALLOW_LIST == 1
        return found;
    #else
        return !found;
	#endif
}

void Repeater_Init(void) {
	//Sort CAN IDs for filtering
	qsort(LIST_1_TO_2, CNT_1_TO_2, sizeof(uint32_t), compare_ids);
	qsort(LIST_2_TO_1, CNT_2_TO_1, sizeof(uint32_t), compare_ids);


	//Flash LEDS to confirm running code
	HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_SET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_SET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_RESET);


	CAN_FilterTypeDef sFilterConfig;
    // Configure CAN1 Filter
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14; // Start of CAN2 filters

    if(HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) {
        // Filter configuration Error
        Error_Handler();
    }

    // Configure CAN2 Filter
    sFilterConfig.FilterBank = 14;

    if(HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig) != HAL_OK) {
        // Filter configuration Error
        Error_Handler();
    }

    // Start CAN peripherals
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_CAN_Start(&hcan2) != HAL_OK) {
        Error_Handler();
    }

    // Activate CAN RX notification
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        Error_Handler();
    }
}

void Repeater_Process(void) {
    CAN_Message_t msg;
    CAN_TxHeaderTypeDef txHeader;
    uint32_t txMailbox;

    // Process messages from CAN1 to CAN2

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) > 0) {
        if(Queue_Pop(&q_can1_to_can2, &msg)) {
			// Prepare TX header
			txHeader.StdId = msg.header.StdId;
			txHeader.ExtId = msg.header.ExtId;
			txHeader.RTR = msg.header.RTR;
			txHeader.IDE = msg.header.IDE;
			txHeader.DLC = msg.header.DLC;
			txHeader.TransmitGlobalTime = DISABLE;

			// Transmit on CAN2
			HAL_CAN_AddTxMessage(&hcan2, &txHeader, msg.data, &txMailbox);
        }
    }

    // Process messages from CAN2 to CAN1
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        if(Queue_Pop(&q_can2_to_can1, &msg)) {
			// Prepare TX header
			txHeader.StdId = msg.header.StdId;
			txHeader.ExtId = msg.header.ExtId;
			txHeader.RTR = msg.header.RTR;
			txHeader.IDE = msg.header.IDE;
			txHeader.DLC = msg.header.DLC;
			txHeader.TransmitGlobalTime = DISABLE;

			// Transmit on CAN1
			HAL_CAN_AddTxMessage(&hcan1, &txHeader, msg.data, &txMailbox);
        }

    }
}

// Interrupt Callbacks

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (hcan->Instance == CAN1) {
        // Message received on CAN1
    	HAL_GPIO_TogglePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin);
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            if (Should_Forward(&rxHeader, 1)) {
                Queue_Push(&q_can1_to_can2, &rxHeader, rxData, 1);
            }
        }
    } else if (hcan->Instance == CAN2) {
        // Message received on CAN2
    	HAL_GPIO_TogglePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin);
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            if (Should_Forward(&rxHeader, 2)) {
                Queue_Push(&q_can2_to_can1, &rxHeader, rxData, 2);
            }
        }
    }
}
