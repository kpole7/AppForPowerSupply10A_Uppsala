/// @file config.h

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include <string>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define CONFIGURATION_FILE_NAME "KwadrupoleWLiniiPionowej.cfg"

#define PSU_NUMBER 2

// Attention: compare with getErrorDescription()
enum class InitializationFailureCodes {
	NO_FAILURE,
	ERROR_COMMAND_LINE_SYNTAX,
	ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH,
	ERROR_SETTINGS_UNABLE_TO_OPEN_FILE,
	ERROR_SETTINGS_PORT_NAME_NOT_FOUND,
	ERROR_SETTINGS_REDUNDANT_PORT_NAME,
	ERROR_DEVICE_NAME_MISMATCH,
	ERROR_DEVICE_TIME_STAMP_MISMATCH,
	ANOTHER_ERROR,
};

enum class RunTimeFailureCodes {
	NO_FAILURE,
	CONNECTION_LOST,
	WRONG_ANSWERS,
};

//.................................................................................................
// Global variables
//.................................................................................................

extern bool VerboseMode;

extern int StatusLevelForGui;

extern std::string ThisApplicationDirectory;

extern std::string ConfigurationFilePath;

#endif // SOURCE_CONFIG_H_
