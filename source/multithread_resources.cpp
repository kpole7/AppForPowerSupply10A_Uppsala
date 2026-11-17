/// @file multithread_resources.cpp
/// @brief Shared data between FLTK thread and the peripheral thread.

#include "multithread_resources.h"
#include <cstdint> // for uint16_t
#include <string>

//=================================================================================================
// Global variables
//=================================================================================================

std::atomic<bool> VerboseMode{false};

std::atomic<uint8_t> StatusLevelForGui{DEFAULT_STATUS_LEVEL_FOR_GUI};

std::string ThisApplicationDirectory;

std::string ConfigurationFilePath;

std::atomic<main_power_state> ReceivedMainPowerState{main_power_state::UNDEFINED};
std::atomic<int16_t> ReceivedSetpoint1{0};
std::atomic<int16_t> ReceivedSetpoint2{0};

std::atomic<bool> CommandToBeSentPowerOn{false};
std::atomic<bool> CommandToBeSentPowerOff{false};

std::atomic<bool> CommandToBeSentSetpoint1{false};
std::atomic<int16_t> ValueToBeSentSetpoint1{0};
std::atomic<bool> CommandToBeSentSetpoint2{false};
std::atomic<int16_t> ValueToBeSentSetpoint2{0};
