/// @file communication_quality.cpp
/// @brief Implements functions and utilities for handling communication quality metrics.

#include <cassert>
#include <iostream>

#include "communication_quality.h"
#include "debug_tools.h"

//=================================================================================================
// Local variables
//=================================================================================================

static communication_quality CommunicationQualityInstance;

//=================================================================================================
// Local class member function implementations
//=================================================================================================

communication_quality::communication_quality()
    : ResponseFailureHistory(0), TotalResponseCount(0), TotalFailedResponseCount(0) {}

void communication_quality::update(int8_t Weight) {
	const uint64_t Ones[21] = {0,         0x1ULL,    0x3ULL,    0x7ULL,     0xFULL,     0x1FULL,    0x3FULL,
	                           0x7FULL,   0xFFULL,   0x1FFULL,  0x3FFULL,   0x7FFULL,   0xFFFULL,   0x1FFFULL,
	                           0x3FFFULL, 0x7FFFULL, 0xFFFFULL, 0x1FFFFULL, 0x3FFFFULL, 0x7FFFFULL, 0xFFFFFULL};
	assert((Weight > -20) && (Weight < 20) && (Weight != 0));
	ResponseFailureHistory <<= Weight;
	if (Weight < 0) {
		ResponseFailureHistory |= Ones[-Weight];
		TotalFailedResponseCount += -Weight;
		TotalResponseCount += -Weight;
	}
	else {
		TotalResponseCount += Weight;
	}
	if (UINT32_MAX - (uint32_t)20 < TotalResponseCount) {
		// Prevent overflow by scaling down the counts
		TotalResponseCount /= 2;
		TotalFailedResponseCount /= 2;
	}
}

uint8_t communication_quality::getCurrentQuality() const {
	if (TotalResponseCount == 0) {
		return 100;
	}
	uint16_t FailureCount = 0;
	for (int i = 0; i < 64; ++i) {
		if ((ResponseFailureHistory & (1ULL << i)) != 0) {
			FailureCount++;
		}
	}
	uint16_t SuccessCount = 64 - FailureCount;
	SuccessCount *= 1000;

	uint16_t CurrentQuality = (SuccessCount + 32) / 640;

#ifdef DEBUG_COMMUNICATION_QUALITY
	//		DebuggingFlagPrinting = false;

	std::cout << "getCurrentQuality: " << std::hex << ResponseFailureHistory << std::dec << " "
				<< static_cast<int>(64 - FailureCount) << " / 64 = " << static_cast<int>(CurrentQuality)
				<< " %             ";
#endif
	return static_cast<uint8_t>(CurrentQuality);
}

uint8_t communication_quality::getTotalQuality() const {
	if (TotalResponseCount == 0) {
		return 100;
	}
	uint32_t LocalTotalCount = TotalResponseCount;
	uint32_t LocalSuccessCount = TotalResponseCount - TotalFailedResponseCount;

	if (LocalTotalCount > (uint32_t)0x200000U) { // see calculations.ods
		LocalTotalCount /= 1000;
	}
	else {
		LocalSuccessCount *= 1000;
	}
	uint32_t LocalQualityPerMille = LocalSuccessCount / LocalTotalCount;

	uint8_t LocalQuality = static_cast<uint8_t>((LocalQualityPerMille + 5) / 10);

#ifdef DEBUG_COMMUNICATION_QUALITY
	std::cout << "getTotalQuality: " << static_cast<int>(LocalSuccessCount) << " / "
	          << static_cast<int>(TotalResponseCount) << " = " << static_cast<int>(LocalQuality) << " %\n";
#endif
	return LocalQuality;
}

//=================================================================================================
// Global function declarations
//=================================================================================================

void updateCommunicationQuality(int8_t Weight) { CommunicationQualityInstance.update(Weight); }

uint8_t getCurrentCommunicationQuality() { return CommunicationQualityInstance.getCurrentQuality(); }

uint8_t getTotalCommunicationQuality() { return CommunicationQualityInstance.getTotalQuality(); }
