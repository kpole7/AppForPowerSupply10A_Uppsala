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

static const char CommandA[] = "?POWER\r\n";   // a
static const char CommandB[] = "?Z\r\n";       // b
static const char CommandC[] = "?PC\r\n";      // c
static const char CommandD[] = "ST\r\n";       // d
static const char CommandE[] = "VERSION\r\n";  // e
static const char CommandF[] = "RE\r\n";       // f
static const char CommandG[] = "POWER1\r\n";   // g
static const char CommandH[] = "POWER0\r\n";   // h
static const char CommandI[] = "Z1\r\n";       // i
static const char CommandJ[] = "Z2\r\n";       // j
static const char CommandK[] = "PC 0\r\n";     // k
static const char CommandL[] = "PC 1\r\n";     // l
static const char CommandM[] = "PC 2\r\n";     // m
static const char CommandN[] = "PC 0.1\r\n";   // n
static const char CommandO[] = "PC 0.7\r\n";   // o
static const char CommandP[] = "PC 0.59\r\n";  // p
static const char CommandQ[] = "PC 1.7\r\n";   // q
static const char CommandR[] = "PC 10.0\r\n";  // r
static const char CommandS[] = "PC 10.00\r\n"; // s
static const char CommandT[] = "PC -1\r\n";    // t
static const char CommandU[] = "PC -2\r\n";    // u
static const char CommandV[] = "PC -0.1\r\n";  // v
static const char CommandW[] = "PC -0.7\r\n";  // w
static const char CommandX[] = "PC -0.59\r\n"; // x
static const char CommandY[] = "PC -1.7\r\n";  // y
static const char CommandZ[] = "PC -10.00\r\n";// z
const char *CommandTexts[] = {&CommandA[0], &CommandB[0], &CommandC[0], &CommandD[0], &CommandE[0], &CommandF[0], &CommandG[0],
                              &CommandH[0], &CommandI[0], &CommandJ[0], &CommandK[0], &CommandL[0], &CommandM[0], &CommandN[0],
                              &CommandO[0], &CommandP[0], &CommandQ[0], &CommandR[0], &CommandS[0], &CommandT[0], &CommandU[0],
                              &CommandV[0], &CommandW[0], &CommandX[0], &CommandY[0], &CommandZ[0]};

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
	if ((PressedKey >= 'a') && (PressedKey <= 'z')) {
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
