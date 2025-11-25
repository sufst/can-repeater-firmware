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
#define CAN_APB1_CLOCK_HZ 8000000
// #define CAN_APB1_CLOCK_HZ 36000000

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
#define REPEATER_MODE_ALLOW_LIST 1

// List 1: Messages arriving on CAN1, targeted towards CAN2
#define FILTER_IDS_CAN1_TO_CAN2  { \
/* e.g.
    0x123, \
    0x0A0  \

    backslashes are needed for compiler to ignore new line

*/	0x123, \
	0x0A0  \
}

// List 2: Messages arriving on CAN2, targeted towards CAN1
#define FILTER_IDS_CAN2_TO_CAN1  { \
/* e.g.
    0x123, \
    0x0A0  \

    backslashes are needed for compiler to ignore new line

*/	0x123, \
	0x0A0  \
}


#endif /* INC_CAN_CONFIG_H_ */
