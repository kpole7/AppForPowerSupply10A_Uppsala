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

std::atomic<main_power_state> SharedMainPowerState{main_power_state::UNDEFINED};

std::string ThisApplicationDirectory;

std::string ConfigurationFilePath;
