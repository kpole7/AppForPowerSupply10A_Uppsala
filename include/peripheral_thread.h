/// @file peripheral_thread.h

#ifndef PERIPHERAL_THREAD_H
#define PERIPHERAL_THREAD_H

#include <atomic>


void initializePeripheralThreadModule();

void peripheralThreadStart();

void peripheralThreadExit();

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they be terminated
bool peripheralsTick(bool TerminationSignal);

void peripheralsInitializations();

#endif // PERIPHERAL_THREAD_H
