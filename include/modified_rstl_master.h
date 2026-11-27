/// @file modified_rstl_master.h

#ifndef MODIFIED_RSTL_MASTER_H
#define MODIFIED_RSTL_MASTER_H

#include <cstdint>
#include <string>

#include "usart_master.h"

//=================================================================================================
// Classes and types
//=================================================================================================

enum class rstl_master_fsm_state {
	Uninitialized,
	IdleAndSend,
	WaitForResponse,
	Receive,
	Noise,
	CriticalError, // the application should be terminated
};

enum class single_order_code : uint8_t {
	NoOrder = 0,
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
	single_order_code OrderCode = single_order_code::NoOrder;
	int16_t Argument = 0;
	order_with_argument() = default;
};

class modified_rstl_master {
  private:
	rstl_master_fsm_state CurrentState;
	low_level_usart_ptr LowLevelUsartPtr;
	order_processing_state OrderProcessingState;
	order_with_argument CurrentOrder;
	bool IsResentTransmissionSuccessful;
	mutable int16_t ReceivedValueInResponse;
	std::string CommandInTextFormat;

	void fsmTickUninitialized();
	void fsmTickIdleAndSend();
	void fsmTickWaitForResponse();
	void fsmTickReceive();
	void fsmTickNoise();

	void placeOrderSetPC(const order_with_argument &NewOrder);
	bool parseResponse(int16_t &ValueReturnedInResponse, uint8_t &EssentialResponseStart,
	                   uint8_t &EssentialResponseLength) const;

  public:
	modified_rstl_master();
	~modified_rstl_master();
	usart_low_level_error initialize();
	[[nodiscard]] rstl_master_fsm_state getCurrentState() const;
	[[nodiscard]] usart_low_level_error getUsartErrorCode() const;
	bool fsmTick(bool TerminationSignal);

	[[nodiscard]] order_processing_state getOrderProcessingState() const;
	bool placeOrder(const order_with_argument &NewOrder);
	[[nodiscard]] bool getIsResponseSuccessful() const;
	[[nodiscard]] int16_t getReceivedNumericalData() const;
	[[nodiscard]] single_order_code getRecentOrder() const;
};

typedef modified_rstl_master *modified_rstl_master_ptr;

//=================================================================================================
// Prototypes of global functions
//=================================================================================================

usart_low_level_error initializeRstlMaster();

[[nodiscard]] rstl_master_fsm_state getRstlMasterState(usart_low_level_error &ErrorCode);

bool rstlMasterTick(bool TerminationSignal);

typedef void (*rstl_callback_function)(bool IsResponseSuccessful, int16_t ValueReturnedInResponse,
                                       const char *EssentialResponsePtr, uint8_t EssentialResponseLength);

void registerRstlCallback(rstl_callback_function Callback);

bool placeRstlOrder(const order_with_argument &NewOrder);

order_processing_state getOrderProcessingState();

[[nodiscard]] bool getIsResponseSuccessful();

[[nodiscard]] int16_t getReceivedNumericalData();

[[nodiscard]] single_order_code getRecentOrder();

#endif // MODIFIED_RSTL_MASTER_H