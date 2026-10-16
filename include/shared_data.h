/// @file shared_data.h

#ifndef SOURCE_SHARED_DATA_H_
#define SOURCE_SHARED_DATA_H_

#include <atomic>
#include <chrono>
#include <cstdint>

//.................................................................................................
// Types
//.................................................................................................

/// @brief Minimal representation of the main power state visible to the user.
enum class main_power_state : uint8_t { OFF = 0, RISING = 2, ON = 1, FALLING = 3, UNDEFINED = 4 };

//.................................................................................................
// Global variables
//.................................................................................................

/// This variable is set if there is argument "-v" in command line
extern std::atomic<bool> VerboseMode;

/// @brief This variable defines amount of information to be displayed in the GUI. It can be used in the FLTK thread only.
extern int StatusLevelForGui;

/// @brief This variable stores the directory where the application executable file is located.
/// @details This variable can be set only once, during the initialization of the application.
extern std::string ThisApplicationDirectory;

/// @brief This variable stores the full path to the configuration file.
/// @details This variable can be set only once, during the initialization of the application.
extern std::string ConfigurationFilePath;

/// @brief Counter for the number of times peripherals have run out of time.
extern std::atomic<uint16_t> PeripheralsRunningOutOfTimeCounter;

/// @brief This variable stores the current state of the main power visible for the user.
extern std::atomic<main_power_state> SharedMainPowerState;


#endif // SOURCE_SHARED_DATA_H_
