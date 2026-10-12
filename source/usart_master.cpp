/// @file usart_master.cpp
/// @brief Implementation of communication with the slave device based on RP2040 microcontroller.
///
/// @details This file contains low-level layer for usart communication.


#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <iostream>
#include <cassert>
#include <cstring>
#include "usart_master.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

/// This constant determines USART speed; it is defined in termios.h
#define USART_HARDWARE_SPEED B4800

/// This constant determines the period of silence (in milliseconds) to detect the end of input data.
/// It is equal about 3.5 character times at the configured USART speed.
#define SILENCE_DETECTION_PERIOD_MS 7

//.................................................................................................
// Local function prototypes
//.................................................................................................

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName);



//.................................................................................................
// Public method definitions
//.................................................................................................

LowLevelUsart::LowLevelUsart() {
    NameOfPort = nullptr;
    State = UsartState::Uninitialized;
    SerialPortHandler = -1;
    ErrorCode = UsartError::None;
    TotalReceivedBytes = 0;
    memset(TotalResponse, 0, MAX_RESPONSE_LENGTH);
    TimeWhenSilenceStarted = std::chrono::high_resolution_clock::now();
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

void LowLevelUsart::openUsart(const std::string& Name) {
    if (UsartState::Uninitialized != State) {
        ErrorCode = UsartError::InternalError;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
    }
    if (NameOfPort != nullptr) {
        delete NameOfPort;
        NameOfPort = nullptr;
        ErrorCode = UsartError::InternalError;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
    }
    if (-1 != SerialPortHandler) {
        close(SerialPortHandler);
        SerialPortHandler = -1;
        ErrorCode = UsartError::InternalError;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
    }
    if (UsartError::None != ErrorCode) {
        State = UsartState::Error;
        return;
    }

    NameOfPort = new std::string(Name);

	static const char* PortNameCharPtr = NameOfPort->c_str();
	int Result = access(PortNameCharPtr, F_OK );
	if(0 != Result){
        ErrorCode = UsartError::UsartDeviceNotFound;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
        State = UsartState::Error;
        return;
    }

    SerialPortHandler = configureSerialPort( PortNameCharPtr );
	if(-1 == SerialPortHandler){
		ErrorCode = UsartError::OpenFailed;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
        State = UsartState::Error;
        return;
	}
    State = UsartState::Open;
}

std::string LowLevelUsart::getName() const {
    if (NameOfPort != nullptr) {
        return *NameOfPort;
    } else {
        return "";
    }
}

UsartState LowLevelUsart::getState() const {
    return State;
}

UsartError LowLevelUsart::getErrorCode() const {
    return ErrorCode;
}

/// @brief Collect input data from the USART until a period of silence is detected.
/// @return True if data was successfully collected, false otherwise.
bool LowLevelUsart::collectInputDataUntilSilence() {
    std::chrono::milliseconds SilenceDuration(0);
    std::chrono::high_resolution_clock::time_point TimeNow;

    if (UsartState::Open != State) {
        ErrorCode = UsartError::InternalError;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
    }
    if (-1 != SerialPortHandler) {
        close(SerialPortHandler);
        SerialPortHandler = -1;
        ErrorCode = UsartError::InternalError;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
    }
    if (UsartError::None != ErrorCode) {
        State = UsartState::Error;
        delete NameOfPort;
        NameOfPort = nullptr;
        return false;
    }

    // input buffer handling
    fd_set SerialReadFds;
	FD_ZERO(&SerialReadFds);
	FD_SET(SerialPortHandler, &SerialReadFds);
	struct timeval SerialTimeout = { 0, 0 };
	int SerialPortState = select(SerialPortHandler + 1, &SerialReadFds, nullptr, nullptr, &SerialTimeout);

	if (SerialPortState == -1) {
        ErrorCode = UsartError::SelectFailed;
        printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
		close( SerialPortHandler );
		SerialPortHandler = -1;
		State = UsartState::Error;
        delete NameOfPort;
        NameOfPort = nullptr;
		return false;
	}
	else if ((SerialPortState > 0) && FD_ISSET(SerialPortHandler, &SerialReadFds)) {
		// There is something in the input buffer of the serial port

		char ReceivedBytes[1000];
		int NumberOfReceived = read(SerialPortHandler, &ReceivedBytes[0], sizeof(ReceivedBytes));
		assert( NumberOfReceived <= (int)sizeof(ReceivedBytes));

		if (NumberOfReceived > 0) {
            if (0 == TotalReceivedBytes){
                TimeWhenSilenceStarted = std::chrono::high_resolution_clock::now();
            }

			for (int J=0; J < NumberOfReceived; J++){
				if (MAX_RESPONSE_LENGTH-1 > TotalReceivedBytes){
                    TotalResponse[TotalReceivedBytes] = ReceivedBytes[J];
    				TotalReceivedBytes++;
                }
                else {
					break; // answers that are too long are shortened
				}

			}
			TotalResponse[TotalReceivedBytes] = 0;
		}
		else{
            ErrorCode = UsartError::ReadFailed;
            printf("USART Error %d in %s at line %d.\n", static_cast<int>(ErrorCode), __FILE__, __LINE__);
            close( SerialPortHandler );
            SerialPortHandler = -1;
            State = UsartState::Error;
            delete NameOfPort;
            NameOfPort = nullptr;
            return false;
		}
	}
	else{
		// There is a silence
		std::chrono::high_resolution_clock::time_point TimeNow = std::chrono::high_resolution_clock::now();
		SilenceDuration = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeWhenSilenceStarted);
        if (SilenceDuration.count() > SILENCE_DETECTION_PERIOD_MS) {
            return true;
        }
	}
    return false;
}

//........................................................................................................
// Local function definitions
//........................................................................................................

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName){
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
    PortSettings.c_oflag = 0;                            // Disable output processing
    PortSettings.c_lflag = 0;                            // Mode 'raw'

    PortSettings.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CRTSCTS);
    PortSettings.c_cflag |= (CS8 | CREAD | CLOCAL);

    PortSettings.c_cc[VMIN]  = 0;                        // Minimum number of bytes to read
    PortSettings.c_cc[VTIME] = 0;                        // Timeout in tenth of a second

    // apply the configuration
    if (tcsetattr(FileHandler, TCSANOW, &PortSettings) != 0) {
        close(FileHandler);
        return -1;
    }

    return FileHandler;
}

//........................................................................................................
