/// @file peripheral_thread.cpp
/// @brief Implements a separate thread and timing functionality for peripheral operations.

#include <FL/Fl.H>
#include <chrono>
#include <iostream>
#include <thread>

#include "config.h"
#include "debug_tools.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define SHUT_DOWN_TIMEOUT 1000 // milliseconds

#define SHUT_DOWN_LOOP_DELAY 20 // milliseconds

#define SHUT_DOWN_COUNT_DOWN (SHUT_DOWN_TIMEOUT / SHUT_DOWN_LOOP_DELAY)

//=================================================================================================
// Global variables
//=================================================================================================

bool AwakeRefreshWholeGui = false;

bool AwakeRefreshDebugInfoGui = false;

uint16_t LongestTimeTicsBehindSchedule = 0;

//=================================================================================================
// Local variables
//=================================================================================================

/// This flag is set when the application is being closed
static std::atomic<bool> DoClosePeripheralThread;

/// This flag is set when the application is being closed instantly
static std::atomic<bool> DoHardClosePeripheralThread;

/// This flag is set when the peripheral thread has been stopped
static std::atomic<bool> PeripheralThreadIsStopped;

static std::thread peripheralThread;

static bool DebugingPeripheralsEnable;

static std::chrono::high_resolution_clock::time_point PeripheralThreadLoopStart;
static std::chrono::high_resolution_clock::time_point TimeNow;
static int64_t PeripheralThreadTimeInMilliseconds;

//=================================================================================================
// Local function prototypes
//=================================================================================================

static void peripheralThreadTiming();
static void peripheralThreadHandler();

//=================================================================================================
// Function definitions
//=================================================================================================

/// This function launches a new thread to support peripherals. It should be called in the FLTK thread.
void peripheralThreadStart() {
	atomic_store_explicit(&DoClosePeripheralThread, false, std::memory_order_release);
	atomic_store_explicit(&DoHardClosePeripheralThread, false, std::memory_order_release);
	atomic_store_explicit(&PeripheralThreadIsStopped, false, std::memory_order_release);
	peripheralThread = std::thread(peripheralThreadHandler);
}

/// This function is called by the FLTK onMainWindowCloseCallback event handler. It stops the peripheral thread safely.
void peripheralThreadExit() {
	if (atomic_load_explicit(&PeripheralThreadIsStopped, std::memory_order_acquire)) {
		return;
	}

	atomic_store_explicit(&DoClosePeripheralThread, true, std::memory_order_release);

	int TimeoutCounter = SHUT_DOWN_COUNT_DOWN;
	while (!(atomic_load_explicit(&PeripheralThreadIsStopped, std::memory_order_acquire)) || (!peripheralThread.joinable())) {
		if (0 == TimeoutCounter) {
			std::cout << "Problem encountered during peripherals closing" << '\n';
			atomic_store_explicit(&DoHardClosePeripheralThread, true, std::memory_order_release);
		}
		Fl::wait(0.001 * SHUT_DOWN_LOOP_DELAY); // SHUT_DOWN_LOOP_DELAY in milliseconds
		TimeoutCounter--;
	}
	if (peripheralThread.joinable() && peripheralThread.get_id() != std::this_thread::get_id()) {
		peripheralThread.join();
	}
}

/// This function runs the second thread (FLTK is the main thread).
/// The peripheral thread supports communication with peripherals and sends signals to FLTK to refresh graphics.
static void peripheralThreadHandler() {
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	debugPeripheralsInitializations();

	DebugingPeripheralsEnable = true;

	PeripheralThreadTimeInMilliseconds = 0;
	PeripheralThreadLoopStart = std::chrono::high_resolution_clock::now();

	while (DebugingPeripheralsEnable) {
		if (AwakeRefreshWholeGui) {
			Fl::awake(refreshWholeGui, nullptr);
			AwakeRefreshWholeGui = false;
		}
		peripheralThreadTiming();
		if (atomic_load_explicit(&DoHardClosePeripheralThread, std::memory_order_acquire)) {
			break;
		}
		DebugingPeripheralsEnable = debuggingPeripheralsTick(atomic_load_explicit(&DoClosePeripheralThread, std::memory_order_acquire));
	} // while (...)
	// exit
	atomic_store_explicit(&PeripheralThreadIsStopped, true, std::memory_order_release);
}

/// @brief Handles the timing for the peripheral thread loop.
/// @details This function ensures a stable loop frequency in the peripheral thread.
/// In addition, the function checks to see if task processing is taking too long.
static void peripheralThreadTiming() {
	static uint16_t ActualTimeTicsBehindSchedule = 0;

	PeripheralThreadTimeInMilliseconds += PERIPHERAL_THREAD_LOOP_DURATION;
	TimeNow = std::chrono::high_resolution_clock::now();
	std::chrono::milliseconds DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
	bool ItIsRunningOutOfTime = true;
	while (DurationTime.count() < PeripheralThreadTimeInMilliseconds) {
		ItIsRunningOutOfTime = false;
		TimeNow = std::chrono::high_resolution_clock::now();
		DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
		if (DurationTime.count() >= PeripheralThreadTimeInMilliseconds) {
			break;
		}

		// delay so as not to overload the processor core
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		TimeNow = std::chrono::high_resolution_clock::now();
		DurationTime = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - PeripheralThreadLoopStart);
	}
	if (ItIsRunningOutOfTime) {
		if (ActualTimeTicsBehindSchedule < UINT16_MAX) {
			ActualTimeTicsBehindSchedule++;
		}
		if (ActualTimeTicsBehindSchedule > LongestTimeTicsBehindSchedule) {
			LongestTimeTicsBehindSchedule = ActualTimeTicsBehindSchedule;
		}
	}
	else {
		ActualTimeTicsBehindSchedule = 0;
	}
}
