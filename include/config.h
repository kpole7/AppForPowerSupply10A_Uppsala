/// @file config.h

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include <cinttypes> // for fixed-width integer types

//=================================================================================================
// Preprocessor directives
//=================================================================================================

static_assert(sizeof(int) == 4, "Expected int to be 4 bytes.");

#define CONFIGURATION_FILE_NAME "KwadrupoleWLiniiPionowej.cfg"

#define PSU_NUMBER_OF_CHANNELS 2

#define PERIPHERAL_THREAD_LOOP_DURATION 10 // milliseconds

#define ABSOLUTE_MINIMUM_SETTING (-1000) // in hundredths of an ampere
#define ABSOLUTE_MAXIMUM_SETTING 1000    // in hundredths of an ampere

// TODO review
// Attention: compare with getErrorDescription()
enum class initialization_failure_codes : uint8_t {
	NO_FAILURE,
	ERROR_COMMAND_LINE_SYNTAX,
	ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH,
	ERROR_SETTINGS_UNABLE_TO_OPEN_FILE,
	ERROR_SETTINGS_PORT_NAME_NOT_FOUND,
	ERROR_SETTINGS_REDUNDANT_PORT_NAME,
	ERROR_DEVICE_TIME_STAMP_MISMATCH,
	ANOTHER_ERROR,
};

enum class usart_low_level_error : uint8_t {
	NoError,
	UsartDeviceNotFound, // port manipulation error
	OpenFailed,          // port manipulation error
	SelectFailed,        // port manipulation error
	ReadFailed,          // port manipulation error
	WriteFailed,         // port manipulation error
	InternalError,       // internal error within the usart_master module
	CallError            // internal error within this module
};

#endif // SOURCE_CONFIG_H_
