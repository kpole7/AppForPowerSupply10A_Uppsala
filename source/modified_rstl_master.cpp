/// @file modified_rstl_master.cpp
/// @brief Implementation of the modified RSTL protocol for the master device.
/// @details The protocol is based on RSTL 488 Programmer protocol specifications (Electronic Measurements, Inc.)
/// This implementation handles communication with the procedures defined in the usart_master module.
/// This implementation is driven by the peripheral_thread module. The usart_master module sends and receives
/// continuous data packets. This module is responsible for syntax and timing of the communication.
/// This module manages errors reported by the usart_master module. The module's function is to generate commands
/// that comply with the protocol, and then to receive and interpret the responses. In addition, it ensures proper
/// that complies with the protocol, and then to receive and interpret the responses. In addition, it ensures proper
/// error handling (for syntax and timing errors). This module covers handling of single commands. Another
/// higher-level module is responsible for creating and managing sequences of commands.

#include <cassert>
#include <iostream>

#include "modified_rstl_master.h"
#include "settings_file.h"
#include "usart_master.h"
#include "usart_master_debug.h"

#define SUCCESSFUL_COMMUNICATION true
#define FAILED_COMMUNICATION false

/// @brief Response length limits for each single order code.
/// @note The index corresponds to the single_order_code enum value.
static const uint8_t ResponseLengthLimits[] = {
	0,  // Placeholder for index 0
    12,  // InquiryPower
    9,  // SetPower
    10,  // InquiryZ
    5,  // SetZ
    12,  // InquiryPC
    12,  // SetPC
    44, // InquiryST
    38, // InquiryVersion
    23, // ResetErrors
};

rstl_callback_function RstlCallback = nullptr;

/// @brief Registers a callback function for handling RSTL commands.
/// @param Callback The callback function to be registered.
/// @note This function can be called only once to register the callback.
void registerRstlCallback(rstl_callback_function Callback) {
	assert(nullptr == RstlCallback);
	assert(nullptr != Callback);
	RstlCallback = Callback;
}

static modified_rstl_master_ptr RstlMasterInstancePtr = nullptr;

modified_rstl_master::modified_rstl_master() {
	CurrentState = rstl_master_fsm_state::Uninitialized;
	LowLevelUsartPtr = new low_level_usart();
	OrderProcessingState = order_processing_state::NotReady;
	ResponseFailureHistory = 0;
	TotalResponseCount = 0;
	TotalFailedResponseCount = 0;
}

modified_rstl_master::~modified_rstl_master() { delete LowLevelUsartPtr; }

usart_low_level_error modified_rstl_master::initialize() {
	std::string PortName = "/dev/ttyUSB0";
	CurrentState = rstl_master_fsm_state::IdleAndSend;
	usart_low_level_error Error = LowLevelUsartPtr->startModule(PortName);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
	}
	return Error;
}

rstl_master_fsm_state modified_rstl_master::getCurrentState() const { return CurrentState; }

usart_low_level_error modified_rstl_master::getUsartErrorCode() const { return LowLevelUsartPtr->getErrorCode(); }

order_processing_state modified_rstl_master::getOrderProcessingState() const { return OrderProcessingState; }

bool modified_rstl_master::placeOrder(const order_with_argument &NewOrder) {
	if (OrderProcessingState != order_processing_state::Ready) {
		std::cout << "Order is not ready for placement (line " << __LINE__ << " in file " << __FILE__ << ")\n";
		return false;
	}

	// Formulate the command based on the current order
	switch (NewOrder.OrderCode) {
	case single_order_code::InquiryPower:
		CommandInTextFormat = "?POWER\r\n";
		break;
	case single_order_code::SetPower:
		if (0 == NewOrder.Argument) {
			CommandInTextFormat = "POWER0\r\n";
		}
		else {
			assert(NewOrder.Argument == 1); // Ensure the argument is valid for SetPower
			CommandInTextFormat = "POWER1\r\n";
		}
		break;
	case single_order_code::InquiryZ:
		CommandInTextFormat = "?Z\r\n";
		break;
	case single_order_code::SetZ:
		assert(NewOrder.Argument >= 1);
		assert(NewOrder.Argument <= PSU_NUMBER_OF_CHANNELS);
		CommandInTextFormat = "Z" + std::to_string(NewOrder.Argument) + "\r\n";
		break;
	case single_order_code::InquiryPC:
		CommandInTextFormat = "?PC\r\n";
		break;
	case single_order_code::SetPC: {
		assert(NewOrder.Argument >= -1000); // TODO correct magic numbers
		assert(NewOrder.Argument <= 1000);
		bool IsNegative;
		int16_t AbsoluteValue;
		if (NewOrder.Argument >= 0) {
			IsNegative = false;
			AbsoluteValue = NewOrder.Argument;
		}
		else {
			IsNegative = true;
			AbsoluteValue = -NewOrder.Argument;
		}
		int16_t BeforePoint = AbsoluteValue / 100;
		int16_t AfterPoint = AbsoluteValue % 100;
		if (IsNegative) {
			CommandInTextFormat = "PC -" + std::to_string(BeforePoint) + "." + (AfterPoint < 10 ? "0" : "") + std::to_string(AfterPoint) + "\r\n";
		}
		else {
			CommandInTextFormat = "PC " + std::to_string(BeforePoint) + "." + (AfterPoint < 10 ? "0" : "") + std::to_string(AfterPoint) + "\r\n";
		}
	} break;
	case single_order_code::InquiryST:
		CommandInTextFormat = "ST\r\n";
		break;
	case single_order_code::InquiryVersion:
		CommandInTextFormat = "VERSION\r\n";
		break;
	case single_order_code::ResetErrors:
		CommandInTextFormat = "RE\r\n";
		break;
	default:
		// Unsupported order code
		std::cout << "Unsupported order code #" << static_cast<int>(NewOrder.OrderCode) << 
		" (line " << __LINE__ << " in file " << __FILE__ << ")\n";
		return false;
	}

	CurrentOrder = NewOrder;
	OrderProcessingState = order_processing_state::Placed;

	return true;
}

uint8_t modified_rstl_master::getCurrentCommunicationQuality() const {
	if (TotalResponseCount == 0) {
		return 100;
	}
	int FailureCount = 0;
	for (int i = 0; i < 64; ++i) {
		if ((ResponseFailureHistory & (1ULL << i)) != 0) {
			FailureCount++;
		}
	}
	return static_cast<uint8_t>(((TotalResponseCount - FailureCount) * 100 + TotalResponseCount / 2) / TotalResponseCount);
}

uint8_t modified_rstl_master::getTotalCommunicationQuality() const {
	if (TotalResponseCount == 0) {
		return 100;
	}
	uint32_t LocalTotalCount = TotalResponseCount;
	uint32_t LocalFailedCount = TotalFailedResponseCount;
	if (TotalResponseCount > (uint32_t)0xFFFFFFU) {
		// Prevent overflow by scaling down the counts
		LocalTotalCount /= 2;
		LocalFailedCount /= 2;
	}
	return static_cast<uint8_t>(((LocalTotalCount - LocalFailedCount) * 100 + LocalTotalCount / 2) / LocalTotalCount);
}

/// @brief Handles a single tick of operations related to the external device managed with the modified RSTL protocol
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they can be terminated
bool modified_rstl_master::fsmTick(bool TerminationSignal) {
	if (TerminationSignal) {
		if (nullptr == RstlMasterInstancePtr) {
			return false; // Indicate that the peripheral operations should not continue
		}
		delete RstlMasterInstancePtr;
		return false; // Indicate that the peripheral operations should not continue
	}
	assert(nullptr != RstlMasterInstancePtr); // Ensure the instance exists
	assert(LowLevelUsartPtr != nullptr);

	switch (CurrentState) {
	case rstl_master_fsm_state::Uninitialized:
		fsmTickUninitialized();
		break;
	case rstl_master_fsm_state::IdleAndSend:
		fsmTickIdleAndSend();
		break;
	case rstl_master_fsm_state::WaitForResponse:
		fsmTickWaitForResponse();
		break;
	case rstl_master_fsm_state::Receive:
		fsmTickReceive();
		break;
	case rstl_master_fsm_state::Noise:
		fsmTickNoise();
		break;
	default:
		assert(usart_low_level_error::None != LowLevelUsartPtr->getErrorCode());
		assert(usart_low_level_state::Error == LowLevelUsartPtr->getState());
		assert(CurrentState == rstl_master_fsm_state::CriticalError);
		// Wait for termination
		break;
	}

	printStateChanges(LowLevelUsartPtr, CurrentState, nullptr);

	switch (CurrentState) {
	case rstl_master_fsm_state::IdleAndSend:
	case rstl_master_fsm_state::Noise:
		if (order_processing_state::Placed != OrderProcessingState) {
			OrderProcessingState = order_processing_state::Ready;
		}
		break;

	case rstl_master_fsm_state::WaitForResponse:
	case rstl_master_fsm_state::Receive:
		OrderProcessingState = order_processing_state::InProgress;
		break;

	default:
		break;
	}

	return true;
}

void modified_rstl_master::fsmTickUninitialized() {
	assert(usart_low_level_state::Error != LowLevelUsartPtr->getState());
	if (!SerialPortName.empty()) {
		if (usart_low_level_state::Uninitialized == LowLevelUsartPtr->getState()) {
			usart_low_level_error Error = LowLevelUsartPtr->startModule(SerialPortName);
			if (Error != usart_low_level_error::None) {
				CurrentState = rstl_master_fsm_state::CriticalError;
				return;
			}
		}
		assert(usart_low_level_state::Open == LowLevelUsartPtr->getState());
		if (RstlCallback != nullptr) {
			// The FSM can operate normally because the USART port is open and the callback is set
			CurrentState = rstl_master_fsm_state::IdleAndSend;
		}
	}
}

void modified_rstl_master::fsmTickIdleAndSend() {
	assert(usart_low_level_state::Open == LowLevelUsartPtr->getState());
	bool Silence = false;
	assert(static_cast<uint8_t>(CurrentOrder.OrderCode) < sizeof(ResponseLengthLimits) / sizeof(ResponseLengthLimits[0]));
	uint8_t ExpectedResponseLimit = ResponseLengthLimits[static_cast<uint8_t>(CurrentOrder.OrderCode)]; // TODO
	usart_low_level_error Error = LowLevelUsartPtr->handleIdleAndSendState(&Silence, CommandInTextFormat, ExpectedResponseLimit);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
		return;
	}
	if (Silence) {
		if (!CommandInTextFormat.empty()) {
			// The current command has been copied and is now being sent
			CommandInTextFormat.clear(); // Clear the current command pointer
			CurrentState = rstl_master_fsm_state::WaitForResponse;
			return;
		}
		// No current command to send, remain in IdleAndSend state
		return;
	}
	// Unexpected input while expecting silence
	CurrentState = rstl_master_fsm_state::Noise;
	std::cout << "Noise (1)) #" << static_cast<int>(CurrentOrder.OrderCode) << 
	" (line " << __LINE__ << " in file " << __FILE__ << ")\n";
}

void modified_rstl_master::fsmTickWaitForResponse() {
	assert(usart_low_level_state::Open == LowLevelUsartPtr->getState());
	bool TimeoutExceeded = false;
	bool Silence = false;
	bool LimitsExceeded = false;
	usart_low_level_error Error = LowLevelUsartPtr->handleWaitForResponseState(&TimeoutExceeded, &Silence, &LimitsExceeded);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
		return;
	}
	if (TimeoutExceeded) {
		// The silence lasted longer than expected
		CurrentState = rstl_master_fsm_state::IdleAndSend;
		updateCommunicationQuality(FAILED_COMMUNICATION);
		return;
	}
	if (Silence) {
		// Remain in the WaitForResponse state until silence is detected
		return;
	}
	if (LimitsExceeded) {
		CurrentState = rstl_master_fsm_state::Noise;
		updateCommunicationQuality(FAILED_COMMUNICATION);
		std::cout << "Noise (2)) #" << static_cast<int>(CurrentOrder.OrderCode) << 
		" (line " << __LINE__ << " in file " << __FILE__ << ")\n";
		return;
	}
	// Receive incoming transmission until silence is detected
	CurrentState = rstl_master_fsm_state::Receive;
}

void modified_rstl_master::fsmTickReceive() {
	assert(usart_low_level_state::Open == LowLevelUsartPtr->getState());
	bool Silence = false;
	bool LimitsExceeded = false;
	usart_low_level_error Error = LowLevelUsartPtr->handleReceiveState(&Silence, &LimitsExceeded);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
		return;
	}
	if (Silence) {
		// Silence detected, indicating the end of the incoming transmission
		// Call the registered callback function
		(*RstlCallback)(LowLevelUsartPtr->getSendingBuffer(), LowLevelUsartPtr->getReceivingBuffer());
		CurrentState = rstl_master_fsm_state::IdleAndSend; // the command-response cycle successfully completed
		updateCommunicationQuality(SUCCESSFUL_COMMUNICATION);
		return;
	}
	if (LimitsExceeded) {
		CurrentState = rstl_master_fsm_state::Noise;
		updateCommunicationQuality(FAILED_COMMUNICATION);
		std::cout << "Noise (3)) #" << static_cast<int>(CurrentOrder.OrderCode) << 
		" (line " << __LINE__ << " in file " << __FILE__ << ")\n";
		return;
	}

	// Remain in the Receive state until either silence is detected or limits are exceeded
}

void modified_rstl_master::fsmTickNoise() {
	assert(usart_low_level_state::Open == LowLevelUsartPtr->getState());
	bool Silence = false;
	usart_low_level_error Error = LowLevelUsartPtr->handleNoiseState(&Silence);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
		return;
	}
	if (Silence) {
		// Silence detected, try getting back to IdleAndSend state
		CurrentState = rstl_master_fsm_state::IdleAndSend;
		return;
	}

	// Remain in the Noise state
}

void modified_rstl_master::updateCommunicationQuality(bool SuccessfulCommunication) {
	ResponseFailureHistory <<= 1;
	if (!SuccessfulCommunication) {
		ResponseFailureHistory |= 1;
		TotalFailedResponseCount++;
	}
	TotalResponseCount++;
}

usart_low_level_error initializeRstlMaster() {
	assert(nullptr == RstlMasterInstancePtr); // Ensure no previous instance exists
	RstlMasterInstancePtr = new modified_rstl_master();
	return RstlMasterInstancePtr->initialize();
}

bool rstlMasterTick(bool TerminationSignal) {
	assert(nullptr != RstlMasterInstancePtr); // Ensure the instance exists
	return RstlMasterInstancePtr->fsmTick(TerminationSignal);
}

bool placeRstlOrder(const order_with_argument &NewOrder) {
	assert(nullptr != RstlMasterInstancePtr); // Ensure the instance exists
	return RstlMasterInstancePtr->placeOrder(NewOrder);
}