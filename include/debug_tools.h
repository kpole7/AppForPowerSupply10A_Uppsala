/// @file debug_tools.h
/// @brief Header file for universal debugging tools

#ifndef DEBUG_TOOLS_H_
#define DEBUG_TOOLS_H_

#include <chrono>
#include <stdbool.h>
#include <stdint.h>
#include <string>

extern uint16_t LongestTimeTicsBehindSchedule;

//=================================================================================================
// Function prototypes
//=================================================================================================

void debugPeripheralsInitializations();

bool debuggingPeripheralsTick(bool TerminationSignal);

std::string formatElapsedTime(std::chrono::milliseconds::rep TimeSinceReference);

#endif // DEBUG_TOOLS_H_
