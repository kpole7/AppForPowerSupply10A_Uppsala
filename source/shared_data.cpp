/// @file shared_data.cpp
/// @brief Shared data between FLTK thread and the peripheral thread.

#include <atomic>
#include <cstdint> // for uint16_t
#include <string>
#include "shared_data.h"

//...............................................................................................
// Global variables
//...............................................................................................

std::atomic<bool> VerboseMode{false};

int StatusLevelForGui;

std::string ThisApplicationDirectory;

std::string ConfigurationFilePath;

std::atomic<uint16_t> PeripheralsRunningOutOfTimeCounter{0};

std::atomic<main_power_state> SharedMainPowerState{main_power_state::UNDEFINED};

