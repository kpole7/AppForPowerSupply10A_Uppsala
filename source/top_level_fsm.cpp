/// @file top_level_fsm.cpp
/// @brief Implementation of the top-level finite state machine (FSM) for the application.
/// @details This file contains the logic for managing the sequencies of commands that control
/// the external device

#include "top_level_fsm.h"
#include "debug_tools.h"
#include "modified_rstl_master.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"
#include <cstdint>

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define OPTIONAL_INITIAL_PAUSE_TICKS 50 // in ticks TODO

#define OPTIONAL_IDLE_PAUSE_TICKS 50 // in ticks TODO

#define REST_BREAK_TICKS 20 // in ticks TODO

//=================================================================================================
// Types and classes
//=================================================================================================

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

	// User interaction states on the 'power on' event
	PowerOnExecuting = 12,
	PowerOnChecking = 13, // i.e. checking status
	PowerOnSummary = 14,  // used after transmission failures during power on

	// User interaction states on the 'power off' event
	PowerOffExecuting = 15,
	PowerOffChecking = 16,      // i.e. checking status
	PowerOffOptionalPause = 17, // used after transmission failures during power off

	// User interaction states on the 'change setpoint 1' event
	Setpoint1SelectingChannel = 18,
	Setpoint1CheckingChannel = 19,
	Setpoint1Executing = 20,
	Setpoint1Checking = 21,
	Setpoint1OptionalPause = 22, // used after transmission failures during setpoint 1

	// User interaction states on the 'change setpoint 2' event
	Setpoint2SelectingChannel = 23,
	Setpoint2CheckingChannel = 24,
	Setpoint2Executing = 25,
	Setpoint2Checking = 26,
	Setpoint2OptionalPause = 27, // used after transmission failures during setpoint 2

	// Error state
	PermanentError = 28
};

//=================================================================================================
// Local variables
//=================================================================================================

static top_level_fsm_states TopLevelFsmState = top_level_fsm_states::Uninitialized;
static top_level_fsm_states PreviousTopLevelFsmState = top_level_fsm_states::Uninitialized;
static top_level_fsm_states AnotherPreviousTopLevelFsmState = top_level_fsm_states::Uninitialized;

//=================================================================================================
// Local function prototypes
//=================================================================================================

static top_level_fsm_states getDifferentPreviousState();
static void topLevelFsmPlaceOrder(single_order_code NewCommand, int16_t Argument,
                                  top_level_fsm_states NextStateOnSuccess);
static bool checkResponseAndJumpOnFailure(top_level_fsm_states NewStateOnFailure);
static void topLevelCommonCommand(top_level_fsm_states NextStateOnFailureBeforeCommand, single_order_code NewCommand,
                                  int16_t Argument, top_level_fsm_states NextStateOnSuccess);
static void inescapablePauseThenCommandAndJump(uint16_t PauseTicksMax, single_order_code NewCommand, int16_t Argument,
                                               top_level_fsm_states NextState);
static void topLevelFsmTakeUpNewOrder();

static void topLevelFsmRestBreakState();
static void topLevelFsmPowerOnSummaryState();

//=================================================================================================
// Global function definitions
//=================================================================================================

void initializeTopLevelFsm() { TopLevelFsmState = top_level_fsm_states::Uninitialized; }

void topLevelFsmTick(bool TerminationSignal) {
	if (TerminationSignal) {
		return;
	}

	usart_low_level_error ErrorCode;
	rstl_master_fsm_state RstlMasterState = getRstlMasterState(ErrorCode);
	ReceivedMainPowerState.store(static_cast<main_power_state>(RstlMasterState), std::memory_order_release);
	ReceivedUsartErrorCode.store(static_cast<uint8_t>(ErrorCode), std::memory_order_release);
	if (RstlMasterState == rstl_master_fsm_state::CriticalError) {
		TopLevelFsmState = top_level_fsm_states::PermanentError;
	}

	switch (TopLevelFsmState) {

		// Initialization states

	case top_level_fsm_states::Uninitialized:
		topLevelCommonCommand(top_level_fsm_states::OptionalInitialPause, single_order_code::InquiryVersion, 0,
		                      top_level_fsm_states::GettingVersion);
		break;

	case top_level_fsm_states::GettingVersion:
		topLevelCommonCommand(top_level_fsm_states::OptionalInitialPause, single_order_code::InquiryST, 0,
		                      top_level_fsm_states::CheckingStatus);
		break;

	case top_level_fsm_states::OptionalInitialPause:
		inescapablePauseThenCommandAndJump(OPTIONAL_INITIAL_PAUSE_TICKS, single_order_code::None, 0,
		                                   top_level_fsm_states::Uninitialized);
		break;

		// Idle states

	case top_level_fsm_states::OptionalIdlePause:
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::InquiryST, 0,
		                                   top_level_fsm_states::CheckingStatus);
		break;

	case top_level_fsm_states::CheckingStatus: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			TopLevelFsmState = top_level_fsm_states::RestBreak;
		}
	} break;

	case top_level_fsm_states::RestBreak:
		topLevelFsmRestBreakState();
		break;

		// User interaction states on the 'power on' event

	case top_level_fsm_states::PowerOnExecuting:
		topLevelCommonCommand(top_level_fsm_states::PowerOnSummary, single_order_code::InquiryST, 1,
		                      top_level_fsm_states::PowerOnChecking);
		break;

	case top_level_fsm_states::PowerOnChecking:
		topLevelCommonCommand(top_level_fsm_states::PowerOnSummary, single_order_code::InquiryST, 1,
		                      top_level_fsm_states::PowerOnSummary);
		break;

	case top_level_fsm_states::PowerOnSummary:
		topLevelFsmPowerOnSummaryState();
		break;

		// Error state

	case top_level_fsm_states::PermanentError:
		if (TopLevelFsmState != PreviousTopLevelFsmState) {
			AwakeRefreshWholeGui = true;
		}
		break;

	default:
		break;
	}

	printTopLevelChanges(static_cast<uint8_t>(PreviousTopLevelFsmState), static_cast<uint8_t>(TopLevelFsmState));
	if (TopLevelFsmState != PreviousTopLevelFsmState) {
		AnotherPreviousTopLevelFsmState = PreviousTopLevelFsmState;
	}
	PreviousTopLevelFsmState = TopLevelFsmState;
}

//=================================================================================================
// Local function definitions
//=================================================================================================

static top_level_fsm_states getDifferentPreviousState() {
	if (PreviousTopLevelFsmState != TopLevelFsmState) {
		return PreviousTopLevelFsmState;
	}
	return AnotherPreviousTopLevelFsmState;
}

static void topLevelFsmPlaceOrder(single_order_code NewCommand, int16_t Argument,
                                  top_level_fsm_states NextStateOnSuccess) {
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

/// @brief Checks the response and jumps to the failure state if necessary
/// @param NewStateOnFailure new state to jump to on failure
/// @return True if the response is ready and successful, false otherwise
static bool checkResponseAndJumpOnFailure(top_level_fsm_states NewStateOnFailure) {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return false;
	}

	if (getIsResponseSuccessful()) {
		TopLevelFsmState = NewStateOnFailure;
		return false;
	}
	return true;
}

static void topLevelCommonCommand(top_level_fsm_states NextStateOnFailureBeforeCommand, single_order_code NewCommand,
                                  int16_t Argument, top_level_fsm_states NextStateAfterCommand) {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	if (!getIsResponseSuccessful()) {
		TopLevelFsmState = NextStateOnFailureBeforeCommand;
		return;
	}

	topLevelFsmPlaceOrder(NewCommand, Argument, NextStateAfterCommand);
}

static void inescapablePauseThenCommandAndJump(uint16_t PauseTicksMax, single_order_code NewCommand, int16_t Argument,
                                               top_level_fsm_states NextState) {
	static uint16_t PauseCounter = 0;

	if (TopLevelFsmState != PreviousTopLevelFsmState) {
		PauseCounter = 0; // Redundant reset
	}

	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	if (PauseCounter < PauseTicksMax) {
		PauseCounter++;
		return;
	}
	PauseCounter = 0; // Reset after completing the pause

	if (single_order_code::None == NewCommand) {
		TopLevelFsmState = NextState;
		return;
	}

	topLevelFsmPlaceOrder(NewCommand, Argument, NextState);
}

static void topLevelFsmTakeUpNewOrder() {
	if (CommandToBeSentPowerOn.load(std::memory_order_acquire)) {
		topLevelFsmPlaceOrder(single_order_code::SetPower, 1, top_level_fsm_states::PowerOnExecuting);
		CommandToBeSentPowerOn.store(false, std::memory_order_release);
		return;
	}
	if (CommandToBeSentPowerOff.load(std::memory_order_acquire)) {
		topLevelFsmPlaceOrder(single_order_code::SetPower, 0, top_level_fsm_states::PowerOffExecuting);
		CommandToBeSentPowerOff.store(false, std::memory_order_release);
		return;
	}
}

static void topLevelFsmRestBreakState() {
	static bool IsResponseChecked;
	static uint16_t RestBreakCounter = 0;

	assert(getDifferentPreviousState() == top_level_fsm_states::CheckingStatus);

	if (TopLevelFsmState != PreviousTopLevelFsmState) {
		RestBreakCounter = 0;
		IsResponseChecked = false;
	}

	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	if (!IsResponseChecked) {
		IsResponseChecked = true;
		if (!getIsResponseSuccessful()) {
			TopLevelFsmState = top_level_fsm_states::OptionalIdlePause;
			return;
		}
	}

	bool IsNewOrderReady = CommandToBeSentPowerOn.load(std::memory_order_acquire) ||
	                       CommandToBeSentPowerOff.load(std::memory_order_acquire);

	if (IsNewOrderReady) {
		RestBreakCounter = 0; // Reset the rest break counter when a new order is ready
		topLevelFsmTakeUpNewOrder();
	}

	if (RestBreakCounter < REST_BREAK_TICKS) {
		RestBreakCounter++;
		return;
	}
	RestBreakCounter = 0;
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}

static void topLevelFsmPowerOnSummaryState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	top_level_fsm_states PreviousDifferentState = getDifferentPreviousState();
	if (top_level_fsm_states::PowerOnExecuting == PreviousDifferentState) {
		// Pause after failure related to power on command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 1,
		                                   top_level_fsm_states::PowerOnExecuting);
		return;
	}
	assert(top_level_fsm_states::PowerOnChecking == PreviousDifferentState);

	if (!getIsResponseSuccessful()) {
		// Pause after failure related to check status command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 1,
		                                   top_level_fsm_states::PowerOnExecuting);
		return;
	}
}