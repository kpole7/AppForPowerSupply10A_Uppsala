/// @file usart_master.h

#ifndef USART_MASTER_H
#define USART_MASTER_H

#include <cassert>
#include <chrono>
#include <inttypes.h>
#include <string>

static_assert(sizeof(int) == 4, "Expected int to be 4 bytes.");

#define MAX_RESPONSE_LENGTH 300

enum class usart_low_level_state { Uninitialized, Open, Closed, Error };

enum class usart_low_level_error {
	None,
	UsartDeviceNotFound,
	OpenFailed,
	SelectFailed,
	ReadFailed,
	WriteFailed,
	InternalError,
	SlaveOutOfOrder,
	CallError,
	Unknown
};

enum class usart_transfer_state {
	Idle, // wait for a new outgoing data to be sent
	Sending,
	WaitingForResponse,
	Receiving,
	Completed,
	WaitingLimitExceeded, // the slave did not respond within the expected waiting limit
	IncorrectlyReceiving, // if the response is too long for a single command but not long enough to be considered a permanent error
	IncorrectlyCompleted, // ingoing transfer has ended but the response is too long
	IncorrectlyNoisy,     // fatal error due to excessive noise on the line
	InternalError,
	CallError
};

class low_level_usart {
  private:
	usart_low_level_state State;
	usart_transfer_state TransferState;
	usart_low_level_error ErrorCode;
	std::string *NameOfPort;
	int SerialPortHandler;
	char TotalResponse[MAX_RESPONSE_LENGTH];
	uint16_t TotalReceivedBytes;
	std::chrono::high_resolution_clock::time_point TimeWhenOutgoingDataEnded;
	std::chrono::high_resolution_clock::time_point TimeWhenResponseStarted;
	std::chrono::high_resolution_clock::time_point TimeWhenReceivedLastByte;
	std::chrono::milliseconds WaitingLimit;
	std::chrono::milliseconds ResponseDurationLimit;

	bool validateCollectionState();
	usart_transfer_state failCollection(usart_transfer_state NewTransferState, int Line = __LINE__);
	usart_transfer_state readAvailableInput(std::chrono::high_resolution_clock::time_point TimeNow);
	usart_transfer_state processReceivedInput(const char *ReceivedBytes, int NumberOfReceived,
	                                          std::chrono::high_resolution_clock::time_point TimeNow);
	usart_transfer_state handleSilence(std::chrono::high_resolution_clock::time_point TimeNow);

  public:
	low_level_usart();
	~low_level_usart();
	void openUsart(const std::string &Name);
	[[nodiscard]] std::string getName() const;
	[[nodiscard]] usart_low_level_state getState() const;
	[[nodiscard]] usart_transfer_state getTransferState() const;
	[[nodiscard]] usart_low_level_error getErrorCode() const;
	void resetUsartMaster();
	void sendOutputData(const char *Buffer, int Length, std::chrono::milliseconds WaitingTimeout, std::chrono::milliseconds ResponseTimeout);
	usart_transfer_state collectInputDataUntilSilence();
	int getInputData(char *Buffer, int BufferSize);
};

#endif // USART_MASTER_H
