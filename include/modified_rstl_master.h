/// @file modified_rstl_master.h

#ifndef MODIFIED_RSTL_MASTER_H
#define MODIFIED_RSTL_MASTER_H

#include <cstdint>
#include <string>

#include "usart_master.h"

enum class rstl_master_fsm_state {
	Uninitialized,
	IdleAndSend,
	WaitForResponse,
	Receive,
	Noise,
	CriticalError, // the application should be terminated
};

enum class single_order_code : uint8_t {
	None = 0,
	InquiryPower = 1,   // ?POWER
	SetPower = 2,       // POWER0, POWER1
	InquiryZ = 3,       // ?Z
	SetZ = 4,           // Z0, Z1
	InquiryPC = 5,      // ?PC
	SetPC = 6,          // PC 0, PC 1, PC -1, PC 3.2, PC -4.5
	InquiryST = 7,      // ST
	InquiryVersion = 8, // VERSION
	ResetErrors = 9     // RE
};

enum class order_processing_state { NotReady, Ready, Placed, InProgress };

class order_with_argument {
  public:
	single_order_code OrderCode = single_order_code::None;
	int16_t Argument = 0;
	order_with_argument() = default;
};

class modified_rstl_master {
  private:
	rstl_master_fsm_state CurrentState;
	low_level_usart_ptr LowLevelUsartPtr;
	order_processing_state OrderProcessingState;
	order_with_argument CurrentOrder;
	std::string CommandInTextFormat;
	uint64_t ResponseFailureHistory; // Bitfield tracking the history of response failures (1 = failure, 0 = success); LSB = the most recent response
	uint32_t TotalResponseCount;
	uint32_t TotalFailedResponseCount;

	void fsmTickUninitialized();
	void fsmTickIdleAndSend();
	void fsmTickWaitForResponse();
	void fsmTickReceive();
	void fsmTickNoise();

	void updateCommunicationQuality(bool SuccessfulCommunication); // Updates the communication quality metrics based on the latest response

	void placeOrderSetPC(const order_with_argument &NewOrder);

  public:
	modified_rstl_master();
	~modified_rstl_master();
	usart_low_level_error initialize();
	[[nodiscard]] rstl_master_fsm_state getCurrentState() const;
	[[nodiscard]] usart_low_level_error getUsartErrorCode() const;
	bool fsmTick(bool TerminationSignal);

	[[nodiscard]] order_processing_state getOrderProcessingState() const;
	bool placeOrder(const order_with_argument &NewOrder);
	[[nodiscard]] uint8_t getCurrentCommunicationQuality() const;
	[[nodiscard]] uint8_t getTotalCommunicationQuality() const;
};

typedef modified_rstl_master *modified_rstl_master_ptr;

usart_low_level_error initializeRstlMaster();

bool rstlMasterTick(bool TerminationSignal);

typedef void (*rstl_callback_function)(const char *CommandPtr, const char *ResponsePtr);

void registerRstlCallback(rstl_callback_function Callback);

bool placeRstlOrder(const order_with_argument &NewOrder);

#endif // MODIFIED_RSTL_MASTER_H
