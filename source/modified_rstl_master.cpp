/// @file modified_rstl_master.cpp
/// @brief Implementation of the modified RSTL protocol for the master device.
/// @details The protocol is based on RSTL 488 Programmer protocol specifications (Electronic Measurements, Inc.)
/// This implementation handles communication with the procedures defined in the usart_master module.
/// This implementation is driven by the peripheral_thread module. The usart_master module sends and receives
/// continuous data packets. This module is responsible for syntax and timing of the communication.
/// This module manages errors reported by the usart_master module. The module's function is to generate commands
/// that comply with the protocol, and then to receive and interpret the responses. In addition, it ensures proper
/// that complies with the protocol, and then to receive and interpret the responses. In addition, it ensures proper
/// error handling (for syntax and timing errors). This module covers handling of single commands. Another
/// higher-level module is responsible for creating and managing sequences of commands.

#include "modified_rstl_master.h"
#include "peripheral_thread.h"
#include "usart_master.h"
