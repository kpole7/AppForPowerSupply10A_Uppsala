/// @file shared_resources.cpp
/// @brief Shared data between FLTK thread and the peripheral thread.

#include "shared_resources.h"
#include <atomic>
#include <cstdint> // for uint16_t
#include <string>

//...............................................................................................
// Global variables
//...............................................................................................

std::atomic<bool> VerboseMode{false};

int StatusLevelForGui;

std::string ThisApplicationDirectory;

std::string ConfigurationFilePath;

std::atomic<uint16_t> PeripheralsRunningOutOfTimeCounter{0};

std::atomic<main_power_state> SharedMainPowerState{main_power_state::UNDEFINED};
