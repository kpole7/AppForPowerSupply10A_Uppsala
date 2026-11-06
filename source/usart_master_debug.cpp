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

order_with_argument CurrentOrderForDebugging;

uint16_t ExpectedResponseLimit;

static auto TimeReference = std::chrono::high_resolution_clock::now();

static const order_with_argument MyOrders[26] = {
    {single_order_code::InquiryPower, 0},   // a
    {single_order_code::InquiryZ, 0},       // b
    {single_order_code::InquiryPC, 0},      // c
    {single_order_code::InquiryST, 0},      // d
    {single_order_code::InquiryVersion, 0}, // e
    {single_order_code::ResetErrors, 0},    // f
    {single_order_code::SetPower, 1},       // g
    {single_order_code::SetPower, 0},       // h
    {single_order_code::SetZ, 1},           // i
    {single_order_code::SetZ, 2},           // j
    {single_order_code::SetPC, 0},          // k
    {single_order_code::SetPC, 100},        // l
    {single_order_code::SetPC, 200},        // m
    {single_order_code::SetPC, 10},         // n
    {single_order_code::SetPC, 70},         // o
    {single_order_code::SetPC, 59},         // p
    {single_order_code::SetPC, 170},        // q
    {single_order_code::SetPC, 999},        // r
    {single_order_code::SetPC, 1000},       // s
    {single_order_code::SetPC, -100},       // t
    {single_order_code::SetPC, -200},       // u
    {single_order_code::SetPC, -10},        // v
    {single_order_code::SetPC, -70},        // w
    {single_order_code::SetPC, -59},        // x
    {single_order_code::SetPC, -170},       // y
    {single_order_code::SetPC, -1000}       // z
};

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
	if ((PressedKey >= 'a') && (PressedKey <= 'z')) {
		int Index = static_cast<int>(PressedKey - 'a');
		CurrentOrderForDebugging = MyOrders[Index];
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
