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

#include "modified_rstl_master.h"
#include "usart_master.h"
#include "usart_master_debug.h"


static modified_rstl_master_ptr RstlMasterInstancePtr = nullptr;


modified_rstl_master::modified_rstl_master() {
	CurrentState = rstl_master_fsm_state::Uninitialized;
	LowLevelUsartPtr = new low_level_usart();
	OrderProcessingState = order_processing_state::Ready;
	ResponseFailureHistory = 0;
	TotalResponseCount = 0;
	TotalFailedResponseCount = 0;
}

modified_rstl_master::~modified_rstl_master() { 
	delete LowLevelUsartPtr;
}

usart_low_level_error modified_rstl_master::initialize() {
	std::string PortName = "/dev/ttyUSB0";
	CurrentState = rstl_master_fsm_state::IdleAndSend;
	usart_low_level_error Error = LowLevelUsartPtr->startModule(PortName);
	if (Error != usart_low_level_error::None) {
		CurrentState = rstl_master_fsm_state::CriticalError;
	}
	return Error;
}

rstl_master_fsm_state modified_rstl_master::getCurrentState() const {
	return CurrentState;
}

usart_low_level_error modified_rstl_master::getUsartErrorCode() const {
	return LowLevelUsartPtr->getErrorCode();
}

order_processing_state modified_rstl_master::getOrderProcessingState() const {
	return OrderProcessingState;
}

order_with_arguments modified_rstl_master::getCurrentOrder() const {
	return CurrentOrder;
}

void modified_rstl_master::placeOrder(const order_with_arguments &NewOrder) {
	CurrentOrder = NewOrder;
	OrderProcessingState = order_processing_state::InProgress;
}

std::string modified_rstl_master::getResponse() const {
	return ResponseBuffer;
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
	assert(LowLevelUsartPtr != nullptr);

	switch (CurrentState) {
		case rstl_master_fsm_state::Uninitialized:
			{
				assert(usart_low_level_state::Uninitialized == LowLevelUsartPtr->getState());
				bool *IsSilencePtr = nullptr;
				LowLevelUsartPtr->handleIdleAndSendState(IsSilencePtr, CurrentCommandTextPtr, CurrentCommandTextLength);


			}
			break;
		case rstl_master_fsm_state::IdleAndSend:
			// Handle idle and send state
			break;
		case rstl_master_fsm_state::WaitForResponse:
			// Handle wait for response state
			break;
		case rstl_master_fsm_state::Receive:
			// Handle receive state
			break;
		case rstl_master_fsm_state::Noise:
			// Handle noise state
			break;
		case rstl_master_fsm_state::CriticalError:
			// Handle critical error state
			break;
	}



	return true;
}

void modified_rstl_master::updateCommunicationQuality(bool ResponseFailed) {
	ResponseFailureHistory <<= 1;
	if (ResponseFailed) {
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
	if (TerminationSignal) {
		if (nullptr == RstlMasterInstancePtr) {
			return false; // Indicate that the peripheral operations should not continue
		}
		delete RstlMasterInstancePtr;
		return false; // Indicate that the peripheral operations should not continue
	}
	assert(nullptr != RstlMasterInstancePtr); // Ensure the instance exists
	RstlMasterInstancePtr->fsmTick(false);
	return true;
}
