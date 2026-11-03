/// @file debug_tools.h
/// @brief Header file for universal debugging tools

#ifndef DEBUG_TOOLS_H_
#define DEBUG_TOOLS_H_

#include <chrono>
#include <cstdint>

#include "usart_master_debug.h"

extern std::uint16_t LongestTimeTicsBehindSchedule;

//=================================================================================================
// Function prototypes
//=================================================================================================

void debugPeripheralsInitializations();

bool debuggingToolsTick(bool TerminationSignal);

std::string formatElapsedTime(std::chrono::milliseconds::rep TimeSinceReference);

#endif // DEBUG_TOOLS_H_
