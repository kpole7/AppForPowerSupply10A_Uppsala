/// @file test_communication_quality.cpp
/// @brief Tests that allow for manual verification of the communication_quality module.

#include "communication_quality.h"
#include "debug_tools.h"
#include <iostream>

bool DebuggingSignalPlusOne;
bool DebuggingSignalMinusOne;
bool DebuggingSignalMinusTen;
bool DebuggingSignalEscape;


void communicationQualityTick(int JustPressedKey);



int main() {
    uint8_t CurrentQuality;
    uint8_t TotalQuality;

    // Initialize debugging tools if necessary
    debugPeripheralsInitializations();

    std::cout << "Hello" << '\n';

    while (true) {
        debuggingToolsTick(false);
        communicationQualityTick(DebuggingSignalJustPressedKey);

        if (DebuggingSignalPlusOne) {
            DebuggingSignalPlusOne = false;
            updateCommunicationQuality(1);
            CurrentQuality = getCurrentCommunicationQuality();
            TotalQuality = getTotalCommunicationQuality();
            std::cout << "Current Quality: " << static_cast<int>(CurrentQuality)
                      << " %, Total Quality: " << static_cast<int>(TotalQuality) << " %\n";
        }
        if (DebuggingSignalMinusOne) {
            DebuggingSignalMinusOne = false;
            updateCommunicationQuality(-1);
            CurrentQuality = getCurrentCommunicationQuality();
            TotalQuality = getTotalCommunicationQuality();
            std::cout << "Current Quality: " << static_cast<int>(CurrentQuality)
                      << " %, Total Quality: " << static_cast<int>(TotalQuality) << " %\n";
        }
        if (DebuggingSignalMinusTen) {
            DebuggingSignalMinusTen = false;
            updateCommunicationQuality(-10);
            CurrentQuality = getCurrentCommunicationQuality();
            TotalQuality = getTotalCommunicationQuality();
            std::cout << "Current Quality: " << static_cast<int>(CurrentQuality)
                      << " %, Total Quality: " << static_cast<int>(TotalQuality) << " %\n";
        }

        if (DebuggingSignalEscape) {
            break;
        }
    }
    debuggingToolsTick(true);
    return 0;
}

void communicationQualityTick(int JustPressedKey) {
	switch (JustPressedKey) {
	case '+':
		DebuggingSignalPlusOne = true;
		break;

	case '-':
		DebuggingSignalMinusOne = true;
		break;

	case '_':
		DebuggingSignalMinusTen = true;
		break;

	case 27:
		DebuggingSignalEscape = true;
		break;

	default:
		break;
	}
}
