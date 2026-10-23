/// @file debug_tools.h
/// @brief Header file for universal debugging tools

#ifndef DEBUG_TOOLS_H_
#define DEBUG_TOOLS_H_

#include <stdbool.h>

//=================================================================================================
// Function prototypes
//=================================================================================================

void debugPeripheralsInitializations();

bool debuggingPeripheralsTick(bool TerminationSignal);

#endif // DEBUG_TOOLS_H_
