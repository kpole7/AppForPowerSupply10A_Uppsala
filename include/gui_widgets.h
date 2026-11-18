/// @file gui_widgets.h

#ifndef SOURCE_GUI_WIDGETS_H_
#define SOURCE_GUI_WIDGETS_H_

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H> // to eliminate flickering
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <cstdint>
#include <string>

#include "config.h"
#include "multithread_resources.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define MAIN_WINDOW_WIDTH 370
#define MAIN_WINDOW_HEIGHT 700
#define MAIN_MENU_HEIGHT 22

#define COLOR_BACKGROUND 0x35

//=================================================================================================
// Widgets
//=================================================================================================

enum class indicator_light_state : uint8_t { OFF = 0, DIM = 1, ON = 2, UNDEFINED = 3 };

/// Indicator lamp: a colored circle with a dark gray outline
class power_indicator : public Fl_Widget {
  public:
	power_indicator(int X, int Y, int W, int H, const char *Label = nullptr);

	/// Sets the brightness of the indicator: OFF (dark grayish green), DIM (dim green), or ON (bright green)
	void setBrightness(indicator_light_state NewState);

  protected:
	void draw() override;

  private:
	indicator_light_state State{indicator_light_state::UNDEFINED};
};

/// Main power controls
class main_power_group : public Fl_Group {
  public:
	main_power_group(int X, int Y, int W, int H, const char *L = nullptr);
	void setState(main_power_state NewState);

  private:
	main_power_state CurrentState{main_power_state::UNDEFINED};
	power_indicator *PowerIndicatorPtr;
	Fl_Button *PowerSwitchPtr;
	Fl_Box *StatusTextBoxPtr;
};

/// Setpoint adjustment widget group: displays setpoint value and provides adjustment buttons
class setpoint_adjustment_group : public Fl_Group {
  public:
	/// Constructor for setpoint adjustment group
	/// @param X X position
	/// @param Y Y position
	/// @param W Width (should span entire main window width)
	/// @param H Height
	/// @param SetpointNumber 1 or 2 to identify which setpoint to control
	setpoint_adjustment_group(int X, int Y, int W, int H, uint8_t SetpointNumber);

  private:
	uint8_t SetpointNumber;
	Fl_Box *ValueDisplayPtr;
	Fl_Button *Button_Plus1Ptr;
	Fl_Button *Button_Plus01Ptr;
	Fl_Button *Button_Minus01Ptr;
	Fl_Button *Button_Minus1Ptr;

	static void buttonPlusOneCallback(Fl_Widget *Widget, void *Data);
	static void buttonPlusTenthCallback(Fl_Widget *Widget, void *Data);
	static void buttonMinusTenthCallback(Fl_Widget *Widget, void *Data);
	static void buttonMinusOneCallback(Fl_Widget *Widget, void *Data);
	void updateDisplay();

	friend void refreshWholeGui(void *Data);
};

//=================================================================================================
// Function prototypes
//=================================================================================================

void initializeGraphicWidgets();

void showFailureMessageWidget(initialization_failure_codes FailureCodeForGui);

#endif // SOURCE_GUI_WIDGETS_H_
