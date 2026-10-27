/// @file usart_master.cpp
/// @brief Implementation of communication with a slave device based on the RP2040 microcontroller — 
/// lowest level.
///
/// @details This module provides USART support for sending and receiving data blocks (without
/// interpretation). The module acts as the master in relation to an external device that acts as the slave.
/// The module includes tools for detecting incoming data, including extracting incoming data blocks, 
/// detecting silence, and testing timeouts. The module determines the current status of the data block
/// exchange process and buffers the response from the slave. The module contains resources used by the 
/// higher-level FSM (shown in the diagram “Low-level USART FSM”).

#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

#include "debug_tools.h"
#include "usart_master.h"
#include "usart_master_debug.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define DEBUG_USART_MASTER 1

/// This constant determines USART speed; it is defined in termios.h
#define USART_HARDWARE_SPEED B4800

/// This constant determines the period of silence (in milliseconds) to detect the end of input data.
/// It is equal about 3.5 character times at the configured USART speed.
#define SILENCE_DETECTION_PERIOD_MS 7

//=================================================================================================
// Local function prototypes
//=================================================================================================

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName);

//=================================================================================================
// Public method definitions
//=================================================================================================

low_level_usart::low_level_usart() {
	LowLevelState = usart_low_level_state::Uninitialized;
	TransferState = usart_transfer_state::Idle;
	CriticalErrorCode = usart_low_level_error::None;
	NameOfPort = nullptr;
	SerialPortHandler = -1;
	memset(TotalResponse, 0, MAX_RESPONSE_LENGTH);
	TotalReceivedBytes = 0;
	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	TimeWhenReceivedLastByte = std::chrono::high_resolution_clock::now();
	WaitingLimit = (std::chrono::milliseconds)0;
	ResponseDurationLimit = (std::chrono::milliseconds)0;
}

low_level_usart::~low_level_usart() {
	if (SerialPortHandler != -1) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
	}
	if (NameOfPort != nullptr) {
		delete NameOfPort;
		NameOfPort = nullptr;
	}
}

/// @brief Open and configure the USART port with the specified name.
/// @param Name The name of the USART port to open
/// @return The error code indicating the result of the operation
/// @callgraph
/// @callergraph
usart_low_level_error low_level_usart::openUsart(const std::string &Name) {
	if (usart_low_level_state::Uninitialized != LowLevelState) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if (NameOfPort != nullptr) {
		delete NameOfPort;
		NameOfPort = nullptr;
		CriticalErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		CriticalErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if (usart_low_level_error::None != CriticalErrorCode) {
		LowLevelState = usart_low_level_state::Error;
		return CriticalErrorCode;
	}

	NameOfPort = new std::string(Name);

	static const char *PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK);
	if (0 != Result) {
		CriticalErrorCode = usart_low_level_error::UsartDeviceNotFound;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		LowLevelState = usart_low_level_state::Error;
		return CriticalErrorCode;
	}

	SerialPortHandler = configureSerialPort(PortNameCharPtr);
	if (-1 == SerialPortHandler) {
		CriticalErrorCode = usart_low_level_error::OpenFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		LowLevelState = usart_low_level_state::Error;
		return CriticalErrorCode;
	}
	LowLevelState = usart_low_level_state::Open;
	return CriticalErrorCode;
}

/// @brief Get the name of the USART port.
/// @return The name of the USART port as a string. If the port is not set, returns an empty string.
std::string low_level_usart::getName() const {
	if (NameOfPort != nullptr) {
		return *NameOfPort;
	}
	return "";
}

/// @brief Get the current low-level state of the USART master.
/// @callgraph
/// @callergraph
usart_low_level_state low_level_usart::getState() const { return LowLevelState; }

/// @brief Get the current transfer state of the USART master.
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::getTransferState() const { return TransferState; }

/// @brief Get the current error code of the USART master.
/// @callgraph
/// @callergraph
usart_low_level_error low_level_usart::getErrorCode() const { return CriticalErrorCode; }

/// @brief Reset the USART master to its normal operational state
/// @details This function assumes that the USART port is already open, there are no critical
/// errors, and no transmission is in progress. It resets the internal state variables to their default values.
/// @return True if the reset was successful, false otherwise.
/// @callgraph
/// @callergraph
bool low_level_usart::resetUsartMaster() {
#if 0
	if (State != usart_low_level_state::Open) {
		return false;
	}
#endif
#if 0
	if ((TransferState != usart_transfer_state::WaitingLimitExceeded) && (TransferState != usart_transfer_state::IncorrectlyCompleted)) {
		return false;
	}
	if (CriticalErrorCode != usart_low_level_error::None) {
		return false;
	}
#endif
#if 0
	if ((CriticalErrorCode != usart_low_level_error::None) && (TransferState != usart_transfer_state::InternalError) &&
	    (TransferState != usart_transfer_state::CallError)) {
		return false;
	}
#endif
	TransferState = usart_transfer_state::Idle;

	memset(TotalResponse, 0, MAX_RESPONSE_LENGTH);
	TotalReceivedBytes = 0;
	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	TimeWhenReceivedLastByte = std::chrono::high_resolution_clock::now();
	WaitingLimit = (std::chrono::milliseconds)0;
	ResponseDurationLimit = (std::chrono::milliseconds)0;
	return true;
}

/// @brief Send a given packet of data over the USART interface.
/// @details This function writes the specified data to the USART and sets up the waiting and response timeouts.
/// @param Buffer The buffer containing the data to send
/// @param Length The number of bytes to send from the buffer
/// @param WaitingTimeout The maximum time to wait for a response after sending the data
/// @param ResponseTimeout The maximum allowed duration for the response
/// @return The error code indicating the result of the operation
/// @note The function can consume large operating time depending on the Length parameter.
/// @callgraph
/// @callergraph
usart_low_level_error low_level_usart::sendOutputData(const char *Buffer, int Length, std::chrono::milliseconds WaitingTimeout,
                                                      std::chrono::milliseconds ResponseTimeout) {
	if (usart_low_level_state::Open != LowLevelState) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		return CriticalErrorCode;
	}

	WaitingLimit = WaitingTimeout;
	ResponseDurationLimit = ResponseTimeout;

	TransferState = usart_transfer_state::Sending;

	int BytesWritten = (int)write(SerialPortHandler, Buffer, Length);
	if (BytesWritten != Length) {
		CriticalErrorCode = usart_low_level_error::WriteFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		TransferState = usart_transfer_state::InternalError;
		return CriticalErrorCode;
	}

	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TransferState = usart_transfer_state::WaitingForResponse;
	return CriticalErrorCode;
}

/// @brief Collect input data from the USART until a period of silence is detected.
/// @details The function updates the TransferState member variable. The TransferState private member 
/// variable may remain in the WaitingForResponse or Receiving state. It may also transition to the following
/// states: Receiving, IncorrectlyReceiving, WaitingLimitExceeded, Completed, IncorrectlyCompleted,
/// InternalError, CallError. Other private member variables that may be updated are CriticalErrorCode and 
/// LowLevelState.
/// @return The current transfer state after checking for input or silence.
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::collectInputDataUntilSilence() {
	if (!checkNormalConditions()) {
		return TransferState;
	}

	const auto TimeNow = std::chrono::high_resolution_clock::now();
	return operateAssumingNormalConditions(TimeNow);
}

/// @brief Get the input data from the USART buffer assuming the transfer is completed.
/// @details This function copies the received data into the provided buffer if the transfer is completed.
/// @param Buffer The buffer to copy the received data into
/// @param BufferSize The size of the provided buffer
/// @return The number of bytes copied, or -1 on error
/// @callgraph
/// @callergraph
int low_level_usart::getInputData(char *Buffer, int BufferSize) {
	if ((usart_transfer_state::Completed != TransferState) || (BufferSize >= MAX_RESPONSE_LENGTH)) {
		CriticalErrorCode = usart_low_level_error::CallError;
		handleCriticalError(usart_transfer_state::CallError, __LINE__);
		return -1;
	}

	size_t Length = strlen(TotalResponse);
	if (Length >= (size_t)BufferSize) {

		// TODO: review
		return 0;
	}

	strncpy(Buffer, TotalResponse, BufferSize);
	TransferState = usart_transfer_state::Idle;
	return (int)Length;
}

//=================================================================================================
// Private method definitions
//=================================================================================================

/// @brief Check if the USART is ready to receive a response and that the module state variables
/// are consistent with normal operation.
/// @return True if the USART is in a normal state, false otherwise
/// @callgraph
/// @callergraph
bool low_level_usart::checkNormalConditions() {
	if (usart_low_level_state::Open != LowLevelState) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if ((usart_transfer_state::WaitingForResponse != TransferState) && (usart_transfer_state::Receiving != TransferState) &&
	    (usart_transfer_state::IncorrectlyReceiving != TransferState)) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d; TransferState [%d].\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__,
		       static_cast<int>(TransferState));
	}
	if ((usart_transfer_state::WaitingForResponse != TransferState) && (0 == TotalReceivedBytes)) {
		CriticalErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if (-1 == SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		CriticalErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if (usart_low_level_error::None != CriticalErrorCode) {
		handleCriticalError(usart_transfer_state::InternalError, __LINE__);
		return false;
	}
	return true;
}

/// @brief A collection of actions to take in the event of a critical error
/// @param NewTransferState The new transfer state to set after the failure
/// @param Line The line number where the failure occurred
/// @return The updated transfer state
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::handleCriticalError(usart_transfer_state NewTransferState, int Line) {
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
	}
	LowLevelState = usart_low_level_state::Error;
	delete NameOfPort;
	NameOfPort = nullptr;
	printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, Line);
	TransferState = NewTransferState;
	return TransferState;
}

/// @brief Collect input data from the USART until a period of silence is detected assuming normal conditions.
/// @details It is assumed that the USART is ready to receive a response and that the module state variables
/// are consistent with normal operation. The function updates the TransferState member variable accordingly. 
/// The TransferState private member variable may remain in the WaitingForResponse or Receiving state. It may
/// also transition to the following states: Receiving, IncorrectlyReceiving, WaitingLimitExceeded, Completed, 
/// IncorrectlyCompleted, InternalError, CallError.
/// @param TimeNow The current time point used for timeout calculations
/// @return The updated transfer state
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::operateAssumingNormalConditions(std::chrono::high_resolution_clock::time_point TimeNow) {
	fd_set SerialReadFds;
	FD_ZERO(&SerialReadFds);
	FD_SET(SerialPortHandler, &SerialReadFds);
	struct timeval SerialTimeout = {0, 0};
	int SerialPortState = select(SerialPortHandler + 1, &SerialReadFds, nullptr, nullptr, &SerialTimeout);

	if (SerialPortState == -1) {
		CriticalErrorCode = usart_low_level_error::SelectFailed;
		return handleCriticalError(usart_transfer_state::CallError, __LINE__);
	}
	if ((SerialPortState <= 0) || !FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		return handleSilence(TimeNow);
	}

	char ReceivedBytes[500];
	int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
	assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

	if (NumberOfReceived <= 0) {
		CriticalErrorCode = usart_low_level_error::ReadFailed;
		return handleCriticalError(usart_transfer_state::CallError, __LINE__);
	}

	return processReceivedInput(ReceivedBytes, NumberOfReceived, TimeNow);
}

// TODO: more advanced response time checking
/// @brief Process the received input data from the USART.
/// @details This function handles the received bytes, updates the transfer state, and checks for response 
/// duration limits. The function updates the TransferState member variable. The TransferState private member
/// variable may remain in the WaitingForResponse or Receiving state. It may also transition to the following
/// states: Receiving, IncorrectlyReceiving, InternalError.
/// @param ReceivedBytes Pointer to the buffer containing the received bytes
/// @param NumberOfReceived Number of bytes received
/// @param TimeNow The current time point used for timeout calculations
/// @return The updated transfer state
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::processReceivedInput(const char *ReceivedBytes, int NumberOfReceived,
                                                           std::chrono::high_resolution_clock::time_point TimeNow) {
	if (0 == TotalReceivedBytes) {
		assert(usart_transfer_state::WaitingForResponse == TransferState);
		TimeWhenResponseStarted = TimeNow;
		TransferState = usart_transfer_state::Receiving;
	}
	else {
		assert(usart_transfer_state::Receiving == TransferState);
		auto ReceivingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenResponseStarted);
		if (ReceivingDuration.count() > ResponseDurationLimit.count()) {
			TransferState = usart_transfer_state::IncorrectlyReceiving;
			return TransferState;
		}
	}

	for (int J = 0; J < NumberOfReceived; J++) {
		if (MAX_RESPONSE_LENGTH - 1 > TotalReceivedBytes) {
			TotalResponse[TotalReceivedBytes] = ReceivedBytes[J];
			TotalReceivedBytes++;
			continue;
		}

		CriticalErrorCode = usart_low_level_error::InternalError;
		return handleCriticalError(usart_transfer_state::InternalError, __LINE__);
	}

	TotalResponse[TotalReceivedBytes] = 0;
	TimeWhenReceivedLastByte = TimeNow;
	return TransferState;
}

// TODO: improve limit calculation while bytes are being received
/// @brief Handle silence on the USART line.
/// @details This function checks for silence duration and updates the TransferState member variable 
/// accordingly. The TransferState private member variable may remain in the WaitingForResponse state or 
/// transition to the following states: WaitingLimitExceeded, Completed, IncorrectlyCompleted, InternalError.
/// @param TimeNow The current time point used for silence detection
/// @return The updated transfer state
/// @callgraph
/// @callergraph
usart_transfer_state low_level_usart::handleSilence(std::chrono::high_resolution_clock::time_point TimeNow) {
	if (0 == TotalReceivedBytes) {
		assert(usart_transfer_state::WaitingForResponse == TransferState);

		auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenOutgoingDataEnded);
		if (SilenceDuration.count() > WaitingLimit.count()) {
			TransferState = usart_transfer_state::WaitingLimitExceeded;
		}
		return TransferState;
	}

	auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenReceivedLastByte);
	if (SilenceDuration.count() <= SILENCE_DETECTION_PERIOD_MS) {
		return TransferState;
	}
	if (usart_transfer_state::Receiving == TransferState) {
		TransferState = usart_transfer_state::Completed;
		return TransferState;
	}
	if (usart_transfer_state::IncorrectlyReceiving == TransferState) {
		TransferState = usart_transfer_state::IncorrectlyCompleted;
		return TransferState;
	}

	TransferState = usart_transfer_state::InternalError;
	return TransferState;
}

//=================================================================================================.......
// Local function definitions
//=================================================================================================.......

/// @brief Open and configure a serial port
/// @param DeviceName The name of the serial port device to open
/// @return The file handler for the opened serial port, or -1 on failure
/// @callgraph
/// @callergraph
static int configureSerialPort(const char *DeviceName) {
	int FileHandler;
	struct termios PortSettings;

	FileHandler = open(DeviceName, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (FileHandler == -1) {
		return -1;
	}

	if (tcgetattr(FileHandler, &PortSettings) != 0) {
		close(FileHandler);
		return -1;
	}

	cfsetispeed(&PortSettings, USART_HARDWARE_SPEED);
	cfsetospeed(&PortSettings, USART_HARDWARE_SPEED);

	PortSettings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
	PortSettings.c_oflag = 0; // Disable output processing
	PortSettings.c_lflag = 0; // Mode 'raw'

	PortSettings.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CRTSCTS);
	PortSettings.c_cflag |= (CS8 | CREAD | CLOCAL);

	PortSettings.c_cc[VMIN] = 0;  // Minimum number of bytes to read
	PortSettings.c_cc[VTIME] = 0; // Timeout in tenth of a second

	// apply the configuration
	if (tcsetattr(FileHandler, TCSANOW, &PortSettings) != 0) {
		close(FileHandler);
		return -1;
	}

	return FileHandler;
}
