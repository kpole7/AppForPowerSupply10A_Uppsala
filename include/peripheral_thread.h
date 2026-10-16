/// @file peripheral_thread.h

#ifndef PERIPHERAL_THREAD_H
#define PERIPHERAL_THREAD_H

//.................................................................................................
// Global variables
//.................................................................................................

extern bool AwakeRefreshWholeGui;

extern bool AwakeRefreshDebugInfoGui;

//.................................................................................................
// Function prototypes
//.................................................................................................

void initializePeripheralThreadModule();

void peripheralThreadStart();

void peripheralThreadExit();

bool debuggingPeripheralsTick(bool TerminationSignal);

void peripheralsInitializations();

#endif // PERIPHERAL_THREAD_H
