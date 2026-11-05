/// @file usart_master.h

#ifndef USART_MASTER_H
#define USART_MASTER_H

#include <cassert>
#include <chrono>
#include <cinttypes>
#include <string>

#include "config.h"

/// @brief Size of the USART buffer
#define USART_BUFFER_SIZE 300

enum class usart_low_level_state {
	Uninitialized,
	Open, // normal operation
	Error
};

enum class usart_low_level_error {
	None,
	UsartDeviceNotFound, // port manipulation error
	OpenFailed,          // port manipulation error
	SelectFailed,        // port manipulation error
	ReadFailed,          // port manipulation error
	WriteFailed,         // port manipulation error
	InternalError,       // internal error within the usart_master module
	CallError            // internal error within this module
};

class low_level_usart {
  private:
	usart_low_level_state UsartState;
	usart_low_level_error UsartErrorCode;
	std::string *NameOfPort;
	int SerialPortHandler;
	char SendingBuffer[USART_BUFFER_SIZE];
	char ReceivingBuffer[USART_BUFFER_SIZE];
	uint16_t TotalCommandBytes;
	uint16_t TotalReceivedBytes;
	uint16_t ReceivedBytesLimit;
	std::chrono::milliseconds WaitingLimit; // from the moment transmission begins until the first byte of the response is received
	std::chrono::high_resolution_clock::time_point TimeWhenOutgoingDataStarted;
	std::chrono::high_resolution_clock::time_point TimeWhenResponseStarted;
	bool TerminationFlag;

	void handleCriticalError(int Line);
	void checkSilence(bool *IsSilencePtr);
	void flushBuffer();

	void doIdle(bool *IsSilencePtr, const char *CommandText, uint16_t ResponseLimit);
	void doSending();
	void doWaitForResponse(bool *IsTimeoutPtr, bool *IsSilencePtr);
	void doReceiveFirstData(bool *IsOverLimitsPtr);

  public:
	low_level_usart();
	~low_level_usart();
	usart_low_level_error startModule(const std::string &Name);
	[[nodiscard]] std::string getName() const;
	[[nodiscard]] usart_low_level_state getState() const;
	[[nodiscard]] usart_low_level_error getErrorCode() const;
	[[nodiscard]] const char *getSendingBuffer() const;
	[[nodiscard]] const char *getReceivingBuffer() const;

	[[nodiscard]] usart_low_level_error handleIdleAndSendState(bool *IsSilencePtr, const char *CommandTextPtr, uint16_t ResponseLimit);
	[[nodiscard]] usart_low_level_error handleWaitForResponseState(bool *IsTimeoutExceededPtr, bool *IsSilencePtr, bool *IsOverLimitsPtr);
	[[nodiscard]] usart_low_level_error handleReceiveState(bool *IsSilencePtr, bool *IsOverLimitsPtr);
	[[nodiscard]] usart_low_level_error handleNoiseState(bool *IsSilencePtr);
};

typedef low_level_usart *low_level_usart_ptr;

#endif // USART_MASTER_H
