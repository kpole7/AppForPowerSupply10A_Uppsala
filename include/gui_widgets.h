/// @file gui_widgets.h

#ifndef SOURCE_GUI_WIDGETS_H_
#define SOURCE_GUI_WIDGETS_H_

#include "config.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define MAIN_WINDOW_WIDTH 370
#define MAIN_WINDOW_HEIGHT 470
#define MAIN_MENU_HEIGHT 22
#define STATUS_BAR_HEIGHT 34

#define COLOR_BACKGROUND 0x35

//=================================================================================================
// Function prototypes
//=================================================================================================

void initializeGraphicWidgets();

void showFailureMessageWidget(initialization_failure_codes FailureCodeForGui);

#endif // SOURCE_GUI_WIDGETS_H_
