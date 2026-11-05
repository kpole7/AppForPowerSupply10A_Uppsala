/// @file usart_master_debug.cpp
/// @brief Debugging utilities for the USART master module

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "debug_tools.h"
#include "modified_rstl_master.h"
#include "settings_file.h"
#include "usart_master.h"
#include "usart_master_debug.h"

const char *CurrentCommandTextPtr;

uint16_t ExpectedResponseLimit;

static auto TimeReference = std::chrono::high_resolution_clock::now();

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

void printResults(const char *ResultText1, const char *ResultText2) {
	static char MyBuffer1[300];
	static char MyBuffer2[300];
	strncpy(MyBuffer1, ResultText1, sizeof(MyBuffer1) - 1);
	MyBuffer1[sizeof(MyBuffer1) - 1] = '\0';
	strncpy(MyBuffer2, ResultText2, sizeof(MyBuffer2) - 1);
	MyBuffer2[sizeof(MyBuffer2) - 1] = '\0';
	int Length1 = static_cast<int>(strlen(MyBuffer1));
	int Length2 = static_cast<int>(strlen(MyBuffer2));
	if (Length1 > 2) {
		for (int J = 0; J < Length1; ++J) {
			if (MyBuffer1[J] < ' ') {
				MyBuffer1[J] = '~';
			}
		}
	}
	if (Length2 > 2) {
		for (int J = 0; J < Length2; ++J) {
			if (MyBuffer2[J] < ' ') {
				MyBuffer2[J] = '~';
			}
		}
	}
	std::cout << "Command:  [" << MyBuffer1 << "]\n";
	std::cout << "Response: [" << MyBuffer2 << "]\n";
}



void usartMasterDebuggingInitialize() {
	CurrentCommandTextPtr = nullptr;
	ExpectedResponseLimit = 0;
	registerRstlCallback(printResults);
}

char UsartLowLevelStateTexts[4][15] = {"Uninitialized", "Open", "Error"};

char RstlMasterFsmStateTexts[6][16] = {"Uninitialized", "IdleAndSend", "WaitForResponse", "Receive", "Noise", "CriticalError"};

char SingleCommandFsmStateTexts[10][21] = {"Uninitialized", "Closed", "Idle", "WaitingForResponse", "Receiving", "CriticalError", "NonCriticalError"};

/// @brief Handles a single tick of peripheral operations.
/// @param PressedKey the key that was pressed by the user
/// @callgraph
/// @callergraph
void usartMasterDebuggingTick(int PressedKey) {
	if ((PressedKey >= 'a') && (PressedKey <= 'y')) {
		int Index = static_cast<int>(PressedKey - 'a');
		CurrentCommandTextPtr = CommandTexts[Index];
		ExpectedResponseLimit = 100; // Set the expected response limit as needed
	}


	switch (PressedKey) {
	case 'T':
		TimeReference = std::chrono::high_resolution_clock::now();
		break;

	case 'S':
		std::cout << "LongestTimeTicsBehindSchedule=" << LongestTimeTicsBehindSchedule << '\n';
		break;

	default:
		break;
	}
}

/// @callgraph
/// @callergraph
void printStateChanges(low_level_usart *UsartPtr, rstl_master_fsm_state FsmState, const char *UserEventName) {
	if (nullptr == UsartPtr) {
		return;
	}
	static usart_low_level_state LastState = usart_low_level_state::Uninitialized;
	static usart_low_level_error LastErrorCode = usart_low_level_error::None;
	static rstl_master_fsm_state LastFsmState = rstl_master_fsm_state::Uninitialized;
	bool StateChanged = (LastState != UsartPtr->getState());
	bool ErrorChanged = (LastErrorCode != UsartPtr->getErrorCode());
	bool FsmStateChanged = (LastFsmState != FsmState);
	bool UserEventOccurred = (nullptr != UserEventName);
	bool AnyChange = (StateChanged || ErrorChanged || FsmStateChanged || UserEventOccurred);

	if (AnyChange) {
		auto TimeNow = std::chrono::high_resolution_clock::now();
		auto TimeSinceReference = std::chrono::duration_cast<std::chrono::milliseconds>(TimeNow - TimeReference).count();
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
	if (ErrorChanged) {
		std::cout << "Error [" << static_cast<int>(LastErrorCode) << "] -> [" << static_cast<int>(UsartPtr->getErrorCode()) << "]";
	}
	else {
		if (AnyChange) {
			std::cout << "Error [[" << static_cast<int>(LastErrorCode) << "]]  ";
		}
	}
	if (FsmStateChanged) {
		std::cout << "FSM state [" << RstlMasterFsmStateTexts[static_cast<int>(LastFsmState)] << "] -> ["
		          << RstlMasterFsmStateTexts[static_cast<int>(FsmState)] << "]";
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
	LastErrorCode = UsartPtr->getErrorCode();
	LastFsmState = FsmState;
}
