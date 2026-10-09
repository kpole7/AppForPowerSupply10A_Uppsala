/// @file gui_widgets.h

#ifndef SOURCE_GUI_WIDGETS_H_
#define SOURCE_GUI_WIDGETS_H_

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H> // to eliminate flickering
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <atomic>
#include <cstdint>
#include <string>

#include "config.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define MAIN_WINDOW_WIDTH 370
#define MAIN_WINDOW_HEIGHT 700
#define MAIN_MENU_HEIGHT 22

#define COLOR_BACKGROUND 0x35

//.................................................................................................
// Widgets
//.................................................................................................

enum class IndicatorLightState : uint8_t {
	OFF = 0,
    DIM = 1,
	ON = 2
};

/// Indicator lamp: a colored circle with a dark gray outline
class PowerIndicator : public Fl_Widget {
public:
	PowerIndicator(int X, int Y, int W, int H, const char *Label = nullptr);

	/// Sets the brightness of the indicator: OFF (dark grayish green), DIM (dim green), or ON (bright green)
	void setBrightness(IndicatorLightState NewState);

protected:
	void draw() override;

private:
	IndicatorLightState State;
};

//.................................................................................................
// Function prototypes
//.................................................................................................

void initializeGraphicWidgets();

void showFailureMessageWidget(FailureCodes FailureCodeForGui);

#endif // SOURCE_GUI_WIDGETS_H_
