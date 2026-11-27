/// @file top_level_fsm.cpp
/// @brief Implementation of the top-level finite state machine (FSM) for the application.
/// @details This file contains the logic for managing the sequencies of commands that control
/// the external device

#include "top_level_fsm.h"
#include "communication_quality.h"
#include "config.h"
#include "debug_tools.h"
#include "modified_rstl_master.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"

#include <cassert>
#include <cstdint>
#include <iostream>
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

	// Monitoring-only states
	OptionalIdlePause = 3, // used after transmission failures during normal operation
	CheckingStatus = 4,
	SelectingChannel1 = 5,
	CheckingChannel1 = 6,
	GettingSetpoint1 = 7,
	SelectingChannel2 = 8,
	CheckingChannel2 = 9,
	GettingSetpoint2 = 10,
	RestBreak = 11, // can be finished if a new order arrives

	// User interaction states in response to a 'power on' request
	PowerOnExecuting = 12,
	PowerOnChecking = 13, // i.e. checking status
	PowerOnSummary = 14,

	// User interaction states in response to a 'power off' request
	PowerOffExecuting = 15,
	PowerOffChecking = 16, // i.e. checking status
	PowerOffSummary = 17,

	// User interaction states in response to a 'change setpoint 1' request
	Setpoint1SelectingChannel = 18,
	Setpoint1CheckingChannel = 19,
	Setpoint1Executing = 20,
	Setpoint1Summary = 21,

	// User interaction states in response to a 'change setpoint 2' request
	Setpoint2SelectingChannel = 22,
	Setpoint2CheckingChannel = 23,
	Setpoint2Executing = 24,
	Setpoint2Summary = 25,

	// Error state
	PermanentError = 26
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
                                  int16_t Argument, top_level_fsm_states NextStateAfterCommand);
static void inescapablePauseThenCommandAndJump(uint16_t PauseTicksMax, single_order_code NewCommand, int16_t Argument,
                                               top_level_fsm_states NextState);
static void topLevelFsmTakeUpNewOrder();

static void topLevelFsmRestBreakState();
static void topLevelFsmPowerOnSummaryState();
static void topLevelFsmPowerOffSummaryState();
static void topLevelFsmSetpoint1CheckingChannelState();
static void topLevelFsmSetpoint1SummaryState();
static void topLevelFsmSetpoint2CheckingChannelState();
static void topLevelFsmSetpoint2SummaryState();

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
	ReceivedUsartErrorCode.store(ErrorCode, std::memory_order_release);
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
		inescapablePauseThenCommandAndJump(OPTIONAL_INITIAL_PAUSE_TICKS, single_order_code::NoOrder, 0,
		                                   top_level_fsm_states::Uninitialized);
		break;

		// Monitoring-only states

	case top_level_fsm_states::OptionalIdlePause:
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::InquiryST, 0,
		                                   top_level_fsm_states::CheckingStatus);
		break;

	case top_level_fsm_states::CheckingStatus: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			int16_t ReceivedMainPowerStateValue = getReceivedNumericalData();
			ReceivedMainPowerState.store(static_cast<main_power_state>(ReceivedMainPowerStateValue),
			                             std::memory_order_release);
			AwakeGuiRefresh = true;
			topLevelFsmPlaceOrder(single_order_code::SetZ, 1, top_level_fsm_states::SelectingChannel1);
		}
	} break;

	case top_level_fsm_states::SelectingChannel1: {
		topLevelCommonCommand(top_level_fsm_states::OptionalIdlePause, single_order_code::InquiryZ, 0,
		                      top_level_fsm_states::CheckingChannel1);
	} break;

	case top_level_fsm_states::CheckingChannel1: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			int16_t Value = getReceivedNumericalData();
			if (Value != 1) {
				TopLevelFsmState = top_level_fsm_states::OptionalIdlePause;
			}
			else {
				topLevelFsmPlaceOrder(single_order_code::InquiryPC, 0, top_level_fsm_states::GettingSetpoint1);
			}
		}
	} break;

	case top_level_fsm_states::GettingSetpoint1: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			int16_t Value = getReceivedNumericalData();
			ReceivedSetpoint1.store(Value, std::memory_order_release);
			IsFreshReceivedSetpoint1.store(true, std::memory_order_release);

			topLevelFsmPlaceOrder(single_order_code::SetZ, 2, top_level_fsm_states::SelectingChannel2);
		}
	} break;

	case top_level_fsm_states::SelectingChannel2: {
		topLevelCommonCommand(top_level_fsm_states::OptionalIdlePause, single_order_code::InquiryZ, 0,
		                      top_level_fsm_states::CheckingChannel2);
	} break;

	case top_level_fsm_states::CheckingChannel2: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			int16_t Value = getReceivedNumericalData();
			if (Value != 2) {
				TopLevelFsmState = top_level_fsm_states::OptionalIdlePause;
			}
			else {
				topLevelFsmPlaceOrder(single_order_code::InquiryPC, 0, top_level_fsm_states::GettingSetpoint2);
			}
		}
	} break;

	case top_level_fsm_states::GettingSetpoint2: {
		bool SuccessfulResponse = checkResponseAndJumpOnFailure(top_level_fsm_states::OptionalIdlePause);
		if (SuccessfulResponse) {
			int16_t Value = getReceivedNumericalData();
			ReceivedSetpoint2.store(Value, std::memory_order_release);
			IsFreshReceivedSetpoint2.store(true, std::memory_order_release);

			topLevelFsmPlaceOrder(single_order_code::SetZ, 2, top_level_fsm_states::RestBreak);
		}
	} break;

	case top_level_fsm_states::RestBreak:
		topLevelFsmRestBreakState();
		break;

		// User interaction states on the 'power on' event

	case top_level_fsm_states::PowerOnExecuting:
		topLevelCommonCommand(top_level_fsm_states::PowerOnSummary, single_order_code::InquiryST, 0,
		                      top_level_fsm_states::PowerOnChecking);
		break;

	case top_level_fsm_states::PowerOnChecking:
		topLevelCommonCommand(top_level_fsm_states::PowerOnSummary, single_order_code::InquiryST, 0,
		                      top_level_fsm_states::PowerOnSummary);
		break;

	case top_level_fsm_states::PowerOnSummary:
		topLevelFsmPowerOnSummaryState();
		break;

		// User interaction states on the 'power off' event

	case top_level_fsm_states::PowerOffExecuting:
		topLevelCommonCommand(top_level_fsm_states::PowerOffSummary, single_order_code::InquiryST, 0,
		                      top_level_fsm_states::PowerOffChecking);
		break;

	case top_level_fsm_states::PowerOffChecking:
		topLevelCommonCommand(top_level_fsm_states::PowerOffSummary, single_order_code::InquiryST, 0,
		                      top_level_fsm_states::PowerOffSummary);
		break;

	case top_level_fsm_states::PowerOffSummary:
		topLevelFsmPowerOffSummaryState();
		break;

		// User interaction states on the 'change setpoint 1' event

	case top_level_fsm_states::Setpoint1SelectingChannel:
		topLevelCommonCommand(top_level_fsm_states::Setpoint1Summary, single_order_code::InquiryZ, 0,
		                      top_level_fsm_states::Setpoint1CheckingChannel);
		break;

	case top_level_fsm_states::Setpoint1CheckingChannel:
		topLevelFsmSetpoint1CheckingChannelState();
		break;

	case top_level_fsm_states::Setpoint1Executing:
		topLevelCommonCommand(top_level_fsm_states::Setpoint1Summary, single_order_code::InquiryPC, 0,
		                      top_level_fsm_states::Setpoint1Summary);
		break;

	case top_level_fsm_states::Setpoint1Summary:
		topLevelFsmSetpoint1SummaryState();
		break;

		// User interaction states on the 'change setpoint 2' event

	case top_level_fsm_states::Setpoint2SelectingChannel:
		topLevelCommonCommand(top_level_fsm_states::Setpoint2Summary, single_order_code::InquiryZ, 0,
		                      top_level_fsm_states::Setpoint2CheckingChannel);
		break;

	case top_level_fsm_states::Setpoint2CheckingChannel:
		topLevelFsmSetpoint2CheckingChannelState();
		break;

	case top_level_fsm_states::Setpoint2Executing:
		topLevelCommonCommand(top_level_fsm_states::Setpoint2Summary, single_order_code::InquiryPC, 0,
		                      top_level_fsm_states::Setpoint2Summary);
		break;

	case top_level_fsm_states::Setpoint2Summary:
		topLevelFsmSetpoint2SummaryState();
		break;

		// Error state

	case top_level_fsm_states::PermanentError:
		if (TopLevelFsmState != PreviousTopLevelFsmState) {
			AwakeGuiRefresh = true;
			std::cout << "PERMANENT ERROR ENCOUNTERED" << '\n';
		}
		break;

	default:
		break;
	}

#if 0 // show top-level FSM state changes for debugging purposes
	printTopLevelChanges(static_cast<uint8_t>(PreviousTopLevelFsmState), static_cast<uint8_t>(TopLevelFsmState));
#endif
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

	if (!getIsResponseSuccessful()) {
		TopLevelFsmState = NewStateOnFailure;
		updateCommunicationQuality(-1);
		return false;
	}
	updateCommunicationQuality(1);
	return true;
}

static void topLevelCommonCommand(top_level_fsm_states NextStateOnFailureBeforeCommand, single_order_code NewCommand,
                                  int16_t Argument, top_level_fsm_states NextStateAfterCommand) {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	if (!getIsResponseSuccessful()) {
		TopLevelFsmState = NextStateOnFailureBeforeCommand;
		updateCommunicationQuality(-1);
		return;
	}

	topLevelFsmPlaceOrder(NewCommand, Argument, NextStateAfterCommand);
	updateCommunicationQuality(1);
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
	updateCommunicationQuality(-10);

	if (single_order_code::NoOrder == NewCommand) {
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
	if (CommandToBeSentSetpoint1.load(std::memory_order_acquire)) {
		topLevelFsmPlaceOrder(single_order_code::SetZ, 1, top_level_fsm_states::Setpoint1SelectingChannel);
		CommandToBeSentSetpoint1.store(false, std::memory_order_release);
		return;
	}
	if (CommandToBeSentSetpoint2.load(std::memory_order_acquire)) {
		topLevelFsmPlaceOrder(single_order_code::SetZ, 2, top_level_fsm_states::Setpoint2SelectingChannel);
		CommandToBeSentSetpoint2.store(false, std::memory_order_release);
		return;
	}
}

static void topLevelFsmRestBreakState() {
	static bool IsResponseChecked;
	static uint16_t RestBreakCounter = 0;

	assert(getDifferentPreviousState() == top_level_fsm_states::GettingSetpoint2);

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
	                       CommandToBeSentPowerOff.load(std::memory_order_acquire) ||
	                       CommandToBeSentSetpoint1.load(std::memory_order_acquire) ||
	                       CommandToBeSentSetpoint2.load(std::memory_order_acquire);

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
	int16_t ReceivedData = getReceivedNumericalData();
	main_power_state ReceivedMainPowerStateValue = static_cast<main_power_state>(ReceivedData);
	assert(ReceivedMainPowerStateValue < main_power_state::UNDEFINED);
	if (ReceivedMainPowerStateValue != main_power_state::RISING) {
		// Pause after failure related to check status command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 1,
		                                   top_level_fsm_states::PowerOnExecuting);
		return;
	}

	// If we reach this point, the power is rising as expected

	ReceivedMainPowerState.store(ReceivedMainPowerStateValue, std::memory_order_release);
	AwakeGuiRefresh = true;
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}

static void topLevelFsmPowerOffSummaryState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	top_level_fsm_states PreviousDifferentState = getDifferentPreviousState();
	if (top_level_fsm_states::PowerOffExecuting == PreviousDifferentState) {
		// Pause after failure related to power off command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 0,
		                                   top_level_fsm_states::PowerOffExecuting);
		return;
	}
	assert(top_level_fsm_states::PowerOffChecking == PreviousDifferentState);

	if (!getIsResponseSuccessful()) {
		// Pause after failure related to check status command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 0,
		                                   top_level_fsm_states::PowerOffExecuting);
		return;
	}
	int16_t ReceivedData = getReceivedNumericalData();
	main_power_state ReceivedMainPowerStateValue = static_cast<main_power_state>(ReceivedData);
	assert(ReceivedMainPowerStateValue < main_power_state::UNDEFINED);
	if (ReceivedMainPowerStateValue != main_power_state::FALLING) {
		// Pause after failure related to check status command
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetPower, 0,
		                                   top_level_fsm_states::PowerOffExecuting);
		return;
	}

	// If we reach this point, the power is falling as expected

	ReceivedMainPowerState.store(ReceivedMainPowerStateValue, std::memory_order_release);
	AwakeGuiRefresh = true;

	// Jump to the monitoring-only sequence
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}

static void topLevelFsmSetpoint1CheckingChannelState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	top_level_fsm_states PreviousDifferentState = getDifferentPreviousState();
	assert(top_level_fsm_states::Setpoint1SelectingChannel == PreviousDifferentState);

	if (!getIsResponseSuccessful()) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 1,
		                                   top_level_fsm_states::Setpoint1SelectingChannel);
		return;
	}
	int16_t ReceivedData = getReceivedNumericalData();
	assert((ReceivedData >= 0) && (ReceivedData <= PSU_NUMBER_OF_CHANNELS));
	if (ReceivedData != 1) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 1,
		                                   top_level_fsm_states::Setpoint1SelectingChannel);
		return;
	}

	// If we reach this point, the channel selection is successful

	topLevelFsmPlaceOrder(single_order_code::SetPC, ValueToBeSentSetpoint1.load(std::memory_order_acquire),
	                      top_level_fsm_states::Setpoint1Executing);
}

static void topLevelFsmSetpoint1SummaryState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	single_order_code RecentOrder = getRecentOrder();
	int16_t ReceivedData = getReceivedNumericalData();
	if ((RecentOrder != single_order_code::InquiryPC) || !getIsResponseSuccessful() ||
	    (ReceivedData != ValueToBeSentSetpoint1.load(std::memory_order_acquire))) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 1,
		                                   top_level_fsm_states::Setpoint1SelectingChannel);
		return;
	}

	ReceivedSetpoint1.store(ReceivedData, std::memory_order_release);
	IsFreshReceivedSetpoint1.store(true, std::memory_order_release);

	// Jump to the monitoring-only sequence
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}

static void topLevelFsmSetpoint2CheckingChannelState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	top_level_fsm_states PreviousDifferentState = getDifferentPreviousState();
	assert(top_level_fsm_states::Setpoint2SelectingChannel == PreviousDifferentState);

	if (!getIsResponseSuccessful()) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 2,
		                                   top_level_fsm_states::Setpoint2SelectingChannel);
		return;
	}
	int16_t ReceivedData = getReceivedNumericalData();
	assert((ReceivedData >= 0) && (ReceivedData <= PSU_NUMBER_OF_CHANNELS));
	if (ReceivedData != 2) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 2,
		                                   top_level_fsm_states::Setpoint2SelectingChannel);
		return;
	}

	// If we reach this point, the channel selection is successful

	topLevelFsmPlaceOrder(single_order_code::SetPC, ValueToBeSentSetpoint2.load(std::memory_order_acquire),
	                      top_level_fsm_states::Setpoint2Executing);
}

static void topLevelFsmSetpoint2SummaryState() {
	if (getOrderProcessingState() != order_processing_state::Ready) {
		return;
	}

	single_order_code RecentOrder = getRecentOrder();
	int16_t ReceivedData = getReceivedNumericalData();
	if ((RecentOrder != single_order_code::InquiryPC) || !getIsResponseSuccessful() ||
	    (ReceivedData != ValueToBeSentSetpoint2.load(std::memory_order_acquire))) {
		// Pause after failure
		inescapablePauseThenCommandAndJump(OPTIONAL_IDLE_PAUSE_TICKS, single_order_code::SetZ, 2,
		                                   top_level_fsm_states::Setpoint2SelectingChannel);
		return;
	}

	ReceivedSetpoint2.store(ReceivedData, std::memory_order_release);
	IsFreshReceivedSetpoint2.store(true, std::memory_order_release);

	// Jump to the monitoring-only sequence
	topLevelFsmPlaceOrder(single_order_code::InquiryST, 0, top_level_fsm_states::CheckingStatus);
}
