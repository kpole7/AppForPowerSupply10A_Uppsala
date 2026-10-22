/// @file usart_master.h

#ifndef USART_MASTER_H
#define USART_MASTER_H

#include <cassert>
#include <chrono>
#include <inttypes.h>
#include <string>

static_assert(sizeof(int) == 4, "Expected int to be 4 bytes.");

#define MAX_RESPONSE_LENGTH 300

enum class usart_low_level_state { 
	Uninitialized, 
	Open, // normal operation
	Closed, 
	Error 
};

enum class usart_low_level_error {
	None, // normal operation
	UsartDeviceNotFound,
	OpenFailed,
	SelectFailed,
	ReadFailed,
	WriteFailed,
	InternalError,
	CallError
};

enum class usart_transfer_state {
	Idle, // normal operation; wait for a new outgoing data to be sent
	Sending, // normal operation
	WaitingForResponse, // normal operation
	Receiving, // normal operation
	Completed, // normal operation
	WaitingLimitExceeded, // non-critical error: the slave did not respond within the expected waiting limit and
						  // there is no ongoing transfer now
	IncorrectlyReceiving, // non-critical error: if the response is too long and the transfer is still ongoing
						  // or waiting time has exceeded but now the transfer is still ongoing
	IncorrectlyCompleted, // non-critical error: ingoing transfer has ended but the response was too long
	IncorrectlyNoisy, // non-critical error: if the slave is sending unexpected data and the transfer is still ongoing
	InternalError,
	CallError
};

class low_level_usart {
  private:
	usart_low_level_state State;
	usart_transfer_state TransferState;
	usart_low_level_error CriticalErrorCode;
	std::string *NameOfPort;
	int SerialPortHandler;
	char TotalResponse[MAX_RESPONSE_LENGTH];
	uint16_t TotalReceivedBytes;
	std::chrono::high_resolution_clock::time_point TimeWhenOutgoingDataEnded;
	std::chrono::high_resolution_clock::time_point TimeWhenResponseStarted;
	std::chrono::high_resolution_clock::time_point TimeWhenReceivedLastByte;
	std::chrono::milliseconds WaitingLimit;
	std::chrono::milliseconds ResponseDurationLimit;

	bool checkNormalConditions();
	usart_transfer_state handleCriticalError(usart_transfer_state NewTransferState, int Line = __LINE__);
	usart_transfer_state operateAssumingNormalConditions(std::chrono::high_resolution_clock::time_point TimeNow);
	usart_transfer_state processReceivedInput(const char *ReceivedBytes, int NumberOfReceived,
	                                          std::chrono::high_resolution_clock::time_point TimeNow);
	usart_transfer_state handleSilence(std::chrono::high_resolution_clock::time_point TimeNow);

  public:
	low_level_usart();
	~low_level_usart();
	usart_low_level_error openUsart(const std::string &Name);
	[[nodiscard]] std::string getName() const;
	[[nodiscard]] usart_low_level_state getState() const;
	[[nodiscard]] usart_transfer_state getTransferState() const;
	[[nodiscard]] usart_low_level_error getErrorCode() const;
	bool resetUsartMaster();
	usart_low_level_error sendOutputData(const char *Buffer, int Length, std::chrono::milliseconds WaitingTimeout, 
		std::chrono::milliseconds ResponseTimeout);
	usart_transfer_state collectInputDataUntilSilence();
	int getInputData(char *Buffer, int BufferSize);
};

bool usartMasterDebuggingTick(bool TerminationSignal, int PressedKey);

#endif // USART_MASTER_H
