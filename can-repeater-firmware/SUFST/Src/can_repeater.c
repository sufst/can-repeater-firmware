#include "can_repeater.h"
#include "main.h"
#include "can_config.h"
#include <string.h>

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static CAN_Queue_t q_can1_to_can2;
static CAN_Queue_t q_can2_to_can1;

// --- Filter Storage Arrays ---
static uint32_t INIT_LIST_1_TO_2[] = FILTER_IDS_CAN1_TO_CAN2;
static uint32_t INIT_LIST_2_TO_1[] = FILTER_IDS_CAN2_TO_CAN1;

static uint32_t LIST_1_TO_2[MAX_FILTER_IDS];
static size_t CNT_1_TO_2 = 0;

static uint32_t LIST_2_TO_1[MAX_FILTER_IDS];
static size_t CNT_2_TO_1 = 0;

// ======================================================================
// Flash memory Helper Functions
// ======================================================================
static void Load_Config_From_Flash(void) {
    uint32_t *flash_ptr = (uint32_t *)CONFIG_FLASH_ADDR;

    if (*flash_ptr == CONFIG_MAGIC_WORD) {
        flash_ptr++;
        CNT_1_TO_2 = *flash_ptr++;
        if (CNT_1_TO_2 > MAX_FILTER_IDS) CNT_1_TO_2 = MAX_FILTER_IDS;
        for (size_t i = 0; i < CNT_1_TO_2; i++) LIST_1_TO_2[i] = *flash_ptr++;

        CNT_2_TO_1 = *flash_ptr++;
        if (CNT_2_TO_1 > MAX_FILTER_IDS) CNT_2_TO_1 = MAX_FILTER_IDS;
        for (size_t i = 0; i < CNT_2_TO_1; i++) LIST_2_TO_1[i] = *flash_ptr++;
    } else {
        CNT_1_TO_2 = sizeof(INIT_LIST_1_TO_2) / sizeof(INIT_LIST_1_TO_2[0]);
        if (CNT_1_TO_2 > MAX_FILTER_IDS) CNT_1_TO_2 = MAX_FILTER_IDS;
        memcpy(LIST_1_TO_2, INIT_LIST_1_TO_2, CNT_1_TO_2 * sizeof(uint32_t));

        CNT_2_TO_1 = sizeof(INIT_LIST_2_TO_1) / sizeof(INIT_LIST_2_TO_1[0]);
        if (CNT_2_TO_1 > MAX_FILTER_IDS) CNT_2_TO_1 = MAX_FILTER_IDS;
        memcpy(LIST_2_TO_1, INIT_LIST_2_TO_1, CNT_2_TO_1 * sizeof(uint32_t));
    }
}

static void Save_Config_To_Flash(void) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = CONFIG_FLASH_ADDR;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        uint32_t current_addr = CONFIG_FLASH_ADDR;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, CONFIG_MAGIC_WORD);
        current_addr += 4;
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, CNT_1_TO_2);
        current_addr += 4;
        for (size_t i = 0; i < CNT_1_TO_2; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, LIST_1_TO_2[i]);
            current_addr += 4;
        }
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, CNT_2_TO_1);
        current_addr += 4;
        for (size_t i = 0; i < CNT_2_TO_1; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, LIST_2_TO_1[i]);
            current_addr += 4;
        }
    }
    HAL_FLASH_Lock();
}

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
    int found = 0;

    if (source_bus == 1) found = Id_Is_In_List(msg_id, LIST_1_TO_2, CNT_1_TO_2);
    else found = Id_Is_In_List(msg_id, LIST_2_TO_1, CNT_2_TO_1);

    #if REPEATER_MODE_ALLOW_LIST == 1
        return found;
    #else
        return !found;
    #endif
}

static void Process_Config_Message(uint8_t *data) {
    uint8_t cmd = data[0];
    uint8_t target_list = data[1];
    uint32_t id = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];

    uint32_t *list = (target_list == TARGET_LIST_CAN1_TO_2) ? LIST_1_TO_2 : LIST_2_TO_1;
    size_t *cnt = (target_list == TARGET_LIST_CAN1_TO_2) ? &CNT_1_TO_2 : &CNT_2_TO_1;

    if (cmd == CMD_CLEAR_LIST) *cnt = 0;
    else if (cmd == CMD_ADD_ID) {
        if (*cnt < MAX_FILTER_IDS && !Id_Is_In_List(id, list, *cnt)) {
            list[*cnt] = id;
            (*cnt)++;
        }
    }
    else if (cmd == CMD_REMOVE_ID) {
        for (size_t i = 0; i < *cnt; i++) {
            if (list[i] == id) {
                for (size_t j = i; j < *cnt - 1; j++) list[j] = list[j + 1];
                (*cnt)--;
                break;
            }
        }
    }

    qsort(LIST_1_TO_2, CNT_1_TO_2, sizeof(uint32_t), compare_ids);
    qsort(LIST_2_TO_1, CNT_2_TO_1, sizeof(uint32_t), compare_ids);
    Save_Config_To_Flash();
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
    Load_Config_From_Flash();

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

    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();
    if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();
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
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    if (hcan->Instance == CAN1) {
        HAL_GPIO_TogglePin(STAT1_LED_GPIO_Port, STAT1_LED_Pin);
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            uint32_t msg_id = (rxHeader.IDE == CAN_ID_STD) ? rxHeader.StdId : rxHeader.ExtId;
            if (msg_id == CONFIG_CAN_ID) {
                Process_Config_Message(rxData);
            } else if (Should_Forward(&rxHeader, 1)) {
                Queue_Push(&q_can1_to_can2, &rxHeader, rxData, 1);
            }
        }
    } else if (hcan->Instance == CAN2) {
        HAL_GPIO_TogglePin(STAT2_LED_GPIO_Port, STAT2_LED_Pin);
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
            uint32_t msg_id = (rxHeader.IDE == CAN_ID_STD) ? rxHeader.StdId : rxHeader.ExtId;
            if (msg_id == CONFIG_CAN_ID) {
                Process_Config_Message(rxData);
            } else if (Should_Forward(&rxHeader, 2)) {
                Queue_Push(&q_can2_to_can1, &rxHeader, rxData, 2);
            }
        }
    }
}
