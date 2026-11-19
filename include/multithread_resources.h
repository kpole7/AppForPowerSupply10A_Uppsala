/// @file multithread_resources.h
/// @brief Shared data and functions between FLTK thread and the peripheral thread.

// TODO review + update

#ifndef MULTITHREAD_RESOURCES_H_
#define MULTITHREAD_RESOURCES_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define DEFAULT_STATUS_LEVEL_FOR_GUI 1

//=================================================================================================
// Types
//=================================================================================================

/// @brief Minimal representation of the main power state visible to the user.
enum class main_power_state : uint8_t { OFF = 0, RISING = 1, ON = 2, FALLING = 3, UNDEFINED = 4 };

//=================================================================================================
// Global variables
//=================================================================================================

/// This variable is set if there is argument "-v" in command line
extern std::atomic<bool> VerboseMode;

/// @brief This variable defines amount of information to be displayed in the GUI. It can be used in the FLTK thread
/// only.
extern std::atomic<uint8_t> StatusLevelForGui;

/// @brief This variable stores the directory where the application executable file is located.
/// @details This variable is to be set only once, during the initialization of the application.
extern std::string ThisApplicationDirectory;

/// @brief This variable stores the full path to the configuration file.
/// @details This variable can be set only once, during the initialization of the application.
extern std::string ConfigurationFilePath;

// Information obtained from the “peripheral” thread

/// @brief This variable stores the current state of the main power visible for the user.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<main_power_state> ReceivedMainPowerState;

/// @brief This variable stores the last USART low-level error received from the peripheral thread.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<uint8_t> ReceivedUsartErrorCode;

/// @brief This variable stores the current communication quality received from the peripheral thread.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<uint8_t> ReceivedCommunicationQuality;

/// @brief This variable stores the total communication quality received from the peripheral thread.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<uint8_t> ReceivedTotalCommunicationQuality;

/// @brief This variable stores the current value of setpoint 1 received from the peripheral thread.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<int16_t> ReceivedSetpoint1;

/// @brief This variable indicates whether the received setpoint 1 is fresh
/// @details This variable indicates whether the received setpoint 1 value has been updated since the last change (i.e.,
/// since “true” was written to CommandToBeSentSetpoint1)
extern std::atomic<bool> IsFreshReceivedSetpoint1;

/// @brief This variable stores the current value of setpoint 2 received from the peripheral thread.
/// @details This variable is to be displayed in the FLTK thread and updated from the peripheral thread.
extern std::atomic<int16_t> ReceivedSetpoint2;

/// @brief This variable indicates whether the received setpoint 2 is fresh
/// @details This variable indicates whether the received setpoint 2 value has been updated since the last change (i.e.,
/// since “true” was written to CommandToBeSentSetpoint2)
extern std::atomic<bool> IsFreshReceivedSetpoint2;

// The user demands (information from GUI)

/// @brief This variable indicates whether a power-on command has been issued from the GUI.
extern std::atomic<bool> CommandToBeSentPowerOn;

/// @brief This variable indicates whether a power-off command has been issued from the GUI.
extern std::atomic<bool> CommandToBeSentPowerOff;

/// @brief This variable indicates whether a command to set setpoint 1 was issued from the graphical user interface.
extern std::atomic<bool> CommandToBeSentSetpoint1;

/// @brief This variable stores the value to be sent for setpoint 1 from the GUI.
extern std::atomic<int16_t> ValueToBeSentSetpoint1;

/// @brief This variable indicates whether a command to set setpoint 2 was issued from the graphical user interface.
extern std::atomic<bool> CommandToBeSentSetpoint2;

/// @brief This variable stores the value to be sent for setpoint 2 from the GUI.
extern std::atomic<int16_t> ValueToBeSentSetpoint2;

//=================================================================================================
// Prototypes of functions that are called from the FLTK thread, but impact the peripheral thread.
//=================================================================================================

void peripheralThreadStart();

void peripheralThreadExit();

//=================================================================================================
// Prototypes of functions that are to be run in the FLTK thread, but are awakened by the peripheral thread.
//=================================================================================================

void refreshWholeGui(void *Data);

#endif // MULTITHREAD_RESOURCES_H_
