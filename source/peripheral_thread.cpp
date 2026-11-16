/// @file peripheral_thread.cpp
/// @brief Implements a separate thread and timing functionality for peripheral operations.

#include <FL/Fl.H>
#include <chrono>
#include <iostream>
#include <thread>

#include "config.h"
#include "debug_tools.h"
#include "modified_rstl_master.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"
#include "usart_master.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define SHUT_DOWN_TIMEOUT 1000 // milliseconds

#define SHUT_DOWN_LOOP_DELAY 20 // milliseconds

#define SHUT_DOWN_COUNT_DOWN (SHUT_DOWN_TIMEOUT / SHUT_DOWN_LOOP_DELAY)

#define OPTIONAL_INITIAL_PAUSE_TICKS 100 // in ticks TODO

#define OPTIONAL_IDLE_PAUSE_TICKS 100 // in ticks TODO

#define REST_BREAK_TICKS 100 // in ticks TODO

enum class top_level_fsm_states : uint8_t {
	// Initialization states
	Uninitialized = 0,
	GettingVersion = 1,
	OptionalInitialPause = 2, // used after transmission failures during initialization

	// Idle states
	OptionalIdlePause = 3, // used after transmission failures during normal operation
	CheckingStatus = 4,
	SelectingChannel1 = 5,
	CheckingChannel1 = 6,
	GettingSetpoint1 = 7,
	SelectingChannel2 = 8,
	CheckingChannel2 = 9,
	GettingSetpoint2 = 10,
	RestBreak = 11, // can be finished if a new order arrives

	// User interaction states
	SendingPowerOn = 12,
	SendingPowerOff = 13,
	SendingNewSetpoint1 = 14,
	SendingNewSetpoint2 = 15,

	// Error states
	PermanentError = 16
};

static top_level_fsm_states TopLevelFsmState = top_level_fsm_states::Uninitialized;
static uint16_t OptionalInitialPauseCounter = 0;
static uint16_t OptionalIdlePauseCounter = 0;
static uint16_t RestBreakCounter = 0;

void initializeTopLevelFsm() {
	OptionalInitialPauseCounter = 0;
	OptionalIdlePauseCounter = 0;
	RestBreakCounter = 0;
	TopLevelFsmState = top_level_fsm_states::Uninitialized;
}

void topLevelFsmPlaceOrder(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess) {
	order_with_argument NewOrder;
	NewOrder.OrderCode = NewCommand;
	NewOrder.Argument = Argument;
	bool Success = placeRstlOrder(NewOrder);
	if (Success) {
		TopLevelFsmState = NextStateOnSuccess;
		return;
	}
	TopLevelFsmState = top_level_fsm_states::PermanentError; // Internal error or USART error
}

void topLevelCommonCommand(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess,
                           top_level_fsm_states NextStateOnFailure) {
	if (getOrderProcessingState() == order_processing_state::Ready) {
		if (getIsResponseSuccessful()) {
			topLevelFsmPlaceOrder(NewCommand, Argument, NextStateOnSuccess);
			return;
		}
		TopLevelFsmState = NextStateOnFailure;
	}
}

void topLevelFsmCommonOptionalPauseState(uint16_t &PauseCounter, uint16_t PauseTicksMax, top_level_fsm_states NextState) {
	if (PauseCounter < PauseTicksMax) {
		PauseCounter++;
		return;
	}
	PauseCounter = 0;
	TopLevelFsmState = NextState;
}

void topLevelFsmCheckingStatusState() {
	if (getOrderProcessingState() == order_processing_state::Ready) {
		if (getIsResponseSuccessful()) {
			RestBreakCounter = 0;
			TopLevelFsmState = top_level_fsm_states::RestBreak; // can be shortened
			return;
		}
		TopLevelFsmState = top_level_fsm_states::OptionalIdlePause; // cannot be shortened
	}
}

void topLevelFsmRestBreakState() {
	if (RestBreakCounter < REST_BREAK_TICKS) {
		RestBreakCounter++;
		return;
	}
	RestBreakCounter = 0;
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}

void topLevelFsmTick(bool TerminationSignal) {
	if (TerminationSignal) {
		return;
	}

	switch (TopLevelFsmState) {
	case top_level_fsm_states::Uninitialized:
		topLevelCommonCommand(single_order_code::InquiryVersion, 0, top_level_fsm_states::GettingVersion, top_level_fsm_states::OptionalInitialPause);
		break;

	case top_level_fsm_states::GettingVersion:
		topLevelCommonCommand(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus, top_level_fsm_states::OptionalInitialPause);
		break;

	case top_level_fsm_states::OptionalInitialPause:
		topLevelFsmCommonOptionalPauseState(OptionalInitialPauseCounter, OPTIONAL_INITIAL_PAUSE_TICKS, top_level_fsm_states::Uninitialized);
		break;

	case top_level_fsm_states::OptionalIdlePause:
		topLevelFsmCommonOptionalPauseState(OptionalIdlePauseCounter, OPTIONAL_IDLE_PAUSE_TICKS, top_level_fsm_states::Uninitialized);
		break;

	case top_level_fsm_states::CheckingStatus:
		topLevelFsmCheckingStatusState();
		break;

	case top_level_fsm_states::RestBreak:
		topLevelFsmRestBreakState();
		break;

	case top_level_fsm_states::PermanentError:
		// Handle permanent error state if needed
		break;

	// Add other states as needed
	default:
		break;
	}

	static top_level_fsm_states PreviousTopLevelFsmState = top_level_fsm_states::Uninitialized;
	printTopLevelChanges(static_cast<uint8_t>(PreviousTopLevelFsmState), static_cast<uint8_t>(TopLevelFsmState));
	PreviousTopLevelFsmState = TopLevelFsmState;
}

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
	static bool ContinuePeripheralsLoop;

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	debugPeripheralsInitializations();
	initializeRstlMaster();
	initializeTopLevelFsm();

	PeripheralThreadTimeInMilliseconds = 0;
	PeripheralThreadLoopStart = std::chrono::high_resolution_clock::now();

	ContinuePeripheralsLoop = true; // start the peripheral loop
	while (ContinuePeripheralsLoop) {
		if (AwakeRefreshWholeGui) {
			Fl::awake(refreshWholeGui, nullptr);
			AwakeRefreshWholeGui = false;
		}
		peripheralThreadTiming();
		if (atomic_load_explicit(&DoHardClosePeripheralThread, std::memory_order_acquire)) {
			break;
		}
		bool TerminationSignal = atomic_load_explicit(&DoClosePeripheralThread, std::memory_order_acquire);
		ContinuePeripheralsLoop = !TerminationSignal; // initially set based on termination signal

		// Tasks to be executed in the peripheral loop

		ContinuePeripheralsLoop |= debuggingToolsTick(TerminationSignal);

		ContinuePeripheralsLoop |= rstlMasterTick(TerminationSignal);

		topLevelFsmTick(TerminationSignal);

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
