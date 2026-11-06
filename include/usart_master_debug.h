/// @file usart_master_debug.h
/// @brief Debugging utilities for the USART master module

#ifndef USART_MASTER_DEBUG_H_
#define USART_MASTER_DEBUG_H_

#include "modified_rstl_master.h"
#include <cstdint>

extern const char *CurrentCommandTextPtr;

/// @brief The maximum number of bytes expected in the response from the USART slave device
extern uint16_t ExpectedResponseLimit;

void usartMasterDebuggingInitialize();

void usartMasterDebuggingTick(int PressedKey);

void printStateChanges(low_level_usart *UsartPtr, rstl_master_fsm_state FsmState, const char *UserEventName);

#endif // USART_MASTER_DEBUG_H_