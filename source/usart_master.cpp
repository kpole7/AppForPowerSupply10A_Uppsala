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

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define DEBUG_USART_MASTER 1

/// This constant determines USART speed; it is defined in termios.h
#define USART_HARDWARE_SPEED B4800

/// This constant determines the period of silence (in milliseconds) to detect the end of input data.
/// It is equal about 3.5 character times at the configured USART speed.
#define SILENCE_DETECTION_PERIOD_MS 7

static_assert(SILENCE_DETECTION_PERIOD_MS < 1 * PERIPHERAL_THREAD_LOOP_DURATION,
              "A single period of the peripheral thread is not sufficient for silence detection");

//=================================================================================================
// Local function prototypes
//=================================================================================================

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName);

//=================================================================================================
// Private method definitions
//=================================================================================================

/// @brief A collection of actions to take in the event of a critical error
/// @param Line The line number where the failure occurred
/// @callgraph
/// @callergraph
void low_level_usart::handleCriticalError(int Line) {
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
	}
	UsartState = usart_low_level_state::Error;
	delete NameOfPort;
	NameOfPort = nullptr;
	printf("USART Error %d in %s at line %d.\n", static_cast<int>(UsartErrorCode), __FILE__, Line);
}

/// @brief Check for silence on the USART line
/// @param IsSilencePtr Pointer to a boolean that will be set to true if the line is silent, false otherwise.
void low_level_usart::checkSilence(bool *IsSilencePtr) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	fd_set SerialReadFds;
	FD_ZERO(&SerialReadFds);
	FD_SET(SerialPortHandler, &SerialReadFds);
	struct timeval SerialTimeout = {0, 0};
	int SerialPortState = select(SerialPortHandler + 1, &SerialReadFds, nullptr, nullptr, &SerialTimeout);

	if (SerialPortState == -1) {
		UsartErrorCode = usart_low_level_error::SelectFailed;
		handleCriticalError(__LINE__);
		return;
	}
	if ((SerialPortState <= 0) || !FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		// there is silence on the line
		*IsSilencePtr = true;
	}
	else {
		*IsSilencePtr = false;
	}
}

void low_level_usart::flushBuffer() {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	char ReceivedBytes[500];
	int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
	assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

	if (NumberOfReceived <= 0) {
		UsartErrorCode = usart_low_level_error::ReadFailed;
		handleCriticalError(__LINE__);
	}
	return;
}

/// @brief Check for silence on the USART line and prepare a new command for transmission if CommandTextPtr is not nullptr.
/// @param IsSilencePtr Pointer to a boolean that will be set to true if the line is silent, false otherwise
/// @param CommandTextPtr The command text to be sent over the USART line or nullptr if no new command is to be sent
/// @param ResponseLimit The maximum number of bytes expected in the response
void low_level_usart::doIdle(bool *IsSilencePtr, const char *CommandTextPtr, uint16_t ResponseLimit) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	assert(USART_BUFFER_SIZE - 1 > ResponseLimit);

	TotalReceivedBytes = 0;
	ReceivedBytesLimit = ResponseLimit;

	checkSilence(IsSilencePtr);
	if (usart_low_level_error::None != UsartErrorCode) {
		// in case of error
		return;
	}
	if (!(*IsSilencePtr)) {
		// there is noise on the line
		flushBuffer();
		return;
	}

	if (nullptr != CommandTextPtr) {
		// there is a new command to send
		TotalCommandBytes = strlen(CommandTextPtr);
		assert(TotalCommandBytes < USART_BUFFER_SIZE);
		strcpy(UsartBuffer, CommandTextPtr);
	}
	else {
		// no new command to send
		TotalCommandBytes = 0;
	}
}

/// @brief Send a given packet of data over the USART interface.
/// @details This function writes the specified data to the USART and sets up the waiting and response timeouts.
/// @param Buffer The buffer containing the data to send
void low_level_usart::doSending() {
	assert(usart_low_level_state::Open == UsartState);

	int BytesWritten = (int)write(SerialPortHandler, UsartBuffer, TotalCommandBytes);
	if (BytesWritten != TotalCommandBytes) {
		UsartErrorCode = usart_low_level_error::WriteFailed;
		UsartState = usart_low_level_state::Error;
		return;
	}

	TimeWhenOutgoingDataStarted = std::chrono::high_resolution_clock::now();
	TotalReceivedBytes = 0;
	return;
}

/// @brief Wait for a response from the USART interface.
/// @param IsTimeoutPtr Pointer to a boolean that will be set to true if a timeout occurs
/// @param IsSilencePtr Pointer to a boolean that will be set to true if silence is detected on the line
void low_level_usart::doWaitForResponse(bool *IsTimeoutPtr, bool *IsSilencePtr) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	assert(0 == TotalReceivedBytes);

	*IsTimeoutPtr = false;
	*IsSilencePtr = false;
	const auto TimeNow = std::chrono::high_resolution_clock::now();

	checkSilence(IsSilencePtr);
	if (usart_low_level_error::None != UsartErrorCode) {
		return;
	}
	if (*IsSilencePtr) {
		// no bytes have been received yet, check if waiting time has exceeded the limit
		auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenOutgoingDataStarted);
		if (SilenceDuration.count() > WaitingLimit.count()) {
			// waiting time has exceeded the limit
			*IsTimeoutPtr = true;
		}
		return;
	}
	return;
}

/// @brief Receive the data from the USART interface assuming there is incoming data available
/// @param IsOverLimitsPtr Pointer to a boolean that will be set to true if the number of received bytes exceeds the limit
void low_level_usart::doReceiveFirstData(bool *IsOverLimitsPtr) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	assert(0 == TotalReceivedBytes);

	*IsOverLimitsPtr = false;

	char ReceivedBytes[500];
	int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
	assert(NumberOfReceived <= (int)sizeof(ReceivedBytes)); // This condition should always be met,
	                                                        // because the data collection period is short

	if (NumberOfReceived <= 0) {
		UsartErrorCode = usart_low_level_error::ReadFailed;
		handleCriticalError(__LINE__);
		return;
	}

	if (NumberOfReceived > ReceivedBytesLimit) {
		*IsOverLimitsPtr = true;
		return;
	}

	// Copy the received bytes into the USART buffer
	for (int J = 0; J < NumberOfReceived; J++) {
		UsartBuffer[TotalReceivedBytes] = ReceivedBytes[J];
		TotalReceivedBytes++;
	}
	UsartBuffer[TotalReceivedBytes] = 0;

	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	return;
}

//=================================================================================================
// Public method definitions
//=================================================================================================

/// @brief Constructor for the low_level_usart class
low_level_usart::low_level_usart() {
	UsartState = usart_low_level_state::Uninitialized;
	UsartErrorCode = usart_low_level_error::None;
	NameOfPort = nullptr;
	SerialPortHandler = -1;
	memset(UsartBuffer, 0, USART_BUFFER_SIZE);
	TotalReceivedBytes = 0;
	TimeWhenOutgoingDataStarted = std::chrono::high_resolution_clock::now();
	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	WaitingLimit = (std::chrono::milliseconds)0;
}

/// @brief Destructor for the low_level_usart class
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
usart_low_level_error low_level_usart::startModule(const std::string &Name) {
	assert(usart_low_level_state::Uninitialized == UsartState);
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(NameOfPort == nullptr);
	assert(-1 == SerialPortHandler);

	NameOfPort = new std::string(Name);

	static const char *PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK);
	if (0 != Result) {
		UsartErrorCode = usart_low_level_error::UsartDeviceNotFound;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(UsartErrorCode), __FILE__, __LINE__);
		UsartState = usart_low_level_state::Error;
		return UsartErrorCode;
	}

	SerialPortHandler = configureSerialPort(PortNameCharPtr);
	if (-1 == SerialPortHandler) {
		UsartErrorCode = usart_low_level_error::OpenFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(UsartErrorCode), __FILE__, __LINE__);
		UsartState = usart_low_level_state::Error;
		return UsartErrorCode;
	}
	UsartState = usart_low_level_state::Open;
	return UsartErrorCode;
}

/// @brief Get the name of the USART port.
/// @return The name of the USART port as a string. If the port is not set, returns an empty string.
std::string low_level_usart::getName() const {
	const std::string EmptyString = "";
	if (NameOfPort != nullptr) {
		return *NameOfPort;
	}
	return EmptyString;
}

/// @brief Get the current low-level state of the USART master.
/// @callgraph
/// @callergraph
usart_low_level_state low_level_usart::getState() const { return UsartState; }

/// @brief Get the current error code of the USART master.
/// @callgraph
/// @callergraph
usart_low_level_error low_level_usart::getErrorCode() const { return UsartErrorCode; }

/// @brief Get the input data from the USART buffer assuming the transfer is completed.
/// @details This function copies the received data into the provided buffer if the transfer is completed.
/// @param HigherLevelBuffer The buffer to copy the received data into
/// @param BufferSize The size of the provided buffer
/// @return The number of bytes copied, or -1 on error
/// @callgraph
/// @callergraph
usart_low_level_error low_level_usart::getInputData(int *ReceivedBytes, char *HigherLevelBuffer, int BufferSize) {
	if (BufferSize >= USART_BUFFER_SIZE) {
		UsartErrorCode = usart_low_level_error::CallError;
		handleCriticalError(__LINE__);
		return UsartErrorCode;
	}

	size_t Length = strlen(UsartBuffer);
	if (Length >= (size_t)BufferSize) {

		// TODO: review
		*ReceivedBytes = 0;
		UsartErrorCode = usart_low_level_error::CallError;
		handleCriticalError(__LINE__);
		return UsartErrorCode;
	}

	strncpy(HigherLevelBuffer, UsartBuffer, BufferSize);
	*ReceivedBytes = (int)Length;
	return UsartErrorCode;
}

/// @brief Handle the IdleAndSend state of the Low-level USART FSM.
/// @param IsSilencePtr Pointer to a boolean that will be set to true if silence is detected on the line
/// @param CommandTextPtr Pointer to the command text to be sent or nullptr if no command is to be sent
/// @param ResponseLimit The maximum number of response bytes expected if a command is to be sent; should be 0 if no command is to be sent
/// @return The error code indicating the result of the operation
usart_low_level_error low_level_usart::handleIdleAndSendState(bool *IsSilencePtr, const char *CommandTextPtr, uint16_t ResponseLimit) {
	bool IsSilence = false;
	doIdle(&IsSilence, CommandTextPtr, ResponseLimit);
	if (usart_low_level_error::None != UsartErrorCode) {
		return UsartErrorCode;
	}
	*IsSilencePtr = IsSilence;
	if (!IsSilence) {
		return UsartErrorCode;
	}

	if (nullptr == CommandTextPtr) {
		assert(0 == ResponseLimit);
		return UsartErrorCode;
	}
	assert(0 != ResponseLimit);

	doSending();
	return UsartErrorCode;
}

/// @brief Handle the WaitForResponse state of the Low-level USART FSM.
/// @param IsTimeoutExceededPtr Pointer to a boolean that will be set to true if the response timeout is exceeded (no response received within the
/// expected time)
/// @param IsSilencePtr Pointer to a boolean that will be set to true if silence is detected on the line
/// @param IsOverLimitsPtr Pointer to a boolean that will be set to true if the number of received bytes exceeds the limit
/// @return The error code indicating the result of the operation
usart_low_level_error low_level_usart::handleWaitForResponseState(bool *IsTimeoutExceededPtr, bool *IsSilencePtr, bool *IsOverLimitsPtr) {
	*IsSilencePtr = false;
	*IsOverLimitsPtr = false;
	doWaitForResponse(IsTimeoutExceededPtr, IsSilencePtr);
	if (usart_low_level_error::None != UsartErrorCode) {
		return UsartErrorCode;
	}
	if ((*IsTimeoutExceededPtr) || (*IsSilencePtr)) {
		return UsartErrorCode;
	}

	doReceiveFirstData(IsOverLimitsPtr);
	return UsartErrorCode;
}

/// @brief Handle the Receive state of the Low-level USART FSM.
/// @param IsSilencePtr Pointer to a boolean that will be set to true if silence is detected on the line
/// @param IsOverLimitsPtr A pointer to a boolean variable that will be set to “true” if the number of bytes received exceeds
/// the limit or if there are short pauses in the response and their total duration is too long
/// @return The error code indicating the result of the operation
usart_low_level_error low_level_usart::handleReceiveState(bool *IsSilencePtr, bool *IsOverLimitsPtr) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);
	assert(0 != TotalReceivedBytes);

	*IsOverLimitsPtr = false;

	checkSilence(IsSilencePtr);
	if (usart_low_level_error::None != UsartErrorCode) {
		return UsartErrorCode;
	}
	if (*IsSilencePtr) {
		return UsartErrorCode;
	}

	char ReceivedBytes[500];
	int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
	assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

	if (NumberOfReceived <= 0) {
		UsartErrorCode = usart_low_level_error::ReadFailed;
		handleCriticalError(__LINE__);
		return UsartErrorCode;
	}

	// Check if adding the new received bytes would exceed the limit given by the higher layer for the particular command
	if (TotalReceivedBytes + NumberOfReceived >= ReceivedBytesLimit) {
		*IsOverLimitsPtr = true;
		return UsartErrorCode;
	}

	// a rough check of transmission continuity
	auto ResponseDuration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - TimeWhenResponseStarted);
	int64_t ResponseDurationMs = ResponseDuration.count();
	if (ResponseDurationMs > LIMIT_ADDITION_COMPONENT) {
		ResponseDurationMs -= LIMIT_ADDITION_COMPONENT;
		int64_t ResponseDurationUs = ResponseDurationMs * 1000; // change the unit from milliseconds to microseconds
		if (ResponseDurationUs > LIMIT_MULTIPLIER_PER_BYTE * (TotalReceivedBytes + NumberOfReceived)) {
			*IsOverLimitsPtr = true;
			return UsartErrorCode;
		}
	}

	// Copy the received bytes into the internal buffer
	for (int J = 0; J < NumberOfReceived; J++) {
		UsartBuffer[TotalReceivedBytes] = ReceivedBytes[J];
		TotalReceivedBytes++;
	}
	UsartBuffer[TotalReceivedBytes] = 0;

	return UsartErrorCode;
}

usart_low_level_error low_level_usart::handleNoiseState(bool *IsSilencePtr) {
	assert(usart_low_level_error::None == UsartErrorCode);
	assert(usart_low_level_state::Open == UsartState);
	assert(-1 != SerialPortHandler);

	*IsSilencePtr = false;

	checkSilence(IsSilencePtr);
	if (usart_low_level_error::None != UsartErrorCode) {
		return UsartErrorCode;
	}
	if (*IsSilencePtr) {
		return UsartErrorCode;
	}

	return UsartErrorCode;
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
