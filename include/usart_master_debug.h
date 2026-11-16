/// @file usart_master_debug.h
/// @brief Debugging utilities for the USART master module

#ifndef USART_MASTER_DEBUG_H_
#define USART_MASTER_DEBUG_H_

#include "modified_rstl_master.h"
#include <cstdint>

void usartMasterDebuggingInitialize();

void usartMasterDebuggingTick(int PressedKey);

void printStateChanges(low_level_usart *UsartPtr, rstl_master_fsm_state FsmState, const char *UserEventName);

void printTopLevelChanges(uint8_t StateIndex1, uint8_t StateIndex2);

#endif // USART_MASTER_DEBUG_H_