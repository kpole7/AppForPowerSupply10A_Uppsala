/// @file usart_master.cpp
/// @brief Implementation of communication with the slave device based on RP2040 microcontroller.
///
/// @details This file contains low-level layer for communication between the master and the slave device using USART.
/// This application runs on the master device (PC) and communicates with the slave device (RP2040 microcontroller).
/// The master device sends commands to the slave device and receives responses.
/// At this low-level layer, the communication is organized in such a way that the master sends a continuous packet of bytes
/// and waits for the response from the slave device. The response is also a continuous packet of bytes.
/// Then the master detects a period of silence to determine the end of the response from the slave device.
/// At this layer, time of waiting for the response is checked but not enforced. Similarly, the response time is measured
/// but not enforced. These responsibilities are handled at a higher layer of the application.

#include "usart_master.h"
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <termios.h>
#include <unistd.h>

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

static void printNewState(usart_low_level_state State);
static void printNewTransferState(usart_transfer_state State);

static std::string formatElapsedTime(std::chrono::milliseconds::rep TimeSinceReference) {
	auto Minutes = (TimeSinceReference / 60000) % 100;
	auto Seconds = (TimeSinceReference / 1000) % 60;
	auto Milliseconds = TimeSinceReference % 1000;

	std::ostringstream FormattedTime;
	FormattedTime << std::setfill('0') << std::setw(2) << Minutes << ":" << std::setw(2) << Seconds << ":" << std::setw(3) << Milliseconds;
	return FormattedTime.str();
}

//=================================================================================================
// Public method definitions
//=================================================================================================

low_level_usart::low_level_usart() {
	State = usart_low_level_state::Uninitialized;
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
	if (usart_low_level_state::Uninitialized != State) {
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
		State = usart_low_level_state::Error;
		printNewState(State);
		return CriticalErrorCode;
	}

	NameOfPort = new std::string(Name);

	static const char *PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK);
	if (0 != Result) {
		CriticalErrorCode = usart_low_level_error::UsartDeviceNotFound;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		State = usart_low_level_state::Error;
		return CriticalErrorCode;
	}

	SerialPortHandler = configureSerialPort(PortNameCharPtr);
	if (-1 == SerialPortHandler) {
		CriticalErrorCode = usart_low_level_error::OpenFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		State = usart_low_level_state::Error;
		return CriticalErrorCode;
	}
	State = usart_low_level_state::Open;
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
usart_low_level_state low_level_usart::getState() const { return State; }

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
	if (State != usart_low_level_state::Open) {
		return false;
	}
#if 0
	if ((TransferState != usart_transfer_state::WaitingLimitExceeded) && (TransferState != usart_transfer_state::IncorrectlyCompleted)) {
		return false;
	}
	if (CriticalErrorCode != usart_low_level_error::None) {
		return false;
	}
#else
	// FSM debugging
	if ((CriticalErrorCode != usart_low_level_error::None) &&
	    (TransferState != usart_transfer_state::InternalError) &&
	    (TransferState != usart_transfer_state::CallError)) {
		return false;
	}
#endif
	TransferState = usart_transfer_state::Idle;
	printNewTransferState(TransferState);

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
	if (usart_low_level_state::Open != State) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		return CriticalErrorCode;
	}

	WaitingLimit = WaitingTimeout;
	ResponseDurationLimit = ResponseTimeout;

	TransferState = usart_transfer_state::Sending;
	printNewTransferState(TransferState);

	int BytesWritten = (int)write(SerialPortHandler, Buffer, Length);
	if (BytesWritten != Length) {
		CriticalErrorCode = usart_low_level_error::WriteFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
		TransferState = usart_transfer_state::InternalError;
		printNewTransferState(TransferState);
		return CriticalErrorCode;
	}

	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TransferState = usart_transfer_state::WaitingForResponse;
	printNewTransferState(TransferState);
	return CriticalErrorCode;
}

/// @brief Collect input data from the USART until a period of silence is detected.
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
		handleCriticalError(usart_transfer_state::CallError);
		return -1;
	}

	size_t Length = strlen(TotalResponse);
	if (Length >= (size_t)BufferSize) {

		// TODO: review
		return 0;
	}

	strncpy(Buffer, TotalResponse, BufferSize);
	TransferState = usart_transfer_state::Idle;
	printNewTransferState(TransferState);
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
	if (usart_low_level_state::Open != State) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
	}
	if ((usart_transfer_state::WaitingForResponse != TransferState) && (usart_transfer_state::Receiving != TransferState) &&
	    (usart_transfer_state::IncorrectlyReceiving != TransferState)) {
		CriticalErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, __LINE__);
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
		handleCriticalError(usart_transfer_state::InternalError);
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
	State = usart_low_level_state::Error;
	delete NameOfPort;
	NameOfPort = nullptr;
	printf("USART Error %d in %s at line %d.\n", static_cast<int>(CriticalErrorCode), __FILE__, Line);
	TransferState = NewTransferState;
	printNewTransferState(TransferState);
	return TransferState;
}

/// @brief Collect input data from the USART until a period of silence is detected assuming normal conditions.
/// @details It is assumed that the USART is ready to receive a response and that the module state variables
/// are consistent with normal operation.
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
		return handleCriticalError(usart_transfer_state::CallError);
	}
	if ((SerialPortState <= 0) || !FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		return handleSilence(TimeNow);
	}

	char ReceivedBytes[500];
	int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
	assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

	if (NumberOfReceived <= 0) {
		CriticalErrorCode = usart_low_level_error::ReadFailed;
		return handleCriticalError(usart_transfer_state::CallError);
	}

	return processReceivedInput(ReceivedBytes, NumberOfReceived, TimeNow);
}

// TODO: more advanced response time checking
/// @brief Process the received input data from the USART.
/// @details This function handles the received bytes, updates the transfer state, and checks for response duration limits.
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
		printNewTransferState(TransferState);
	}
	else {
		auto ReceivingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenResponseStarted);
		if (ReceivingDuration.count() > ResponseDurationLimit.count()) {
			TransferState = usart_transfer_state::IncorrectlyReceiving;
			printNewTransferState(TransferState);
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
		return handleCriticalError(usart_transfer_state::InternalError);
	}

	TotalResponse[TotalReceivedBytes] = 0;
	TimeWhenReceivedLastByte = TimeNow;
	return TransferState;
}

/// @brief Handle silence on the USART line.
/// @details This function checks for silence duration and updates the transfer state accordingly.
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
			printNewTransferState(TransferState);
		}
		return TransferState;
	}

	auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenReceivedLastByte);
	if (SilenceDuration.count() <= SILENCE_DETECTION_PERIOD_MS) {
		return TransferState;
	}
	if (usart_transfer_state::Receiving == TransferState) {
		TransferState = usart_transfer_state::Completed;
		printNewTransferState(TransferState);
		return TransferState;
	}
	if (usart_transfer_state::IncorrectlyReceiving == TransferState) {
		TransferState = usart_transfer_state::IncorrectlyCompleted;
		printNewTransferState(TransferState);
		return TransferState;
	}

	TransferState = usart_transfer_state::InternalError;
	printNewTransferState(TransferState);
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

//=================================================================================================
// Code used for debugging and integration testing
//=================================================================================================

#if DEBUG_USART_MASTER
char UsartLowLevelStateTexts[4][15] = {"Uninitialized", "Open", "Closed", "Error"};

char UsartTransferStateTexts[11][21] = {"Idle",
                                        "Sending",
                                        "WaitingForResponse",
                                        "Receiving",
                                        "Completed",
                                        "WaitingLimitExceeded",
                                        "IncorrectlyReceiving",
                                        "IncorrectlyCompleted",
                                        "IncorrectlyNoisy",
                                        "InternalError",
                                        "CallError"};
#endif

static void printNewState(usart_low_level_state State) {
#if DEBUG_USART_MASTER
	static usart_low_level_state PreviousState = usart_low_level_state::Uninitialized;
	printf("usart_master state: %s -> %s\n", UsartLowLevelStateTexts[static_cast<int>(PreviousState)],
	       UsartLowLevelStateTexts[static_cast<int>(State)]);
	PreviousState = State;
#else
	(void)State;
#endif
}

static void printNewTransferState(usart_transfer_state State) {
#if 0 // DEBUG_USART_MASTER
	static usart_transfer_state PreviousState = usart_transfer_state::Idle;
	printf("usart_master transfer state: %s -> %s\n", UsartTransferStateTexts[static_cast<int>(PreviousState)],
	       UsartTransferStateTexts[static_cast<int>(State)]);
	PreviousState = State;
#else
	(void)State;
#endif
}

#if DEBUG_USART_MASTER

enum class single_command_fsm_state : uint8_t {
	Uninitialized,
	Closed,
	Idle,
	WaitingForResponse,
	Receiving,

	InitializationError,
	OpenError,
	SendingError,
	ReceivingError,
};

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they can be terminated
/// @callgraph
/// @callergraph
bool usartMasterDebuggingTick(bool TerminationSignal, int PressedKey) {
	void printStateChanges(low_level_usart * UsartPtr, std::chrono::high_resolution_clock::time_point ReferenceTime, const char *UserEventName);
	static const char InquiryPOWER[] = "?POWER\r\n"; // a
	static const char InquiryZ[] = "?Z\r\n";         // b
	static const char InquiryPC[] = "?PC\r\n";       // c
	static const char InquiryST[] = "ST\r\n";        // d
	static const char SetPOWER1[] = "POWER1\r\n";    // e
	static const char SetPOWER0[] = "POWER0\r\n";    // f
	static const char SetZ1[] = "Z1\r\n";            // g
	static const char SetZ2[] = "Z2\r\n";            // h
	static const char SetPC0[] = "PC 0\r\n";         // i
	static const char SetPC1[] = "PC 1\r\n";         // j
	static const char SetPC2[] = "PC 2\r\n";         // k
	static const char SetPC01[] = "PC 0.1\r\n";      // l
	static const char SetPC07[] = "PC 0.7\r\n";      // m
	static const char SetPC09[] = "PC 0.9\r\n";      // n
	static const char SetPC11[] = "PC 1.1\r\n";      // o
	static const char SetPC17[] = "PC 1.7\r\n";      // p
	static const char SetPC19[] = "PC 1.9\r\n";      // q
	static const char SetPC_1[] = "PC -1\r\n";       // r
	static const char SetPC_2[] = "PC -2\r\n";       // s
	static const char SetPC_01[] = "PC -0.1\r\n";    // t
	static const char SetPC_07[] = "PC -0.7\r\n";    // u
	static const char SetPC_09[] = "PC -0.9\r\n";    // v
	static const char SetPC_11[] = "PC -1.1\r\n";    // w
	static const char SetPC_17[] = "PC -1.7\r\n";    // x
	static const char SetPC_19[] = "PC -1.9\r\n";    // y
	const char *CommandTexts[] = {&InquiryPOWER[0], &InquiryZ[0], &InquiryPC[0], &InquiryST[0], &SetPOWER1[0], &SetPOWER0[0], &SetZ1[0],
	                              &SetZ2[0],        &SetPC0[0],   &SetPC1[0],    &SetPC2[0],    &SetPC01[0],   &SetPC07[0],   &SetPC09[0],
	                              &SetPC11[0],      &SetPC17[0],  &SetPC19[0],   &SetPC_1[0],   &SetPC_2[0],   &SetPC_01[0],  &SetPC_07[0],
	                              &SetPC_09[0],     &SetPC_11[0], &SetPC_17[0],  &SetPC_19[0]};
	static low_level_usart *MyUsartPtr = nullptr;

	static single_command_fsm_state FsmState = single_command_fsm_state::Uninitialized;

	char *CommandText = nullptr;

	// Handle termination signal and cleanup if necessary
	if (TerminationSignal) {
		if (nullptr != MyUsartPtr) {
			delete MyUsartPtr;
			MyUsartPtr = nullptr;
		}
		return 0;
	}

	static auto TimeReference = std::chrono::high_resolution_clock::now();

	if ('T' == PressedKey) {
		TimeReference = std::chrono::high_resolution_clock::now();
	}

	usart_low_level_error Error = usart_low_level_error::None;

	// FSM state handling
	switch (FsmState) {
	case single_command_fsm_state::Uninitialized:
		// Initial state
		if (nullptr == MyUsartPtr) {
			MyUsartPtr = new low_level_usart();
		}
		if (nullptr == MyUsartPtr) {
			std::cerr << "Failed to initialize USART\n";
			FsmState = single_command_fsm_state::InitializationError; // Failed to initialize USART
		}
		else {
			FsmState = single_command_fsm_state::Closed; // Transition to close state
		}
		break;
	case single_command_fsm_state::Closed:
		// Closed state
		assert(nullptr != MyUsartPtr);
		Error = MyUsartPtr->openUsart("/dev/ttyUSB0");
		if (Error != usart_low_level_error::None) {
			std::cerr << "Failed to open USART\n";
			FsmState = single_command_fsm_state::OpenError; // Failed to open USART
		}
		else {
			FsmState = single_command_fsm_state::Idle; // Transition to idle state
		}
		break;
	case single_command_fsm_state::Idle:
		// Idle state = waiting for user input

		if ((PressedKey >= 'a') && (PressedKey <= 'y')) {
			CommandText = const_cast<char *>(CommandTexts[PressedKey - 'a']);
			Error = MyUsartPtr->sendOutputData(CommandText, strlen(CommandText), static_cast<std::chrono::milliseconds>(200),
			                                   static_cast<std::chrono::milliseconds>(100));
			if (Error != usart_low_level_error::None) {
				std::cerr << "Failed to send data\n";
				FsmState = single_command_fsm_state::SendingError; // Failed to send data
			}
			else {
				FsmState = single_command_fsm_state::WaitingForResponse; // Transition to receiving state
			}
		}

		break;
	case single_command_fsm_state::WaitingForResponse:
		// Receiving state

		MyUsartPtr->collectInputDataUntilSilence();

		if (Error != usart_low_level_error::None) {
			std::cerr << "Failed to collect input data\n";
			FsmState = single_command_fsm_state::ReceivingError; // Failed to collect input data
		}
		break;

	default:
		// Handle unexpected states

		if ('R' == PressedKey) {
			bool ResetSuccess = MyUsartPtr->resetUsartMaster();
			if (!ResetSuccess) {
				std::cerr << "Failed to reset USART master\n";
			}
			else {
				FsmState = single_command_fsm_state::Idle;
				std::cout << "USART master reset successfully\n";
			}
		}

		break;
	}

	printStateChanges(MyUsartPtr, TimeReference, nullptr);

	return (nullptr != MyUsartPtr);
}

/// @callgraph
/// @callergraph
void printStateChanges(low_level_usart *UsartPtr, std::chrono::high_resolution_clock::time_point ReferenceTime, const char *UserEventName) {
	static usart_low_level_state LastState = usart_low_level_state::Uninitialized;
	static usart_transfer_state LastTransferState = usart_transfer_state::Idle;
	static usart_low_level_error LastErrorCode = usart_low_level_error::None;
	bool StateChanged = (LastState != UsartPtr->getState());
	bool TransferStateChanged = (LastTransferState != UsartPtr->getTransferState());
	bool ErrorChanged = (LastErrorCode != UsartPtr->getErrorCode());
	bool UserEventOccurred = (nullptr != UserEventName);
	bool PrintingStarted = false;

	if (StateChanged || TransferStateChanged || ErrorChanged || UserEventOccurred) {
		auto TimeNow = std::chrono::high_resolution_clock::now();
		auto TimeSinceReference = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - ReferenceTime).count();
		std::cout << "[" << formatElapsedTime(TimeSinceReference) << "] ";
	}

	if (StateChanged) {
		std::cout << "State " << UsartLowLevelStateTexts[static_cast<int>(LastState)] << " -> "
		          << UsartLowLevelStateTexts[static_cast<int>(UsartPtr->getState())];
		PrintingStarted = true;
	}
	if (TransferStateChanged) {
		if (PrintingStarted) {
			std::cout << "   ";
		}
		std::cout << "Transfer state " << UsartTransferStateTexts[static_cast<int>(LastTransferState)] << " -> "
		          << UsartTransferStateTexts[static_cast<int>(UsartPtr->getTransferState())];
		PrintingStarted = true;
	}
	if (ErrorChanged) {
		if (PrintingStarted) {
			std::cout << "   ";
		}
		std::cout << "Error " << static_cast<int>(LastErrorCode) << " -> " << static_cast<int>(UsartPtr->getErrorCode());
		PrintingStarted = true;
	}
	if (UserEventOccurred) {
		if (PrintingStarted) {
			std::cout << "   ";
		}
		std::cout << UserEventName;
		PrintingStarted = true;
	}
	if (PrintingStarted) {
		std::cout << '\n';
	}

	LastState = UsartPtr->getState();
	LastTransferState = UsartPtr->getTransferState();
	LastErrorCode = UsartPtr->getErrorCode();
}

#else

bool usartMasterDebuggingTick(bool TerminationSignal, int PressedKey) {
	(void)PressedKey;
	return !TerminationSignal;
}
#endif

//=================================================================================================.......
