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
	None,
	InquiryPower,   // ?POWER
	SetPower,       // POWER0, POWER1
	InquiryZ,       // ?Z
	SetZ,           // Z0, Z1
	InquiryPC,      // ?PC
	SetPC,          // PC 0, PC 1, PC -1, PC 3.2, PC -4.5
	InquiryST,      // ST
	InquiryVersion, // VERSION
	ResetErrors     // RE
};

enum class order_processing_state { Ready, InProgress, Completed, Failed };

class order_with_arguments {
	single_order_code OrderCode;
	bool IsOn;
	uint8_t Channel;
	double Value;
};

class modified_rstl_master {
  private:
	rstl_master_fsm_state CurrentState;
	low_level_usart_ptr LowLevelUsartPtr;
	order_processing_state OrderProcessingState;
	order_with_arguments CurrentOrder;
	std::string ResponseBuffer;
	uint64_t ResponseFailureHistory; // Bitfield tracking the history of response failures (1 = failure, 0 = success); LSB = the most recent response
	uint32_t TotalResponseCount;
	uint32_t TotalFailedResponseCount;

	void updateCommunicationQuality(bool ResponseFailed); // Updates the communication quality metrics based on the latest response

  public:
	modified_rstl_master();
	~modified_rstl_master();
	usart_low_level_error initialize();
	rstl_master_fsm_state getCurrentState() const;
	usart_low_level_error getUsartErrorCode() const;
	bool fsmTick(bool TerminationSignal);

	order_processing_state getOrderProcessingState() const;
	order_with_arguments getCurrentOrder() const;
	void placeOrder(const order_with_arguments &NewOrder);
	std::string getResponse() const;
	uint8_t getCurrentCommunicationQuality() const;
	uint8_t getTotalCommunicationQuality() const;
};

typedef modified_rstl_master *modified_rstl_master_ptr;

usart_low_level_error initializeRstlMaster();

bool rstlMasterTick(bool TerminationSignal);

#endif // MODIFIED_RSTL_MASTER_H
