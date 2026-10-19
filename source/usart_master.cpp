/// @file usart_master.cpp
/// @brief Implementation of communication with the slave device based on RP2040 microcontroller.
///
/// @details This file contains low-level layer for communication between the master and slave devices using USART.
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
#include <iostream>
#include <termios.h>
#include <unistd.h>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define DEBUG_USART_MASTER 1

/// This constant determines USART speed; it is defined in termios.h
#define USART_HARDWARE_SPEED B4800

/// This constant determines the period of silence (in milliseconds) to detect the end of input data.
/// It is equal about 3.5 character times at the configured USART speed.
#define SILENCE_DETECTION_PERIOD_MS 7

/// This constant defines the absolute limit for the response duration (in milliseconds).
/// If this limit is exceeded, the application will be closed.
#define RESPONSE_DURATION_ABSOLUTE_LIMIT_MS 500

//.................................................................................................
// Local function prototypes
//.................................................................................................

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName);

#if DEBUG_USART_MASTER
static void printNewState(usart_low_level_state State);
static void printNewTransferState(usart_transfer_state State);
#endif

//.................................................................................................
// Public method definitions
//.................................................................................................

low_level_usart::low_level_usart() {
	State = usart_low_level_state::Uninitialized;
	TransferState = usart_transfer_state::Idle;
	ErrorCode = usart_low_level_error::None;
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

void low_level_usart::openUsart(const std::string &Name) {
	if (usart_low_level_state::Uninitialized != State) {
		ErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (NameOfPort != nullptr) {
		delete NameOfPort;
		NameOfPort = nullptr;
		ErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		ErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (usart_low_level_error::None != ErrorCode) {
		State = usart_low_level_state::Error;
#if DEBUG_USART_MASTER
		printNewState(State);
#endif
		return;
	}

	NameOfPort = new std::string(Name);

	static const char *PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK);
	if (0 != Result) {
		ErrorCode = usart_low_level_error::UsartDeviceNotFound;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		State = usart_low_level_state::Error;
		return;
	}

	SerialPortHandler = configureSerialPort(PortNameCharPtr);
	if (-1 == SerialPortHandler) {
		ErrorCode = usart_low_level_error::OpenFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		State = usart_low_level_state::Error;
		return;
	}
	State = usart_low_level_state::Open;
}

std::string low_level_usart::getName() const {
	if (NameOfPort != nullptr) {
		return *NameOfPort;
	}
	return "";
}

usart_low_level_state low_level_usart::getState() const { return State; }

usart_transfer_state low_level_usart::getTransferState() const { return TransferState; }

usart_low_level_error low_level_usart::getErrorCode() const { return ErrorCode; }

void low_level_usart::resetUsartMaster() {
	State = usart_low_level_state::Uninitialized;
	TransferState = usart_transfer_state::Idle;
	printNewTransferState(TransferState);
	ErrorCode = usart_low_level_error::None;

	memset(TotalResponse, 0, MAX_RESPONSE_LENGTH);
	TotalReceivedBytes = 0;
	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	TimeWhenReceivedLastByte = std::chrono::high_resolution_clock::now();
	WaitingLimit = (std::chrono::milliseconds)0;
	ResponseDurationLimit = (std::chrono::milliseconds)0;
}

void low_level_usart::sendOutputData(const char *Buffer, int Length, std::chrono::milliseconds WaitingTimeout,
                                     std::chrono::milliseconds ResponseTimeout) {
	if (usart_low_level_state::Open != State) {
		ErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		return;
	}

	WaitingLimit = WaitingTimeout;
	ResponseDurationLimit = ResponseTimeout;

	TransferState = usart_transfer_state::Sending;
	printNewTransferState(TransferState);

	int BytesWritten = (int)write(SerialPortHandler, Buffer, Length);
	if (BytesWritten != Length) {
		ErrorCode = usart_low_level_error::WriteFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		TransferState = usart_transfer_state::InternalError;
		printNewTransferState(TransferState);
		return;
	}

	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TransferState = usart_transfer_state::WaitingForResponse;
	printNewTransferState(TransferState);
}

/// @brief Collect input data from the USART until a period of silence is detected.
/// @return True if data was successfully collected, false otherwise.
usart_transfer_state low_level_usart::collectInputDataUntilSilence() {
	std::chrono::high_resolution_clock::time_point TimeNow;

	if (usart_low_level_state::Open != State) {
		ErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if ((usart_transfer_state::WaitingForResponse != TransferState) && (usart_transfer_state::Receiving != TransferState) &&
	    (usart_transfer_state::IncorrectlyReceiving != TransferState)) {
		ErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if ((usart_transfer_state::WaitingForResponse != TransferState) && (0 == TotalReceivedBytes)) {
		ErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		ErrorCode = usart_low_level_error::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (usart_low_level_error::None != ErrorCode) {
		State = usart_low_level_state::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		TransferState = usart_transfer_state::InternalError;
		printNewTransferState(TransferState);
		return TransferState;
	}

	TimeNow = std::chrono::high_resolution_clock::now();

	// check if there is data available in the input buffer of the serial port
	fd_set SerialReadFds;
	FD_ZERO(&SerialReadFds);
	FD_SET(SerialPortHandler, &SerialReadFds);
	struct timeval SerialTimeout = {0, 0};
	int SerialPortState = select(SerialPortHandler + 1, &SerialReadFds, nullptr, nullptr, &SerialTimeout);

	if (SerialPortState == -1) {
		// select() failed, indicating an error with the serial port monitoring
		ErrorCode = usart_low_level_error::SelectFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		close(SerialPortHandler);
		SerialPortHandler = -1;
		State = usart_low_level_state::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		TransferState = usart_transfer_state::InternalError;
		printNewTransferState(TransferState);
	}
	else if ((SerialPortState > 0) && FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		// There is something in the input buffer of the serial port

		char ReceivedBytes[500];
		int NumberOfReceived = (int)read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
		assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

		if (NumberOfReceived > 0) {
			if (0 == TotalReceivedBytes) {
				assert(usart_transfer_state::WaitingForResponse == TransferState);
				TimeWhenResponseStarted = TimeNow;
				TransferState = usart_transfer_state::Receiving;
				printNewTransferState(TransferState);
			}
			else {
				auto ReceivingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenResponseStarted);
				if (ReceivingDuration.count() > ResponseDurationLimit.count()) {
					if (ReceivingDuration.count() > RESPONSE_DURATION_ABSOLUTE_LIMIT_MS) {
						TransferState = usart_transfer_state::IncorrectlyNoisy;
						printNewTransferState(TransferState);
						ErrorCode = usart_low_level_error::SlaveOutOfOrder;
						return TransferState;
					}
					TransferState = usart_transfer_state::IncorrectlyReceiving;
					printNewTransferState(TransferState);
					return TransferState;
				}
			}

			for (int J = 0; J < NumberOfReceived; J++) {
				if (MAX_RESPONSE_LENGTH - 1 > TotalReceivedBytes) {
					TotalResponse[TotalReceivedBytes] = ReceivedBytes[J];
					TotalReceivedBytes++;
				}
				else {
					ErrorCode = usart_low_level_error::InternalError; // the response time limit is absurdly long
					printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
					close(SerialPortHandler);
					SerialPortHandler = -1;
					State = usart_low_level_state::Error;
					delete NameOfPort;
					NameOfPort = nullptr;
					TransferState = usart_transfer_state::InternalError;
					printNewTransferState(TransferState);
					return TransferState;
				}
			}
			TotalResponse[TotalReceivedBytes] = 0;
			TimeWhenReceivedLastByte = TimeNow;
		}
		else {
			// read() failed, indicating an error with the serial port reading
			ErrorCode = usart_low_level_error::ReadFailed;
			printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
			close(SerialPortHandler);
			SerialPortHandler = -1;
			State = usart_low_level_state::Error;
			delete NameOfPort;
			NameOfPort = nullptr;
			TransferState = usart_transfer_state::InternalError;
			printNewTransferState(TransferState);
		}
	}
	else {
		// There is a silence

		if (0 == TotalReceivedBytes) {
			assert(usart_transfer_state::WaitingForResponse == TransferState);

			// waiting for the first byte of the response
			auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenOutgoingDataEnded);
			if (SilenceDuration.count() > WaitingLimit.count()) {
				TransferState = usart_transfer_state::WaitingLimitExceeded;
				printNewTransferState(TransferState);
			}
		}
		else {
			// waiting for the silence after receiving the response
			auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenReceivedLastByte);
			if (SilenceDuration.count() > SILENCE_DETECTION_PERIOD_MS) {
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
				// unexpected transfer state, treat as internal error
				TransferState = usart_transfer_state::InternalError;
				printNewTransferState(TransferState);
			}
		}
	}
	return TransferState;
}

int low_level_usart::getInputData(char *Buffer, int BufferSize) {
	if ((usart_transfer_state::Completed != TransferState) || (BufferSize >= MAX_RESPONSE_LENGTH)) {
		ErrorCode = usart_low_level_error::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		close(SerialPortHandler);
		SerialPortHandler = -1;
		State = usart_low_level_state::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		TransferState = usart_transfer_state::CallError;
		printNewTransferState(TransferState);
		return -1;
	}

	size_t Length = strlen(TotalResponse);
	if (Length >= (size_t)BufferSize) {
		return 0;
	}

	strncpy(Buffer, TotalResponse, BufferSize);
	Buffer[BufferSize - 1] = '\0'; // Ensure null-termination
	return (int)Length;
}

//........................................................................................................
// Local function definitions
//........................................................................................................

// This function opens and configures a serial port
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

static void printNewState(usart_low_level_state State) {
	static usart_low_level_state PreviousState = usart_low_level_state::Uninitialized;
	printf("usart_master state: %s -> %s\n", UsartLowLevelStateTexts[static_cast<int>(PreviousState)],
	       UsartLowLevelStateTexts[static_cast<int>(State)]);
	PreviousState = State;
}

static void printNewTransferState(usart_transfer_state State) {
	static usart_transfer_state PreviousState = usart_transfer_state::Idle;
	printf("usart_master transfer state: %s -> %s\n", UsartTransferStateTexts[static_cast<int>(PreviousState)],
	       UsartTransferStateTexts[static_cast<int>(State)]);
	PreviousState = State;
}
#endif

//........................................................................................................
