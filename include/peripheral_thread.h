/// @file peripheral_thread.h

#ifndef PERIPHERAL_THREAD_H
#define PERIPHERAL_THREAD_H

#include <atomic>


void initializePeripheralThreadModule();

void peripheralThreadStart();

void peripheralThreadExit();

bool debuggingPeripheralsTick(bool TerminationSignal);

void peripheralsInitializations();

#endif // PERIPHERAL_THREAD_H
