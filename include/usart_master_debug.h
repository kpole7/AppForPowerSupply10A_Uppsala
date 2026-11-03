/// @file usart_master_debug.h
/// @brief Debugging utilities for the USART master module

#ifndef USART_MASTER_DEBUG_H_
#define USART_MASTER_DEBUG_H_

#include <cstdint>

extern char* CurrentCommandTextPtr;

extern std::uint16_t CurrentCommandTextLength;

extern uint16_t CurrentCommandTextLength;

bool usartMasterDebuggingTick(bool TerminationSignal, int PressedKey);

#endif // USART_MASTER_DEBUG_H_