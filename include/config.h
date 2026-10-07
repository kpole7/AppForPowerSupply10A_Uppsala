/// @file config.h

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include <string>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define CONFIGURATION_FILE_NAME "KwadrupoleWLiniiPionowej.cfg"


// Attention: compare with getErrorDescription()
enum class FailureCodes {
	NO_FAILURE,
	ERROR_COMMAND_LINE_SYNTAX,
	ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH,
	ERROR_SETTINGS_UNABLE_TO_OPEN_FILE,
	ERROR_SETTINGS_PORT_NAME_NOT_FOUND,
	ERROR_SETTINGS_REDUNDANT_PORT_NAME,
	ERROR_SETTINGS_CUP_INSERTING_TIMEOUTS_NOT_FOUND,
	ERROR_SETTINGS_CUP_WITHDRAWING_TIMEOUTS_NOT_FOUND,
	ERROR_SETTINGS_CALIBRATION_CURRENTS_NOT_FOUND,
	ERROR_SETTINGS_CALIBRATION_ADC_READINGS_NOT_FOUND,
	ERROR_SETTINGS_REDUNDANT_CUP_NAME,
	ERROR_SETTINGS_REDUNDANT_PARAMETER_DEFINITION,
	ERROR_SETTINGS_REDUNDANT_CURRENT_DEFINITION,
	ERROR_SETTINGS_REDUNDANT_ADC_READING,
	ERROR_SETTINGS_CONVERTION_TO_NUMBER,
	ERROR_SETTINGS_VALUE_OUT_OF_RANGE,
	ERROR_SETTINGS_TOO_HIGH_CURRENT_VALUE,
	ERROR_SETTINGS_INCORRECT_CUP_OR_CHANNEL_INDEX,
	ERROR_MODBUS_INITIALIZATION_1,
	ERROR_MODBUS_INITIALIZATION_2,
	ERROR_MODBUS_OPENING,
	ERROR_MODBUS_READING,
	ERROR_MODBUS_WRITING,
	ERROR_MODBUS_FRAME_READ,
	ERROR_DEVICE_NAME_MISMATCH,
	ERROR_DEVICE_TIME_STAMP_MISMATCH,
	ANOTHER_ERROR,
};


//.................................................................................................
// Global variables
//.................................................................................................

extern bool VerboseMode;

extern int StatusLevelForGui;

extern std::string ThisApplicationDirectory;

extern std::string ConfigurationFilePath;

#endif // SOURCE_CONFIG_H_
