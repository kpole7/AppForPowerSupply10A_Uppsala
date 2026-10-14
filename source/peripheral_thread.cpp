/// @file peripheral_thread.cpp
/// @brief Implements a separate thread and timing functionality for peripheral operations.

#include <FL/Fl.H>
#include <thread>
#include <chrono>
#include <iostream>

#include "peripheral_thread.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define PERIPHERAL_THREAD_LOOP_DURATION 50 // milliseconds

#define SHUT_DOWN_TIMEOUT 1000  // milliseconds

#define SHUT_DOWN_LOOP_DELAY 20 // milliseconds

#define SHUT_DOWN_COUNT_DOWN (SHUT_DOWN_TIMEOUT / SHUT_DOWN_LOOP_DELAY)

//...............................................................................................
// Local variables
//...............................................................................................

/// This flag is set when the application is being closed
static std::atomic<bool> DoClosePeripheralThread;

/// This flag is set when the peripheral thread has been stopped
static std::atomic<bool> PeripheralThreadIsStopped;

static std::thread peripheralThread;

static std::chrono::high_resolution_clock::time_point PeripheralThreadLoopStart;
static std::chrono::high_resolution_clock::time_point TimeNow;
static int64_t PeripheralThreadTimeInMilliseconds;

//.................................................................................................
// Local function prototypes
//.................................................................................................

static void peripheralThreadTiming();
static void peripheralThreadHandler();

//.................................................................................................
// Function definitions
//.................................................................................................

void initializePeripheralThreadModule() {
	atomic_store_explicit(&DoClosePeripheralThread, false, std::memory_order_release);
	atomic_store_explicit(&PeripheralThreadIsStopped, true, std::memory_order_release);
}

/// This function initializes the module variables and launches a new thread to support peripherals
void peripheralThreadStart() {
	atomic_store_explicit(&DoClosePeripheralThread, false, std::memory_order_release);
	atomic_store_explicit(&PeripheralThreadIsStopped, false, std::memory_order_release);
	peripheralThread = std::thread(peripheralThreadHandler);
}

/// This function is called by FLTK onMainWindowCloseCallback event handler
void peripheralThreadExit() {
	if (atomic_load_explicit(&PeripheralThreadIsStopped, std::memory_order_acquire)) {
		return;
	}

	atomic_store_explicit(&DoClosePeripheralThread, true, std::memory_order_release);

	int TimeoutCounter = SHUT_DOWN_COUNT_DOWN;
	while (!(atomic_load_explicit(&PeripheralThreadIsStopped, std::memory_order_acquire)) || (!peripheralThread.joinable())) {
		if (0 == TimeoutCounter) {
			std::cout << "Problem encountered during peripherals closing" << '\n';
			break;
		}
		Fl::wait(0.001 * SHUT_DOWN_LOOP_DELAY); // SHUT_DOWN_LOOP_DELAY in milliseconds
		TimeoutCounter--;
	}
	if (peripheralThread.joinable() && peripheralThread.get_id() != std::this_thread::get_id()) {
		peripheralThread.join();
#if 0
        std::cout << "Peripherals closed; delay loop ran " << SHUT_DOWN_COUNT_DOWN - TimeoutCounter << " times" << '\n';
#endif
	}
}

static void peripheralThreadTiming() {
	// timing
	PeripheralThreadTimeInMilliseconds += PERIPHERAL_THREAD_LOOP_DURATION;
	TimeNow = std::chrono::high_resolution_clock::now();
	std::chrono::milliseconds DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
	while (DurationTime.count() < PeripheralThreadTimeInMilliseconds) {
		TimeNow = std::chrono::high_resolution_clock::now();
		DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
		if (DurationTime.count() >= PeripheralThreadTimeInMilliseconds) {
			break;
		}

		// delay so as not to overload the processor core
		std::this_thread::sleep_for(std::chrono::milliseconds(2));

		TimeNow = std::chrono::high_resolution_clock::now();
		DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
	}
}

/// This function runs the second thread (FLTK is the main thread).
/// The peripheral thread supports Modbus communication and sends signals to FLTK to refresh graphics.
static void peripheralThreadHandler() {
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

    peripheralsInitializations();

	PeripheralThreadTimeInMilliseconds = 0;
	PeripheralThreadLoopStart = std::chrono::high_resolution_clock::now();

    bool PeripheralThreadRunning = true;
	while (PeripheralThreadRunning) {

		peripheralThreadTiming();

        PeripheralThreadRunning = peripheralsTick(atomic_load_explicit(&DoClosePeripheralThread, std::memory_order_acquire));

#if 0 // debugging
		std::chrono::high_resolution_clock::time_point TimeAfter = std::chrono::high_resolution_clock::now();
		std::chrono::milliseconds ProcessingTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeAfter - TimeNow);
		std::cout << "Peripheral thread " << PeripheralThreadTimeInMilliseconds << "  " << ProcessingTime.count() << '\n';
#endif

	} // while (...)
	// exit
	atomic_store_explicit(&PeripheralThreadIsStopped, true, std::memory_order_release);
}

#if 0 // debugging

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they may end
bool peripheralsTick(bool TerminationSignal) {
    static int LoopCounter = 0;
    LoopCounter++;
    LoopCounter &= 0x03;
    if (0 == LoopCounter) {
        static int DebugCounter = 0;
        DebugCounter++;
        std::cout << "DebugCounter: " << DebugCounter << '\n';
    }

    static int ContinuationCounter = 10;
    if (TerminationSignal && (ContinuationCounter > 0)) {
        ContinuationCounter--;
    }
    return ContinuationCounter > 0;
}

#endif // debugging
