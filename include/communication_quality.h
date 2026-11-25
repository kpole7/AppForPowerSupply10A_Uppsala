/// @file communication_quality.h
/// @brief Provides definitions and utilities for handling communication quality metrics.

#ifndef COMMUNICATION_QUALITY_H
#define COMMUNICATION_QUALITY_H

#include <cstdint>

//=================================================================================================
// Class definitions
//=================================================================================================

class communication_quality {
  public:
	communication_quality();
	void update(int8_t Weight);
	[[nodiscard]] uint8_t getCurrentQuality() const;
	[[nodiscard]] uint8_t getTotalQuality() const;

  private:
	uint64_t ResponseFailureHistory; // Bitfield tracking the history of response failures (1 = failure, 0 = success);
	                                 // LSB = the most recent response
	uint32_t TotalResponseCount;
	uint32_t TotalFailedResponseCount;
};

//=================================================================================================
// Global function declarations
//=================================================================================================

void updateCommunicationQuality(int8_t Weight);
uint8_t getCurrentCommunicationQuality();
uint8_t getTotalCommunicationQuality();

#endif // COMMUNICATION_QUALITY_H
