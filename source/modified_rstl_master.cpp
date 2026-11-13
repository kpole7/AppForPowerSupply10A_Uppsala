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
#include <cstring>
#include <iostream>

#include "modified_rstl_master.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"
#include "settings_file.h"
#include "usart_master.h"
#include "usart_master_debug.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define SUCCESSFUL_COMMUNICATION true
#define FAILED_COMMUNICATION false

#define ABSOLUTE_MINIMUM_SETTING (-1000) // in hundredths of an ampere
#define ABSOLUTE_MAXIMUM_SETTING 1000    // in hundredths of an ampere

//=================================================================================================
// Types
//=================================================================================================

enum class external_device_states : uint8_t {
	PSU_STOPPED,					// stable state; power supply is turned off
	PSU_INITIAL_SIG2_LOW_SET_DAC,	// transitional state; during power-up
	PSU_INITIAL_SIG2_LOW_TEST,		// transitional state; during power-up
	PSU_INITIAL_SIG2_HIGH_SET_DAC,	// transitional state; during power-up
	PSU_INITIAL_SIG2_HIGH_TEST,		// transitional state; during power-up
	PSU_INITIAL_ZEROING,			// transitional state; during power-up
	PSU_INITIAL_CONTACTOR_ON,		// transitional state; during power-up
	PSU_RUNNING,					// stable state; power supply is turned on
	PSU_SHUTTING_DOWN_ZEROING,		// transitional state; during power-down
	PSU_SHUTTING_DOWN_CONTACTOR_OFF,// transitional state; during power-down
	PSU_ILLEGAL_STATE				// number of correct states
};

//=================================================================================================
// Local constants and variables
//=================================================================================================

/// @brief Response length limits for each single order code.
/// @note The index corresponds to the single_order_code enum value.
static const uint8_t ResponseLengthLimits[] = {
    0,  // Placeholder for index 0
    12, // InquiryPower
    9,  // SetPower
    10, // InquiryZ
    5,  // SetZ
    14, // InquiryPC
    13, // SetPC
    50, // InquiryST (44+6=50)
    38, // InquiryVersion
    23, // ResetErrors
};

static rstl_callback_function RstlCallback = nullptr;

static modified_rstl_master_ptr RstlMasterInstancePtr = nullptr;

//=================================================================================================
// Local function declarations
//=================================================================================================

static main_power_state convertToExternalDeviceSimplifiedState(uint16_t ExternalDevicedState);

static void printAllCharacters(const char *Buffer);

//=================================================================================================
// Public method definitions
//=================================================================================================

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
	case single_order_code::SetPC:
		placeOrderSetPC(NewOrder);
		break;
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
		std::cout << "Unsupported order code #" << static_cast<int>(NewOrder.OrderCode) << " (line " << __LINE__ << " in file " << __FILE__ << ")\n";
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

//=================================================================================================
// Private method definitions
//=================================================================================================

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
	std::cout << "Noise (1)) #" << static_cast<int>(CurrentOrder.OrderCode) << " (line " << __LINE__ << " in file " << __FILE__ << ")\n";
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
		std::cout << "Noise (2)) #" << static_cast<int>(CurrentOrder.OrderCode) << " (line " << __LINE__ << " in file " << __FILE__ << ")\n";
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

		int16_t ValueReturned;
		uint8_t EssentialStart, EssentialLength;
		bool IsResponseSyntaxCorrect = parseResponse(ValueReturned, EssentialStart, EssentialLength);
		const char *ReceivingBuffer = LowLevelUsartPtr->getReceivingBuffer();
		assert(ReceivingBuffer != nullptr);
		ReceivingBuffer += EssentialStart;

		if(!IsResponseSyntaxCorrect) {
			std::cout << "Response syntax incorrect for order #" << static_cast<int>(CurrentOrder.OrderCode) << " (line " << __LINE__ << " in file " << __FILE__ << ")\n";
		}

		// Call the registered callback function
		(*RstlCallback)(IsResponseSyntaxCorrect, ValueReturned, ReceivingBuffer, EssentialLength);
		CurrentState = rstl_master_fsm_state::IdleAndSend; // the command-response cycle successfully completed
		updateCommunicationQuality(SUCCESSFUL_COMMUNICATION);
		return;
	}
	if (LimitsExceeded) {
		CurrentState = rstl_master_fsm_state::Noise;
		updateCommunicationQuality(FAILED_COMMUNICATION);
		std::cout << "Noise (3)) #" << static_cast<int>(CurrentOrder.OrderCode) << " (line " << __LINE__ << " in file " << __FILE__ << ")\n";
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

void modified_rstl_master::placeOrderSetPC(const order_with_argument &NewOrder) {
	assert(NewOrder.Argument >= ABSOLUTE_MINIMUM_SETTING);
	assert(NewOrder.Argument <= ABSOLUTE_MAXIMUM_SETTING);
	bool IsNegative;
	int16_t AbsoluteValue;
	if (NewOrder.Argument >= 0) {
		IsNegative = false;
		AbsoluteValue = NewOrder.Argument;
	}
	else {
		IsNegative = true;
		AbsoluteValue = static_cast<int16_t>(-NewOrder.Argument);
	}
	int16_t BeforePoint = static_cast<int16_t>(AbsoluteValue / 100);
	int16_t AfterPoint = static_cast<int16_t>(AbsoluteValue % 100);
	if (IsNegative) {
		CommandInTextFormat = "PC -" + std::to_string(BeforePoint) + "." + (AfterPoint < 10 ? "0" : "") + std::to_string(AfterPoint) + "\r\n";
	}
	else {
		CommandInTextFormat = "PC " + std::to_string(BeforePoint) + "." + (AfterPoint < 10 ? "0" : "") + std::to_string(AfterPoint) + "\r\n";
	}
}

bool modified_rstl_master::parseResponse(int16_t &ValueReturnedInResponse, uint8_t &EssentialResponseStart, uint8_t &EssentialResponseLength) const {
	const char *SendingBuffer = LowLevelUsartPtr->getSendingBuffer();
	assert(SendingBuffer != nullptr);
	const char *ReceivingBuffer = LowLevelUsartPtr->getReceivingBuffer();
	assert(ReceivingBuffer != nullptr);

#if 0
	std::cout << "  SendingBuffer: ";
	printAllCharacters(SendingBuffer);
	std::cout << "ReceivingBuffer: ";
	printAllCharacters(ReceivingBuffer);
	std::cout << '\n';
#endif

	int SendingBufferLength = static_cast<int>(strlen(SendingBuffer));
	int ReceivingBufferLength = static_cast<int>(strlen(ReceivingBuffer));
	if (ReceivingBufferLength <= SendingBufferLength) {
		return false;
	}

	char *SearchResult = const_cast<char *>(strstr(ReceivingBuffer, SendingBuffer));
	if (SearchResult != ReceivingBuffer) {
		return false;
	}
	ReceivingBuffer += SendingBufferLength;

#if 0
	std::cout << "Essentials: ";
	printAllCharacters(ReceivingBuffer);
	std::cout << '\n';
#endif

	ValueReturnedInResponse = 0;
	EssentialResponseStart = 0;
	EssentialResponseLength = 0;
	switch (CurrentOrder.OrderCode) {
	case single_order_code::SetPC:
	case single_order_code::SetPower:
	case single_order_code::SetZ: {
		bool Success = (0 == strncmp(ReceivingBuffer, ">", 2));
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
	} break;

	case single_order_code::ResetErrors: {
		bool Success = (0 == strncmp(ReceivingBuffer, "Resetting errors\r\n>", 20));
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
	} break;

	case single_order_code::InquiryPower: {
		bool Success = testAndExtractOneDigit(ReceivingBuffer, "#\r\n>", 5, ValueReturnedInResponse, 1, 0, 1);
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
	} break;

	case single_order_code::InquiryZ: {
		bool Success = testAndExtractOneDigit(ReceivingBuffer, "Z=#\r\n>", 7, ValueReturnedInResponse, 3, 1, PSU_NUMBER_OF_CHANNELS);
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
	} break;

	case single_order_code::InquiryVersion: {
		bool Success = parseInquiryVersion(ReceivingBuffer, EssentialResponseLength);
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
		EssentialResponseStart = 9;
	} break;

	case single_order_code::InquiryST: {
		bool Success = parseInquiryStatus(ReceivingBuffer, ValueReturnedInResponse, EssentialResponseLength);
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
		EssentialResponseStart = 4;
		main_power_state ExternalDeviceSimplifiedState = convertToExternalDeviceSimplifiedState(ValueReturnedInResponse);
		atomic_store_explicit(&SharedMainPowerState, ExternalDeviceSimplifiedState, std::memory_order_release);
		AwakeRefreshWholeGui = true;
	} break;

	case single_order_code::InquiryPC: {
		bool Success = parseInquiryPC(ReceivingBuffer, ValueReturnedInResponse);
		if (!Success) {
			printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
			return false;
		}
	} break;

	default:
		printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
		return false;
	}

	return true;
}

bool modified_rstl_master::testAndExtractOneDigit(const char *BufferUnderTest, const char *ConstantPhrase, size_t Length, int16_t &ValueExtracted,
                                                  uint8_t Position, uint8_t MinValue, uint8_t MaxValue) const {
	if (0 != strncmp(BufferUnderTest, ConstantPhrase, Position - 1)) {
		printf("Response syntax error in %s at line %d; [", __FILE__, __LINE__);
		printAllCharacters(BufferUnderTest);
		printf("] != [");
		printAllCharacters(ConstantPhrase);
		printf("] len. %d\n", (int)(Position - 1));
		return false;
	}
	if (0 != strncmp(BufferUnderTest + Position + 1, ConstantPhrase + Position + 1, Length - Position)) {
		printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
		return false;
	}
	if ((BufferUnderTest[Position - 1] < '0') || (BufferUnderTest[Position - 1] > '9')) {
		printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
		return false;
	}
	ValueExtracted = BufferUnderTest[Position - 1] - '0';
	if ((ValueExtracted < MinValue) || (ValueExtracted > MaxValue)) {
		printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
		return false;
	}
	return true;
}

bool modified_rstl_master::parseInquiryVersion(const char *BufferUnderTest, uint8_t &EssentialResponseLength) const {
	// Example:
	// Command:  [VERSION~~]
	// Response: [VERSION~~ver. Oct  8 2026, 15:04:13~~>]
	// BufferUnderTest: "ver. Oct  8 2026, 15:04:13\r\n>"
	int RequiredLength = 29;
	if ((int)strlen(BufferUnderTest) != RequiredLength) {
		printf("Response syntax error in %s at line %d; [", __FILE__, __LINE__);
		printAllCharacters(BufferUnderTest);
		printf("]\n");
		return false;
	}
	const char Form[] = "ver. Oct  8 2026, 15:04:13\r\n>";
	const char Mask[] = "ver. Www pd dddd, dd:dd:dd\r\n>";
	for (int i = 0; i < RequiredLength; ++i) {
		if (Mask[i] == 'W') {
			if (0 == isupper(BufferUnderTest[i])) {
				printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
				return false;
			}
		}
		else if (Mask[i] == 'w') {
			if (0 == islower(BufferUnderTest[i])) {
				printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
				return false;
			}
		}
		else if (Mask[i] == 'd') {
			if (0 == isdigit(BufferUnderTest[i])) {
				printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
				return false;
			}
		}
		else if (Mask[i] == 'p') {
			if ((0 == isdigit(BufferUnderTest[i])) && (BufferUnderTest[i] != ' ')) {
				printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
				return false;
			}
		}
		else {
			if (BufferUnderTest[i] != Form[i]) {
				printf("Response syntax error in %s at line %d.\n", __FILE__, __LINE__);
				return false;
			}
		}
	}
	EssentialResponseLength = 26;
	return true;
}

bool modified_rstl_master::parseInquiryStatus(const char *BufferUnderTest, int16_t &ValueExtracted, uint8_t &EssentialResponseLength) const {
	// Example:
	// Command:  [ST~~]
	// Response: [ST~~sig2 ?? ?? ?? ?? i2c 0 0 uart 0 fsm 0~~>]
	// Response: [ST~~sig2 LH LH LH LH i2c 0 0 uart 0 fsm 8~~>]
	//                1234567890123456789012345678901234567890
	// BufferUnderTest: "sig2 LH LH LH LH i2c 0 0 uart 0 fsm 8\r\n>"
	int MeasuredLength = strlen(BufferUnderTest);
	if (MeasuredLength < 40) {
		return false;
	}
	if (MeasuredLength > 46) {
		return false;
	}
	if (BufferUnderTest != strstr(BufferUnderTest, "sig2 ")) {
		return false;
	}
	if (nullptr == strstr(BufferUnderTest, " i2c ")) {
		return false;
	}
	if (nullptr == strstr(BufferUnderTest, " uart ")) {
		return false;
	}
	const char *FsmPtr = strstr(BufferUnderTest, " fsm ");
	if (nullptr == FsmPtr) {
		return false;
	}
	if (BufferUnderTest + MeasuredLength - 3 != strstr(BufferUnderTest, "\r\n>")) {
		return false;
	}
	ValueExtracted = static_cast<int16_t>(strtol(FsmPtr + 5, nullptr, 10));
	EssentialResponseLength = MeasuredLength - 3;
	return true;
}

bool modified_rstl_master::parseInquiryPC(const char *BufferUnderTest, int16_t &ValueReturnedInResponse) const {
	// Example:
	// Command:  [PC~~]
	// Response: [?PC~~-10.00~~>]
	// Response: [?PC~~-3.00~~>]
	// Response: [?PC~~0.00~~>]
	// Response: [?PC~~2.00~~>]
	// Response: [?PC~~10.00~~>]
	//                 123456789
	int MeasuredLength = strlen(BufferUnderTest);
	if (MeasuredLength < 7) {
		return false;
	}
	if (MeasuredLength > 9) {
		return false;
	}
	const char *DotPtr = strchr(BufferUnderTest, '.');
	if (nullptr == DotPtr) {
		return false;
	}
	if (DotPtr - BufferUnderTest < 1) {
		return false;
	}
	const char *EndingPtr = strstr(BufferUnderTest, "\r\n>");
	if (nullptr == EndingPtr) {
		return false;
	}
	if (EndingPtr - DotPtr != 3) {
		return false;
	}
	char *EndPtr = nullptr;
	int16_t Value1 = static_cast<int16_t>(strtol(BufferUnderTest, &EndPtr, 10));
	if (EndPtr != DotPtr) {
		return false;
	}
	int16_t Value2 = static_cast<int16_t>(strtol(DotPtr + 1, &EndPtr, 10));
	if (EndPtr != EndingPtr) {
		return false;
	}

	if (BufferUnderTest[0] != '-') {
		ValueReturnedInResponse = Value1 * 100 + Value2;
	}
	else {
		ValueReturnedInResponse = Value1 * 100 - Value2;
	}
	if ((ValueReturnedInResponse < ABSOLUTE_MINIMUM_SETTING) || (ValueReturnedInResponse > ABSOLUTE_MAXIMUM_SETTING)) {
		return false;
	}

	return true;
}

//=================================================================================================
// Definitions of public functions
//=================================================================================================

/// @brief Registers a callback function for handling RSTL commands.
/// @param Callback The callback function to be registered.
/// @note This function can be called only once to register the callback.
void registerRstlCallback(rstl_callback_function Callback) {
	assert(nullptr == RstlCallback);
	assert(nullptr != Callback);
	RstlCallback = Callback;
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

//=================================================================================================
// Definitions of local functions
//=================================================================================================

static main_power_state convertToExternalDeviceSimplifiedState(uint16_t ExternalDevicedState) {
	external_device_states ExternalState = static_cast<external_device_states>(ExternalDevicedState);

	switch (ExternalState) {
		case external_device_states::PSU_STOPPED:
			return main_power_state::OFF;

		case external_device_states::PSU_INITIAL_SIG2_LOW_SET_DAC:
		case external_device_states::PSU_INITIAL_SIG2_LOW_TEST:
		case external_device_states::PSU_INITIAL_SIG2_HIGH_SET_DAC:
		case external_device_states::PSU_INITIAL_SIG2_HIGH_TEST:
		case external_device_states::PSU_INITIAL_ZEROING:
		case external_device_states::PSU_INITIAL_CONTACTOR_ON:
			return main_power_state::RISING;

		case external_device_states::PSU_RUNNING:
			return main_power_state::ON;

		case external_device_states::PSU_SHUTTING_DOWN_ZEROING:
		case external_device_states::PSU_SHUTTING_DOWN_CONTACTOR_OFF:
			return main_power_state::FALLING;

		default:
		break;
	}
	return main_power_state::UNDEFINED; // failure case
}

static void printAllCharacters(const char *Buffer) {
	assert(Buffer != nullptr);
	for (size_t i = 0; Buffer[i] != '\0'; ++i) {
		if (Buffer[i] == '\r') {
			std::cout << "\\r";
		}
		else if (Buffer[i] == '\n') {
			std::cout << "\\n";
		}
		else if ((Buffer[i] < (char)32) || ((uint8_t)Buffer[i] > 127)) {
			std::cout << "\\x" << std::hex << static_cast<int>(static_cast<unsigned char>(Buffer[i])) << std::dec;
		}
		else {
			std::cout << Buffer[i];
		}
	}
}
