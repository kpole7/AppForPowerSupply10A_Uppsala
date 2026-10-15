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
static void printNewState(UsartLowLevelState State);
static void printNewTransferState(UsartTransferState State);
#endif

//.................................................................................................
// Public method definitions
//.................................................................................................

LowLevelUsart::LowLevelUsart() {
	State = UsartLowLevelState::Uninitialized;
	TransferState = UsartTransferState::Idle;
	ErrorCode = UsartLowLevelError::None;
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

LowLevelUsart::~LowLevelUsart() {
	if (SerialPortHandler != -1) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
	}
	if (NameOfPort != nullptr) {
		delete NameOfPort;
		NameOfPort = nullptr;
	}
}

void LowLevelUsart::openUsart(const std::string &Name) {
	if (UsartLowLevelState::Uninitialized != State) {
		ErrorCode = UsartLowLevelError::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (NameOfPort != nullptr) {
		delete NameOfPort;
		NameOfPort = nullptr;
		ErrorCode = UsartLowLevelError::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		ErrorCode = UsartLowLevelError::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (UsartLowLevelError::None != ErrorCode) {
		State = UsartLowLevelState::Error;
#if DEBUG_USART_MASTER
		printNewState(State);
#endif
		return;
	}

	NameOfPort = new std::string(Name);

	static const char *PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK);
	if (0 != Result) {
		ErrorCode = UsartLowLevelError::UsartDeviceNotFound;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		State = UsartLowLevelState::Error;
		return;
	}

	SerialPortHandler = configureSerialPort(PortNameCharPtr);
	if (-1 == SerialPortHandler) {
		ErrorCode = UsartLowLevelError::OpenFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		State = UsartLowLevelState::Error;
		return;
	}
	State = UsartLowLevelState::Open;
}

std::string LowLevelUsart::getName() const {
	if (NameOfPort != nullptr) {
		return *NameOfPort;
	}
	else {
		return "";
	}
}

UsartLowLevelState LowLevelUsart::getState() const { return State; }

UsartTransferState LowLevelUsart::getTransferState() const { return TransferState; }

UsartLowLevelError LowLevelUsart::getErrorCode() const { return ErrorCode; }

void LowLevelUsart::resetUsartMaster() {
	State = UsartLowLevelState::Uninitialized;
	TransferState = UsartTransferState::Idle;
	ErrorCode = UsartLowLevelError::None;

	memset(TotalResponse, 0, MAX_RESPONSE_LENGTH);
	TotalReceivedBytes = 0;
	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TimeWhenResponseStarted = std::chrono::high_resolution_clock::now();
	TimeWhenReceivedLastByte = std::chrono::high_resolution_clock::now();
	WaitingLimit = (std::chrono::milliseconds)0;
	ResponseDurationLimit = (std::chrono::milliseconds)0;
}

void LowLevelUsart::sendOutputData(const char *Buffer, int Length, std::chrono::milliseconds WaitingTimeout,
                                   std::chrono::milliseconds ResponseTimeout) {
	if (UsartLowLevelState::Open != State) {
		ErrorCode = UsartLowLevelError::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		return;
	}

	WaitingLimit = WaitingTimeout;
	ResponseDurationLimit = ResponseTimeout;

	TransferState = UsartTransferState::Sending;

	int BytesWritten = write(SerialPortHandler, Buffer, Length);
	if (BytesWritten != Length) {
		ErrorCode = UsartLowLevelError::WriteFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		TransferState = UsartTransferState::InternalError;
		return;
	}

	TimeWhenOutgoingDataEnded = std::chrono::high_resolution_clock::now();
	TransferState = UsartTransferState::WaitingForResponse;
}

/// @brief Collect input data from the USART until a period of silence is detected.
/// @return True if data was successfully collected, false otherwise.
UsartTransferState LowLevelUsart::collectInputDataUntilSilence() {
	std::chrono::milliseconds SilenceDuration(0);
	std::chrono::high_resolution_clock::time_point TimeNow;

	if (UsartLowLevelState::Open != State) {
		ErrorCode = UsartLowLevelError::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if ((UsartTransferState::WaitingForResponse != TransferState) && (UsartTransferState::Receiving != TransferState) &&
	    (UsartTransferState::IncorrectlyReceiving != TransferState)) {
		ErrorCode = UsartLowLevelError::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if ((UsartTransferState::WaitingForResponse != TransferState) && (0 == TotalReceivedBytes)) {
		ErrorCode = UsartLowLevelError::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (-1 != SerialPortHandler) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		ErrorCode = UsartLowLevelError::InternalError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
	}
	if (UsartLowLevelError::None != ErrorCode) {
		State = UsartLowLevelState::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		TransferState = UsartTransferState::InternalError;
		return TransferState;
	}

	std::chrono::high_resolution_clock::time_point TimeNow = std::chrono::high_resolution_clock::now();

	// check if there is data available in the input buffer of the serial port
	fd_set SerialReadFds;
	FD_ZERO(&SerialReadFds);
	FD_SET(SerialPortHandler, &SerialReadFds);
	struct timeval SerialTimeout = {0, 0};
	int SerialPortState = select(SerialPortHandler + 1, &SerialReadFds, nullptr, nullptr, &SerialTimeout);

	if (SerialPortState == -1) {
		// select() failed, indicating an error with the serial port monitoring
		ErrorCode = UsartLowLevelError::SelectFailed;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		close(SerialPortHandler);
		SerialPortHandler = -1;
		State = UsartLowLevelState::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		TransferState = UsartTransferState::InternalError;
	}
	else if ((SerialPortState > 0) && FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		// There is something in the input buffer of the serial port

		char ReceivedBytes[500];
		int NumberOfReceived = read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
		assert(NumberOfReceived <= (int)sizeof(ReceivedBytes));

		if (NumberOfReceived > 0) {
			if (0 == TotalReceivedBytes) {
				assert(UsartTransferState::WaitingForResponse == TransferState);
				TimeWhenResponseStarted = TimeNow;
				TransferState = UsartTransferState::Receiving;
			}
			else {
				auto ReceivingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenResponseStarted);
				if (ReceivingDuration.count() > ResponseDurationLimit.count()) {
					if (ReceivingDuration.count() > RESPONSE_DURATION_ABSOLUTE_LIMIT_MS) {
						TransferState = UsartTransferState::IncorrectlyNoisy;
						ErrorCode = UsartLowLevelError::SlaveOutOfOrder;
						return TransferState;
					}
					TransferState = UsartTransferState::IncorrectlyReceiving;
					return TransferState;
				}
			}

			for (int J = 0; J < NumberOfReceived; J++) {
				if (MAX_RESPONSE_LENGTH - 1 > TotalReceivedBytes) {
					TotalResponse[TotalReceivedBytes] = ReceivedBytes[J];
					TotalReceivedBytes++;
				}
				else {
					ErrorCode = UsartLowLevelError::InternalError; // the response time limit is absurdly long
					printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
					close(SerialPortHandler);
					SerialPortHandler = -1;
					State = UsartLowLevelState::Error;
					delete NameOfPort;
					NameOfPort = nullptr;
					TransferState = UsartTransferState::InternalError;
					return TransferState;
				}
			}
			TotalResponse[TotalReceivedBytes] = 0;
			TimeWhenReceivedLastByte = TimeNow;
		}
		else {
			// read() failed, indicating an error with the serial port reading
			ErrorCode = UsartLowLevelError::ReadFailed;
			printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
			close(SerialPortHandler);
			SerialPortHandler = -1;
			State = UsartLowLevelState::Error;
			delete NameOfPort;
			NameOfPort = nullptr;
			TransferState = UsartTransferState::InternalError;
		}
	}
	else {
		// There is a silence

		if (0 == TotalReceivedBytes) {
			assert(UsartTransferState::WaitingForResponse == TransferState);

			// waiting for the first byte of the response
			auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenOutgoingDataEnded);
			if (SilenceDuration.count() > WaitingLimit.count()) {
				TransferState = UsartTransferState::WaitingLimitExceeded;
			}
		}
		else {
			// waiting for the silence after receiving the response
			auto SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenReceivedLastByte);
			if (SilenceDuration.count() > SILENCE_DETECTION_PERIOD_MS) {
				if (UsartTransferState::Receiving == TransferState) {
					TransferState = UsartTransferState::Completed;
					return TransferState;
				}
				else if (UsartTransferState::IncorrectlyReceiving == TransferState) {
					TransferState = UsartTransferState::IncorrectlyCompleted;
					return TransferState;
				}
				else {
					// unexpected transfer state, treat as internal error
				}
				TransferState = UsartTransferState::InternalError;
			}
		}
	}
	return TransferState;
}

int LowLevelUsart::getInputData(char *Buffer, int BufferSize) {
	if (UsartTransferState::Completed != TransferState) {
		ErrorCode = UsartLowLevelError::CallError;
		printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		close(SerialPortHandler);
		SerialPortHandler = -1;
		State = UsartLowLevelState::Error;
		delete NameOfPort;
		NameOfPort = nullptr;
		TransferState = UsartTransferState::CallError;
		return -1;
	}
	return 0;
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

char *UsartLowLevelStateTexts[] = {"Uninitialized", "Open", "Closed", "Error"};

char *UsartTransferStateTexts[] = {"Idle",
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

static void printNewState(UsartLowLevelState State) {
	static UsartLowLevelState PreviousState = UsartLowLevelState::Uninitialized;
	printf("usart_master state: %s -> %s\n", UsartLowLevelStateTexts[static_cast<int>(PreviousState)],
	       UsartLowLevelStateTexts[static_cast<int>(State)]);
	PreviousState = State;
}

static void printNewTransferState(UsartTransferState State) {
	static UsartTransferState PreviousState = UsartTransferState::Idle;
	printf("usart_master transfer state: %s -> %s\n", UsartTransferStateTexts[static_cast<int>(PreviousState)],
	       UsartTransferStateTexts[static_cast<int>(State)]);
	PreviousState = State;
}
#endif

//........................................................................................................
