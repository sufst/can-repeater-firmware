/*
 * can_config.h
 *
 *  Created on: Nov 25, 2025
 *      Author: Adam
 */

#ifndef INC_CAN_CONFIG_H_
#define INC_CAN_CONFIG_H_

#include "main.h"

// CLOCK SETTING - set to match APB1
//#define CAN_APB1_CLOCK_HZ 8000000
#define CAN_APB1_CLOCK_HZ 36000000

// BAUD RATE SELECTION
// Options: 125000, 250000, 500000, 1000000

#define CAN1_TARGET_BAUD 1000000
#define CAN2_TARGET_BAUD 500000

// Automatic Parameter Calculation DO NOT EDIT BELOW

// Helper Macros to calculate Prescaler based on Time Quanta (TQ)
// We use a standard 16 TQ split (Sync=1, BS1=13, BS2=2) => Sample Point = 87.5%
// Exception: 8MHz clock cannot do 16TQ for 1Mbit, so we use 8TQ there.

// --- Calculation for 8MHz Clock ---
#if (CAN_APB1_CLOCK_HZ == 8000000)
    #if (CAN1_TARGET_BAUD == 1000000)
        #define CAN1_PRESCALER 1
        #define CAN1_BS1       CAN_BS1_5TQ
        #define CAN1_BS2       CAN_BS2_2TQ // Total 8 TQ
    #else
        #define CAN1_PRESCALER (8000000 / (CAN1_TARGET_BAUD * 16))
        #define CAN1_BS1       CAN_BS1_13TQ
        #define CAN1_BS2       CAN_BS2_2TQ // Total 16 TQ
    #endif

    #if (CAN2_TARGET_BAUD == 1000000)
        #define CAN2_PRESCALER 1
        #define CAN2_BS1       CAN_BS1_5TQ
        #define CAN2_BS2       CAN_BS2_2TQ
    #else
        #define CAN2_PRESCALER (8000000 / (CAN2_TARGET_BAUD * 16))
        #define CAN2_BS1       CAN_BS1_13TQ
        #define CAN2_BS2       CAN_BS2_2TQ
    #endif

// --- Calculation for 36MHz Clock (Standard Connectivity Line) ---
#elif (CAN_APB1_CLOCK_HZ == 36000000)
    // 36MHz / 18TQ = 2MHz quantum.
    // 18 TQ split: Sync=1, BS1=14, BS2=3 => 83.3% Sample
    #define CAN1_PRESCALER (36000000 / (CAN1_TARGET_BAUD * 18))
    #define CAN1_BS1       CAN_BS1_14TQ
    #define CAN1_BS2       CAN_BS2_3TQ

    #define CAN2_PRESCALER (36000000 / (CAN2_TARGET_BAUD * 18))
    #define CAN2_BS1       CAN_BS1_14TQ
    #define CAN2_BS2       CAN_BS2_3TQ

#else
    #error "Unsupported Clock Frequency in can_config.h"
#endif


// Filter Configuration

// 1 = Allow list
// 0 = Block list
#define REPEATER_MODE_ALLOW_LIST 0

// List 1: Messages arriving on CAN1, targeted towards CAN2
#define FILTER_IDS_CAN1_TO_CAN2  {}

// List 2: Messages arriving on CAN2, targeted towards CAN1
#define FILTER_IDS_CAN2_TO_CAN1  {}


// CAN-Repeater's own messages
#define STATUS_CAN_ID       0x7E1
#define STATUS_INTERVAL_MS  1000  // Send status every 1000 milliseconds

// --- Dynamic Configuration Protocol ---
#define CONFIG_CAN_ID       0x7E0
#define CMD_CLEAR_LIST      0x00
#define CMD_ADD_ID          0x01
#define CMD_REMOVE_ID       0x02
#define CMD_SET_STATE       0x03

#define TARGET_LIST_CAN1_TO_2 0x01
#define TARGET_LIST_CAN2_TO_1 0x02

// Repeater States
#define STATE_NORMAL        0x00
#define STATE_BLOCK_ALL     0x01

// --- Flash Storage Settings ---
#define MAX_FILTER_IDS      64
#define CONFIG_FLASH_ADDR   0x0801F800
#define CONFIG_MAGIC_WORD   0xABCD1234


#endif /* INC_CAN_CONFIG_H_ */
