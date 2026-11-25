/// @file communication_quality.h
/// @brief Provides definitions and utilities for handling communication quality metrics.

#ifndef COMMUNICATION_QUALITY_H
#define COMMUNICATION_QUALITY_H

#include <cstdint>

//=================================================================================================
// Global function declarations
//=================================================================================================

void updateCommunicationQuality(int8_t Weight);
uint8_t getCurrentCommunicationQuality();
uint8_t getTotalCommunicationQuality();

#endif // COMMUNICATION_QUALITY_H
