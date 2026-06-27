#include "can_repeater.h"
#include "main.h"
#include "can_config.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static CAN_Queue_t q_can1_to_can2;
static CAN_Queue_t q_can2_to_can1;

// --- Filter Storage Arrays ---
static uint32_t LIST_1_TO_2[] = FILTER_IDS_CAN1_TO_CAN2;
static size_t CNT_1_TO_2 = sizeof(LIST_1_TO_2) / sizeof(LIST_1_TO_2[0]);

static uint32_t LIST_2_TO_1[] = FILTER_IDS_CAN2_TO_CAN1;
static size_t CNT_2_TO_1 = sizeof(LIST_2_TO_1) / sizeof(LIST_2_TO_1[0]);

#define LED_RX_PULSE_MS     20   // Standard traffic blips for 20ms
#define LED_ERR_PULSE_MS    100  // Errors stay on longer (100ms) so you don't miss them

static uint32_t last_can1_rx_time = 0;
static uint32_t last_can2_rx_time = 0;
static uint32_t last_error_time = 0;
static uint32_t last_heartbeat_time = 0;

static uint32_t dropped_messages = 0;

// ======================================================================
// Filtering and Routing Logic
// ======================================================================
static int compare_ids(const void * a, const void * b) {
    return ( *(uint32_t*)a - *(uint32_t*)b );
}

static int Id_Is_In_List(uint32_t id, uint32_t *list, size_t count) {
    int low = 0, high = count - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (list[mid] == id) return 1;
        if (list[mid] < id) low = mid + 1;
        else high = mid - 1;
    }
    return 0;
}

static int Should_Forward(CAN_RxHeaderTypeDef *header, uint8_t source_bus) {
    uint32_t msg_id = (header->IDE == CAN_ID_STD) ? header->StdId : header->ExtId;
    int found = (source_bus == 1)
        ? Id_Is_In_List(msg_id, LIST_1_TO_2, CNT_1_TO_2)
        : Id_Is_In_List(msg_id, LIST_2_TO_1, CNT_2_TO_1);
#if REPEATER_MODE_ALLOW_LIST == 1
    return found;
#else
    return !found;
#endif
}

// ======================================================================
// Queue management
// ======================================================================
static void Queue_Push(CAN_Queue_t *q, CAN_RxHeaderTypeDef *header, uint8_t *data, uint8_t source) {
    uint16_t next_head = (q->head + 1) % CAN_QUEUE_SIZE;
    if (next_head != q->tail) {
        q->buffer[q->head].header = *header;
        q->buffer[q->head].source_bus = source;
        memcpy(q->buffer[q->head].data, data, 8);
        q->head = next_head;
    } else {
    	HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET);
    	last_error_time = HAL_GetTick();
    	dropped_messages++;
    }
}

static int Queue_Pop(CAN_Queue_t *q, CAN_Message_t *msg) {
    if (q->head == q->tail) return 0;
    *msg = q->buffer[q->tail];
    q->tail = (q->tail + 1) % CAN_QUEUE_SIZE;
    return 1;
}

// ======================================================================
// Application code
// ======================================================================
void Repeater_Init(void) {
    qsort(LIST_1_TO_2, CNT_1_TO_2, sizeof(uint32_t), compare_ids);
    qsort(LIST_2_TO_1, CNT_2_TO_1, sizeof(uint32_t), compare_ids);

    // Boot LEDs
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

    // Hardware Filter Init
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if(HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) Error_Handler();

    sFilterConfig.FilterBank = 14;
    if(HAL_CAN_ConfigFilter(&hcan2, &sFilterConfig) != HAL_OK) Error_Handler();

    if (HAL_CAN_Start(&hcan1) != HAL_OK) Error_Handler();
    if (HAL_CAN_Start(&hcan2) != HAL_OK) Error_Handler();

    // Enable both RX interrupts AND Error interrupts for CAN1
        if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING |
                                                 CAN_IT_ERROR_WARNING |
                                                 CAN_IT_ERROR_PASSIVE |
                                                 CAN_IT_BUSOFF |
                                                 CAN_IT_LAST_ERROR_CODE |
                                                 CAN_IT_ERROR) != HAL_OK) {
            Error_Handler();
        }

        // Enable both RX interrupts AND Error interrupts for CAN2
        if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING |
                                                 CAN_IT_ERROR_WARNING |
                                                 CAN_IT_ERROR_PASSIVE |
                                                 CAN_IT_BUSOFF |
                                                 CAN_IT_LAST_ERROR_CODE |
                                                 CAN_IT_ERROR) != HAL_OK) {
            Error_Handler();
        }
}

static void Process_LEDs(void) {
    uint32_t now = HAL_GetTick();

    // Turn off CAN1 Status LED if time has passed
    if ((now - last_can1_rx_time) > LED_RX_PULSE_MS) {
        HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_RESET);
    }

    // Turn off CAN2 Status LED if time has passed
    if ((now - last_can2_rx_time) > LED_RX_PULSE_MS) {
        HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_RESET);
    }

	// Hardware & Software Error Handling
	// Bypass the sticky HAL variable and read the physical hardware Error Status Register (ESR).
	// Check the specific flags for Error Warning (EWGF), Error Passive (EPVF), and Bus-Off (BOFF).
	uint32_t can1_active_err = hcan1.Instance->ESR & (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF);
	uint32_t can2_active_err = hcan2.Instance->ESR & (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF);

	// If the physical wires break or short out, the hardware flags go HIGH
	if (can1_active_err != 0 || can2_active_err != 0) {
		// Violent 10Hz Strobe (50ms ON, 50ms OFF)
		if (now % 100 < 50) {
			HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_RESET);
		}

		// Clear the sticky HAL software variable so it doesn't mess up our other code later
		hcan1.ErrorCode = HAL_CAN_ERROR_NONE;
		hcan2.ErrorCode = HAL_CAN_ERROR_NONE;
	}
	// Software Queue Drop Warning
	else if ((now - last_error_time) < LED_ERR_PULSE_MS) {
		HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET); // Solid Pulse
	}
	else {
		HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_RESET);
	}
}

static void Broadcast_Heartbeat(void) {
    uint32_t now = HAL_GetTick();

    if ((now - last_heartbeat_time) >= STATUS_INTERVAL_MS) {
        last_heartbeat_time = now;

        CAN_TxHeaderTypeDef txHeader;
        uint32_t txMailbox;
        uint8_t txData[8] = {0};

        txHeader.StdId = STATUS_CAN_ID;
        txHeader.ExtId = 0;
        txHeader.RTR = CAN_RTR_DATA;
        txHeader.IDE = CAN_ID_STD;
        txHeader.DLC = 8;
        txHeader.TransmitGlobalTime = DISABLE;

        // Byte 0: CAN1 -> CAN2 Queue Depth (0 to MAX_QUEUE_SIZE)
        txData[0] = (q_can1_to_can2.head >= q_can1_to_can2.tail) ?
                    (q_can1_to_can2.head - q_can1_to_can2.tail) :
                    (CAN_QUEUE_SIZE - q_can1_to_can2.tail + q_can1_to_can2.head);

        // Byte 1: CAN2 -> CAN1 Queue Depth (0 to MAX_QUEUE_SIZE)
        txData[1] = (q_can2_to_can1.head >= q_can2_to_can1.tail) ?
                    (q_can2_to_can1.head - q_can2_to_can1.tail) :
                    (CAN_QUEUE_SIZE - q_can2_to_can1.tail + q_can2_to_can1.head);

        // Byte 2 & 3: Hardware Error Status Registers (ESR)
        // This tells us exactly if the chip is in Error Warning, Error Passive, or Bus-Off
        txData[2] = hcan1.Instance->ESR & 0xFF;
        txData[3] = hcan2.Instance->ESR & 0xFF;

        // Byte 4 & 5: Node Uptime in seconds (Rolling Counter)
        // Byte 6 & 7: Dropped Messages Counter
        uint32_t uptime_sec = now / 1000;
        txData[4] = (uptime_sec >> 8) & 0xFF;
        txData[5] = uptime_sec & 0xFF;
        txData[6] = (dropped_messages >> 8) & 0xFF;
        txData[7] = dropped_messages & 0xFF;

        // Broadcast to CAN1 if it's not currently locked up
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
            HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &txMailbox);
        }

        // Broadcast to CAN2 if it's not currently locked up
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) > 0) {
            HAL_CAN_AddTxMessage(&hcan2, &txHeader, txData, &txMailbox);
        }
    }
}


void Repeater_Process(void) {
    CAN_Message_t msg;
    CAN_TxHeaderTypeDef txHeader;
    uint32_t txMailbox;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) > 0) {
        if(Queue_Pop(&q_can1_to_can2, &msg)) {
            txHeader.StdId = msg.header.StdId;
            txHeader.ExtId = msg.header.ExtId;
            txHeader.RTR = msg.header.RTR;
            txHeader.IDE = msg.header.IDE;
            txHeader.DLC = msg.header.DLC;
            txHeader.TransmitGlobalTime = DISABLE;
            HAL_CAN_AddTxMessage(&hcan2, &txHeader, msg.data, &txMailbox);
        }
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        if(Queue_Pop(&q_can2_to_can1, &msg)) {
            txHeader.StdId = msg.header.StdId;
            txHeader.ExtId = msg.header.ExtId;
            txHeader.RTR = msg.header.RTR;
            txHeader.IDE = msg.header.IDE;
            txHeader.DLC = msg.header.DLC;
            txHeader.TransmitGlobalTime = DISABLE;
            HAL_CAN_AddTxMessage(&hcan1, &txHeader, msg.data, &txMailbox);
        }
    }
    Process_LEDs();
    Broadcast_Heartbeat();
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (hcan->Instance == CAN1) {
    	HAL_GPIO_WritePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin, GPIO_PIN_SET);
    	last_can1_rx_time = HAL_GetTick();
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            if (Should_Forward(&rxHeader, 1)) {
                Queue_Push(&q_can1_to_can2, &rxHeader, rxData, 1);
            }
        }
    } else if (hcan->Instance == CAN2) {
    	HAL_GPIO_WritePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin, GPIO_PIN_SET);
    	last_can2_rx_time = HAL_GetTick();
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            if (Should_Forward(&rxHeader, 2)) {
                Queue_Push(&q_can2_to_can1, &rxHeader, rxData, 2);
            }
        }
    }
}
