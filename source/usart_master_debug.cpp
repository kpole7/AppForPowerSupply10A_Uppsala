/// @file usart_master_debug.cpp
/// @brief Debugging utilities for the USART master module

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "debug_tools.h"
#include "settings_file.h"
#include "usart_master.h"
#include "usart_master_debug.h"

char UsartLowLevelStateTexts[4][15] = {"Uninitialized", "Open", "Closed", "Error"};

char UsartTransferStateTexts[11][21] = {"Idle",
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

char SingleCommandFsmStateTexts[10][21] = {"Uninitialized", "Closed", "Idle", "WaitingForResponse", "Receiving", "CriticalError", "NonCriticalError"};

enum class single_command_fsm_state : uint8_t {
	Uninitialized,
	Closed,
	Idle,
	WaitingForResponse,
	Receiving,

	CriticalError,
	NonCriticalError,
};

enum class single_order_code : uint8_t { InquiryPower, SetPower, InquiryZ, SetZ, InquiryPC, SetPC, InquiryST };

class single_order_package {
  public:
	single_order_code Code;
	bool IsOn;
	uint8_t Channel;
	double Value;
};

static single_command_fsm_state FsmState = single_command_fsm_state::Uninitialized;

// TODO
static char *CommandTextToBeSent = nullptr;

static void handleFsmUninitialized(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmClosed(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmIdle(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmWaitingForResponse(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmReceiving(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmCriticalError(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);
static void handleFsmNonCriticalError(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState);

static void printStateChanges(low_level_usart *UsartPtr, std::chrono::high_resolution_clock::time_point ReferenceTime, const char *UserEventName);

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they can be terminated
/// @callgraph
/// @callergraph
bool usartMasterDebuggingTick(bool TerminationSignal, int PressedKey) {
	static const char InquiryPOWER[] = "?POWER\r\n"; // a
	static const char InquiryZ[] = "?Z\r\n";         // b
	static const char InquiryPC[] = "?PC\r\n";       // c
	static const char InquiryST[] = "ST\r\n";        // d
	static const char SetPOWER1[] = "POWER1\r\n";    // e
	static const char SetPOWER0[] = "POWER0\r\n";    // f
	static const char SetZ1[] = "Z1\r\n";            // g
	static const char SetZ2[] = "Z2\r\n";            // h
	static const char SetPC0[] = "PC 0\r\n";         // i
	static const char SetPC1[] = "PC 1\r\n";         // j
	static const char SetPC2[] = "PC 2\r\n";         // k
	static const char SetPC01[] = "PC 0.1\r\n";      // l
	static const char SetPC07[] = "PC 0.7\r\n";      // m
	static const char SetPC09[] = "PC 0.9\r\n";      // n
	static const char SetPC11[] = "PC 1.1\r\n";      // o
	static const char SetPC17[] = "PC 1.7\r\n";      // p
	static const char SetPC19[] = "PC 1.9\r\n";      // q
	static const char SetPC_1[] = "PC -1\r\n";       // r
	static const char SetPC_2[] = "PC -2\r\n";       // s
	static const char SetPC_01[] = "PC -0.1\r\n";    // t
	static const char SetPC_07[] = "PC -0.7\r\n";    // u
	static const char SetPC_09[] = "PC -0.9\r\n";    // v
	static const char SetPC_11[] = "PC -1.1\r\n";    // w
	static const char SetPC_17[] = "PC -1.7\r\n";    // x
	static const char SetPC_19[] = "PC -1.9\r\n";    // y
	const char *CommandTexts[] = {&InquiryPOWER[0], &InquiryZ[0], &InquiryPC[0], &InquiryST[0], &SetPOWER1[0], &SetPOWER0[0], &SetZ1[0],
	                              &SetZ2[0],        &SetPC0[0],   &SetPC1[0],    &SetPC2[0],    &SetPC01[0],   &SetPC07[0],   &SetPC09[0],
	                              &SetPC11[0],      &SetPC17[0],  &SetPC19[0],   &SetPC_1[0],   &SetPC_2[0],   &SetPC_01[0],  &SetPC_07[0],
	                              &SetPC_09[0],     &SetPC_11[0], &SetPC_17[0],  &SetPC_19[0]};
	static low_level_usart *MyUsartPtr = nullptr;
    static uint16_t UnchangedStateCounter = 0;

	// Handle termination signal and cleanup if necessary
	if (TerminationSignal) {
		if (nullptr != MyUsartPtr) {
			delete MyUsartPtr;
			MyUsartPtr = nullptr;
		}
		return false;
	}

	static auto TimeReference = std::chrono::high_resolution_clock::now();

	// FSM state handling
    single_command_fsm_state StartState = FsmState;
	switch (FsmState) {
	case single_command_fsm_state::Uninitialized:
		handleFsmUninitialized(MyUsartPtr, FsmState);
		break;
	case single_command_fsm_state::Closed:
		handleFsmClosed(MyUsartPtr, FsmState);
		break;
	case single_command_fsm_state::Idle:

		if ((PressedKey >= 'a') && (PressedKey <= 'y')) {
			CommandTextToBeSent = const_cast<char *>(CommandTexts[PressedKey - 'a']);
			static char Buffer[30];
			snprintf(Buffer, sizeof(Buffer) - 1, "cmd: %s", CommandTextToBeSent);
            char *UnprintablePtr = Buffer;
            while (0 != *UnprintablePtr) {
                if (!isprint(*UnprintablePtr)) {
                    *UnprintablePtr = '~';
                }
                UnprintablePtr++;
            }
			printStateChanges(MyUsartPtr, TimeReference, Buffer);
		}
		else {
			CommandTextToBeSent = nullptr;
		}

        handleFsmIdle(MyUsartPtr, FsmState);

		break;
	case single_command_fsm_state::WaitingForResponse:
		handleFsmWaitingForResponse(MyUsartPtr, FsmState);
		break;
	case single_command_fsm_state::Receiving:
		handleFsmReceiving(MyUsartPtr, FsmState);
		break;

	case single_command_fsm_state::CriticalError:
		handleFsmCriticalError(MyUsartPtr, FsmState);
		break;
	case single_command_fsm_state::NonCriticalError:
		handleFsmNonCriticalError(MyUsartPtr, FsmState);
		break;
	default:
		// Handle unexpected states

		break;
	}
    if (StartState == FsmState) {
        if (UnchangedStateCounter < 0xFFFF) {
            UnchangedStateCounter++;
        }
    }
    else {
        UnchangedStateCounter = 0;
    }

	switch (PressedKey) {
	case 'R':
		printStateChanges(MyUsartPtr, TimeReference, "Attempt to reset");
		{
			bool ResetSuccess = MyUsartPtr->resetUsartMaster();
			if (!ResetSuccess) {
				std::cout << "Failed to reset USART master\n";
			}
			else {
				FsmState = single_command_fsm_state::Idle;
				std::cout << "USART master reset successfully\n";
			}
		}
		break;
	case 'T':
		TimeReference = std::chrono::high_resolution_clock::now();
		break;
	case 'S':
        {
            char Buffer[50];
            snprintf(Buffer, sizeof(Buffer) - 1, "usr; max=%u", LongestTimeTicsBehindSchedule);
    		printStateChanges(MyUsartPtr, TimeReference, Buffer);
        }
		break;
	default:
		break;
	}

	printStateChanges(MyUsartPtr, TimeReference, nullptr);

	return (nullptr != MyUsartPtr);
}

static void handleFsmUninitialized(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr == UsartPtr);
	assert(single_command_fsm_state::Uninitialized == FsmState);
	UsartPtr = new low_level_usart();
	if (nullptr == UsartPtr) {
		std::cerr << "Failed to create USART instance\n";
		FsmState = single_command_fsm_state::CriticalError;
		return;
	}
	FsmState = single_command_fsm_state::Closed;
}

static void handleFsmClosed(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::Closed == FsmState);
	assert(0 != SerialPortName.length());
	usart_low_level_error Error = UsartPtr->openUsart(SerialPortName);
	if (Error != usart_low_level_error::None) {
		std::cerr << "Failed to open USART\n";
		FsmState = single_command_fsm_state::NonCriticalError; // Failed to open USART
		return;
	}
	FsmState = single_command_fsm_state::Idle; // Transition to idle state
}

static void handleFsmIdle(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {

    if ((nullptr == CommandTextToBeSent) || (0 == strlen(CommandTextToBeSent))) {
        return;
    }

	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::Idle == FsmState);
	usart_low_level_error Error = UsartPtr->sendOutputData(CommandTextToBeSent, strlen(CommandTextToBeSent),
	                                                       static_cast<std::chrono::milliseconds>(200), static_cast<std::chrono::milliseconds>(100));
	if (Error != usart_low_level_error::None) {
		std::cout << "Failed to send data\n";
		FsmState = single_command_fsm_state::CriticalError; // Failed to send data
		return;
	}
	FsmState = single_command_fsm_state::WaitingForResponse; // Transition to receiving state
}

static void handleFsmWaitingForResponse(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::WaitingForResponse == FsmState);
	UsartPtr->collectInputDataUntilSilence();
	

}

static void handleFsmReceiving(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::Receiving == FsmState);
}

static void handleFsmCriticalError(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::CriticalError == FsmState);
}

static void handleFsmNonCriticalError(low_level_usart_ptr &UsartPtr, single_command_fsm_state &FsmState) {
	assert(nullptr != UsartPtr);
	assert(single_command_fsm_state::NonCriticalError == FsmState);
}

/// @callgraph
/// @callergraph
static void printStateChanges(low_level_usart *UsartPtr, std::chrono::high_resolution_clock::time_point ReferenceTime, const char *UserEventName) {
	if (nullptr == UsartPtr) {
		return;
	}
	static usart_low_level_state LastState = usart_low_level_state::Uninitialized;
	static usart_transfer_state LastTransferState = usart_transfer_state::Idle;
	static usart_low_level_error LastErrorCode = usart_low_level_error::None;
	static single_command_fsm_state LastFsmState = single_command_fsm_state::Uninitialized;
	bool StateChanged = (LastState != UsartPtr->getState());
	bool TransferStateChanged = (LastTransferState != UsartPtr->getTransferState());
	bool ErrorChanged = (LastErrorCode != UsartPtr->getErrorCode());
	bool FsmStateChanged = (LastFsmState != FsmState);
	bool UserEventOccurred = (nullptr != UserEventName);
	bool AnyChange = (StateChanged || TransferStateChanged || ErrorChanged || FsmStateChanged || UserEventOccurred);

	if (AnyChange) {
		auto TimeNow = std::chrono::high_resolution_clock::now();
		auto TimeSinceReference = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - ReferenceTime).count();
		std::cout << formatElapsedTime(TimeSinceReference) << " ";
	}

	if (StateChanged) {
		std::cout << "L-L State [" << UsartLowLevelStateTexts[static_cast<int>(LastState)] << "] -> ["
		          << UsartLowLevelStateTexts[static_cast<int>(UsartPtr->getState())] << "]  ";
	}
	else {
		if (AnyChange) {
			std::cout << "L-L State [[" << UsartLowLevelStateTexts[static_cast<int>(LastState)] << "]]  ";
		}
	}
	if (TransferStateChanged) {
		std::cout << "Transfer state [" << UsartTransferStateTexts[static_cast<int>(LastTransferState)] << "] -> ["
		          << UsartTransferStateTexts[static_cast<int>(UsartPtr->getTransferState())] << "]  ";
	}
	else {
		if (AnyChange) {
			std::cout << "Transfer state [[" << UsartTransferStateTexts[static_cast<int>(LastTransferState)] << "]]  ";
		}
	}
	if (ErrorChanged) {
		std::cout << "Error [" << static_cast<int>(LastErrorCode) << "] -> [" << static_cast<int>(UsartPtr->getErrorCode()) << "]";
	}
	else {
		if (AnyChange) {
			std::cout << "Error [[" << static_cast<int>(LastErrorCode) << "]]  ";
		}
	}
	if (FsmStateChanged) {
		std::cout << "FSM state [" << SingleCommandFsmStateTexts[static_cast<int>(LastFsmState)] << "] -> ["
		          << SingleCommandFsmStateTexts[static_cast<int>(FsmState)] << "]";
	}
	else {
		if (AnyChange) {
			std::cout << "FSM state [[" << SingleCommandFsmStateTexts[static_cast<int>(LastFsmState)] << "]]";
		}
	}
	if (UserEventOccurred) {
		std::cout << "  (" << UserEventName << ')';
	}

	if (AnyChange) {
		std::cout << '\n';
	}

	LastState = UsartPtr->getState();
	LastTransferState = UsartPtr->getTransferState();
	LastErrorCode = UsartPtr->getErrorCode();
	LastFsmState = FsmState;
}
