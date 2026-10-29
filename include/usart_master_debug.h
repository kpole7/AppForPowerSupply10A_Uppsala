/// @file usart_master_debug.h
/// @brief Debugging utilities for the USART master module

#ifndef USART_MASTER_DEBUG_H_
#define USART_MASTER_DEBUG_H_

enum class usart_transfer_state {
	Idle,               // normal operation; wait for a new outgoing data to be sent
	Sending,            // normal operation
	WaitingForResponse, // normal operation
	Receiving,          // normal operation
	Executing,          // normal operation
	IncorrectNoise,     // non-critical error: if the slave is sending unexpected data and the transfer is still ongoing
	InternalError,
	CallError
};

enum class usart_low_level_order_status {
	NotReady,  // new order cannot be placed
	Ready,     // new order can be placed
	Busy,      // order is being processed
	Completed, // response can be retrieved
	Failed     // order processing failed
};

#endif // USART_MASTER_DEBUG_H_