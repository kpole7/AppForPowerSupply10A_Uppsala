/// @file multithread_resources.h
/// @brief Shared data and functions between FLTK thread and the peripheral thread.

#ifndef MULTITHREAD_RESOURCES_H_
#define MULTITHREAD_RESOURCES_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define DEFAULT_STATUS_LEVEL_FOR_GUI 1

//.................................................................................................
// Types
//.................................................................................................

/// @brief Minimal representation of the main power state visible to the user.
enum class main_power_state : uint8_t { OFF = 0, RISING = 1, ON = 2, FALLING = 3, UNDEFINED = 4 };

//.................................................................................................
// Global variables
//.................................................................................................

/// This variable is set if there is argument "-v" in command line
extern std::atomic<bool> VerboseMode;

/// @brief This variable defines amount of information to be displayed in the GUI. It can be used in the FLTK thread only.
extern std::atomic<uint8_t> StatusLevelForGui;

/// @brief Counter for the number of times peripherals have run out of time.
extern std::atomic<uint16_t> PeripheralsRunningOutOfTimeCounter;

/// @brief This variable stores the current state of the main power visible for the user.
/// @details This variable can be read from the FLTK thread and updated from the peripheral thread.
extern std::atomic<main_power_state> SharedMainPowerState;

/// @brief This variable stores the directory where the application executable file is located.
/// @details This variable can be set only once, during the initialization of the application.
extern std::string ThisApplicationDirectory;

/// @brief This variable stores the full path to the configuration file.
/// @details This variable can be set only once, during the initialization of the application.
extern std::string ConfigurationFilePath;

//.................................................................................................
// Prototypes of functions that are called from the FLTK thread, but impact the peripheral thread.
//.................................................................................................

void peripheralThreadStart();

void peripheralThreadExit();

//.................................................................................................
// Prototypes of functions that are to be run in the FLTK thread, but are awakened by the peripheral thread.
//.................................................................................................

void refreshWholeGui(void *Data);

#endif // MULTITHREAD_RESOURCES_H_
