/// @file top_level_fsm.cpp
/// @brief Implementation of the top-level finite state machine (FSM) for the application.
/// @details This file contains the logic for managing the sequencies of commands that control
/// the external device

#include "top_level_fsm.h"
#include "debug_tools.h"
#include "modified_rstl_master.h"
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

	// User interaction states
	SendingPowerOn = 12,
	SendingPowerOff = 13,
	SendingNewSetpoint1 = 14,
	SendingNewSetpoint2 = 15,

	// Error states
	PermanentError = 16
};

//=================================================================================================
// Local variables
//=================================================================================================

static top_level_fsm_states TopLevelFsmState = top_level_fsm_states::Uninitialized;
static uint16_t OptionalInitialPauseCounter = 0;
static uint16_t OptionalIdlePauseCounter = 0;
static uint16_t RestBreakCounter = 0;

//=================================================================================================
// Local function prototypes
//=================================================================================================

static void topLevelFsmPlaceOrder(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess);
static void topLevelCommonCommand(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess,
                                  top_level_fsm_states NextStateOnFailure);
static void topLevelFsmCommonOptionalPauseState(uint16_t &PauseCounter, uint16_t PauseTicksMax, top_level_fsm_states NextState);
static void topLevelFsmCheckingStatusState();
static void topLevelFsmRestBreakState();

//=================================================================================================
// Function definitions
//=================================================================================================

void initializeTopLevelFsm() {
	OptionalInitialPauseCounter = 0;
	OptionalIdlePauseCounter = 0;
	RestBreakCounter = 0;
	TopLevelFsmState = top_level_fsm_states::Uninitialized;
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

static void topLevelFsmPlaceOrder(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess) {
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

static void topLevelCommonCommand(single_order_code NewCommand, int16_t Argument, top_level_fsm_states NextStateOnSuccess,
                                  top_level_fsm_states NextStateOnFailure) {
	if (getOrderProcessingState() == order_processing_state::Ready) {
		if (getIsResponseSuccessful()) {
			topLevelFsmPlaceOrder(NewCommand, Argument, NextStateOnSuccess);
			return;
		}
		TopLevelFsmState = NextStateOnFailure;
	}
}

static void topLevelFsmCommonOptionalPauseState(uint16_t &PauseCounter, uint16_t PauseTicksMax, top_level_fsm_states NextState) {
	if (PauseCounter < PauseTicksMax) {
		PauseCounter++;
		return;
	}
	PauseCounter = 0;
	TopLevelFsmState = NextState;
}

static void topLevelFsmCheckingStatusState() {
	if (getOrderProcessingState() == order_processing_state::Ready) {
		if (getIsResponseSuccessful()) {
			RestBreakCounter = 0;
			TopLevelFsmState = top_level_fsm_states::RestBreak; // can be shortened
			return;
		}
		TopLevelFsmState = top_level_fsm_states::OptionalIdlePause; // cannot be shortened
	}
}

static void topLevelFsmRestBreakState() {
	if (RestBreakCounter < REST_BREAK_TICKS) {
		RestBreakCounter++;
		return;
	}
	RestBreakCounter = 0;
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}
