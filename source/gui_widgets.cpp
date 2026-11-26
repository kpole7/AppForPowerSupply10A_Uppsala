/// @file gui_widgets.cpp

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H> // to eliminate flickering
#include <FL/Fl_Group.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include "gui_widgets.h"
#include "multithread_resources.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define COMMUNICATION_QUALITY_THRESHOLD 60

#define SEPARATOR_COLOR 0x2A

//=================================================================================================
// Widgets
//=================================================================================================

enum class indicator_light_state : uint8_t { OFF = 0, DIM = 1, ON = 2, UNDEFINED = 3 };

/// Indicator lamp: a colored circle with a dark gray outline
class power_indicator : public Fl_Widget {
  private:
	indicator_light_state State{indicator_light_state::UNDEFINED};
	bool Active{true};

  protected:
	void draw() override;

  public:
	power_indicator(int X, int Y, int W, int H, const char *Label = nullptr);

	indicator_light_state getState() const { return State; }

	/// Sets the color of the indicator: OFF (green), DIM (dim orange), or ON (bright red)
	void setState(indicator_light_state NewState);
	void activate();
	void deactivate();
};

class power_contactor_group : public Fl_Group {
  public:
	power_contactor_group(int X, int Y, int W, int H);
	void activate();
	void deactivate();

  private:
	Fl_Box *SeparatorPtr;
	Fl_Box *TitlePtr;
	power_indicator *IndicatorPtr;
	Fl_Box *StateDescriptionPtr;
	Fl_Button *ControlButtonPtr;

	void setIndicatorState(indicator_light_state NewState);
	void setStateDescriptionText(const char *Description);
	void setButtonActivity(bool Active);
	void setButtonLabel(const char *Label);

	friend void refreshGui(void *Data);
	friend void refreshGuiNormal();
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

	void activateButtons();
	void deactivateButtons();
	void setTitle(const char *Title);

  private:
	uint8_t SetpointNumber;
	Fl_Box *SeparatorPtr;
	Fl_Box *ValueDisplayPtr;
	Fl_Box *TitlePtr;
	Fl_Button *Button_Plus1Ptr;
	Fl_Button *Button_Plus01Ptr;
	Fl_Button *Button_Minus01Ptr;
	Fl_Button *Button_Minus1Ptr;

	static void buttonPlusOneCallback(Fl_Widget *Widget, void *Data);
	static void buttonPlusTenthCallback(Fl_Widget *Widget, void *Data);
	static void buttonMinusTenthCallback(Fl_Widget *Widget, void *Data);
	static void buttonMinusOneCallback(Fl_Widget *Widget, void *Data);
	static void exportNewSetpoint(uint8_t SetpointIndex, int16_t Growth);
	void updateDisplay();

	friend void refreshGui(void *Data);
	friend void refreshGuiNormal();
};

class status_bar_group_on_error : public Fl_Group {
  public:
	status_bar_group_on_error(int X, int Y, int W, int H);

  private:
	Fl_Box *FramePtr;
	Fl_Box *ErrorMessagePtr;

	friend void refreshGui(void *Data);
	friend void refreshGuiNormal();
};

class status_bar_group_normal : public Fl_Group {
  public:
	status_bar_group_normal(int X, int Y, int W, int H);

  private:
	Fl_Box *SeparatorPtr;
	Fl_Box *NormalMessagePtr;

	friend void refreshGui(void *Data);
	friend void refreshGuiNormal();
};

//=================================================================================================
// Definitions of types
//=================================================================================================

//=================================================================================================
// Local constants
//=================================================================================================

static const char *UsartFailureTextsPtr[] = {
    "NoError",
    "UsartDeviceNotFound", // port manipulation error
    "OpenFailed",          // port manipulation error
    "SelectFailed",        // port manipulation error
    "ReadFailed",          // port manipulation error
    "WriteFailed",         // port manipulation error
    "InternalError",       // internal error within the usart_master module
    "CallError"            // internal error within this module
};

#define USART_FAILURE_TEXTS_COUNT (sizeof(UsartFailureTextsPtr) / sizeof(UsartFailureTextsPtr[0]))

static const char *InitializationFailureTextsPtr[] = {"No failure",
                                                      "Command line syntax error",
                                                      "Unable to obtain settings path",
                                                      "Unable to open settings file",
                                                      "Port name not found in settings",
                                                      "Redundant port name in settings",
                                                      "Device time stamp mismatch",
                                                      "Another error"};

#define INITIALIZATION_FAILURE_TEXTS_COUNT                                                                             \
	(sizeof(InitializationFailureTextsPtr) / sizeof(InitializationFailureTextsPtr[0]))

//=================================================================================================
// Local variables
//=================================================================================================

static Fl_Box *FailureMessagePtr;

static power_contactor_group *PowerContactorGroupPtr;
static setpoint_adjustment_group *SetpointGroup1Ptr;
static setpoint_adjustment_group *SetpointGroup2Ptr;
static status_bar_group_on_error *StatusBarErrorPtr;
static status_bar_group_normal *StatusBarNormalPtr;

static Fl_Box *ServiceInformationPtr;

//=================================================================================================
// Local function prototypes
//=================================================================================================

static void powerButtonCallback(Fl_Widget *Widget, void *Data);
void refreshGuiNormal();

//=================================================================================================
// Function definitions
//=================================================================================================

//.................................................................................................
// The implementation of the power_contactor_group class
//.................................................................................................

power_contactor_group::power_contactor_group(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
	SeparatorPtr = new Fl_Box(X, Y, W, 1);
	SeparatorPtr->box(FL_FLAT_BOX);
	SeparatorPtr->color(SEPARATOR_COLOR);

	TitlePtr = new Fl_Box(X, Y + 1, W, 20, "Stycznik mocy");
	TitlePtr->labelfont(FL_HELVETICA);
	TitlePtr->labelsize(16);
	TitlePtr->labelcolor(FL_BLACK);
	TitlePtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	IndicatorPtr = new power_indicator(X + 60, Y + H / 3, 46, 46);

	ControlButtonPtr = new Fl_Button(X + 180, Y + H / 3 + H / 29, 90, 34, "");
	ControlButtonPtr->callback(powerButtonCallback);

	StateDescriptionPtr = new Fl_Box(X, Y + 2 * (H / 3) + H / 15, (W * 9) / 20, 20, "");
	StateDescriptionPtr->labelfont(FL_HELVETICA);
	StateDescriptionPtr->labelsize(12);
	StateDescriptionPtr->labelcolor(FL_BLACK);
	StateDescriptionPtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	ServiceInformationPtr = new Fl_Box(X + (3 * W) / 4, Y + 22, W / 4 - 1, H - 26, "");
	ServiceInformationPtr->hide();
#if 1 // show service info box for debugging purposes
	ServiceInformationPtr->box(FL_FLAT_BOX);
	ServiceInformationPtr->color(FL_GRAY);
#endif
	ServiceInformationPtr->labelfont(FL_COURIER);
	ServiceInformationPtr->labelsize(9);
	ServiceInformationPtr->labelcolor(FL_BLACK);
	ServiceInformationPtr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	end();
}

void power_contactor_group::activate() {
	Fl_Group::activate();
	IndicatorPtr->activate();
}

void power_contactor_group::deactivate() {
	Fl_Group::deactivate();
	IndicatorPtr->deactivate();
}

void power_contactor_group::setIndicatorState(indicator_light_state NewState) {
	if (IndicatorPtr->getState() != NewState) {
		IndicatorPtr->setState(NewState);
		IndicatorPtr->redraw();
	}
}

void power_contactor_group::setStateDescriptionText(const char *Description) {
	StateDescriptionPtr->label(Description);
}

void power_contactor_group::setButtonActivity(bool Active) {
	if (Active) {
		ControlButtonPtr->activate();
		return;
	}
	ControlButtonPtr->deactivate();
}

void power_contactor_group::setButtonLabel(const char *Label) { ControlButtonPtr->label(Label); }

/// @brief Callback function for the power button. Toggles between CommandToBeSentPowerOn and CommandToBeSentPowerOff.
static void powerButtonCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	(void)Data;

	// Get current power state
	main_power_state CurrentState = atomic_load_explicit(&ReceivedMainPowerState, std::memory_order_acquire);
#if 0 // signal power button press for debugging purposes
	std::cout << "--------------Power button pressed, current state: " << static_cast<int>(CurrentState) << "\n";
#endif
	// Toggle power command based on current state
	if (CurrentState == main_power_state::OFF) {
		atomic_store_explicit(&CommandToBeSentPowerOn, true, std::memory_order_release);
	}
	else if (CurrentState == main_power_state::ON) {
		atomic_store_explicit(&CommandToBeSentPowerOff, true, std::memory_order_release);
	}
	else {
		// Do nothing
	}
}

//.................................................................................................
// setpoint_adjustment_group implementation
//.................................................................................................

setpoint_adjustment_group::setpoint_adjustment_group(int X, int Y, int W, int H, uint8_t SetpointNumber)
    : Fl_Group(X, Y, W, H), SetpointNumber(SetpointNumber) {
	const int ValueDisplayWidth = 110;
	const int AmpereLabelWidth = 25;
	const int ButtonWidth = 50;
	const int ButtonHeight = 20;
	const int ButtonSpacing = 2;
	const int TotalButtonHeight = (ButtonHeight + ButtonSpacing) * 4;
	const int VerticalCenterPadding = (H - TotalButtonHeight) / 2;
	const int TitleHeight = 20;
	const int TitleSpacing = 5;
	const int TitleSpace = 16;

	// Separator line at the top of the group
	SeparatorPtr = new Fl_Box(X, Y, W, 1);
	SeparatorPtr->box(FL_FLAT_BOX);
	SeparatorPtr->color(SEPARATOR_COLOR);

	// Title at the top of the group
	TitlePtr = new Fl_Box(X, Y + TitleSpacing, W, TitleHeight);
	TitlePtr->box(FL_FLAT_BOX);
	TitlePtr->labelfont(FL_HELVETICA);
	TitlePtr->labelsize(16);
	TitlePtr->labelcolor(FL_BLACK);
	TitlePtr->color(COLOR_BACKGROUND);
	TitlePtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	// Left side: value display box
	ValueDisplayPtr = new Fl_Box(X + 40, Y + (H - 40) / 2 + TitleSpace - 20, ValueDisplayWidth, 40);
	ValueDisplayPtr->box(FL_DOWN_BOX);
	ValueDisplayPtr->labelfont(FL_COURIER_BOLD);
	ValueDisplayPtr->labelsize(26);
	ValueDisplayPtr->labelcolor(FL_BLACK);
	ValueDisplayPtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	// Label "A" (Amperes) next to the value
	Fl_Box *UnitLabel =
	    new Fl_Box(X + 40 + ValueDisplayWidth + 5, Y + (H - 20) / 2 + TitleSpace - 20, AmpereLabelWidth, 20, "A");
	UnitLabel->labelfont(FL_COURIER);
	UnitLabel->labelsize(26);
	UnitLabel->labelcolor(FL_BLACK);
	UnitLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	// Center: buttons arranged vertically
	int ButtonGroupStartX = X + 40 + ValueDisplayWidth + AmpereLabelWidth + 70;
	int ButtonGroupStartY = Y + VerticalCenterPadding + TitleSpace;

	Button_Plus1Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY, ButtonWidth, ButtonHeight, "+1");
	Button_Plus1Ptr->labelfont(FL_COURIER);
	Button_Plus1Ptr->labelsize(12);
	Button_Plus1Ptr->callback(buttonPlusOneCallback, this);

	Button_Plus01Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + ButtonHeight + ButtonSpacing, ButtonWidth,
	                                 ButtonHeight, "+0.1");
	Button_Plus01Ptr->labelfont(FL_COURIER);
	Button_Plus01Ptr->labelsize(12);
	Button_Plus01Ptr->callback(buttonPlusTenthCallback, this);

	Button_Minus01Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + 2 * (ButtonHeight + ButtonSpacing),
	                                  ButtonWidth, ButtonHeight, "-0.1");
	Button_Minus01Ptr->labelfont(FL_COURIER);
	Button_Minus01Ptr->labelsize(12);
	Button_Minus01Ptr->callback(buttonMinusTenthCallback, this);

	Button_Minus1Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + 3 * (ButtonHeight + ButtonSpacing),
	                                 ButtonWidth, ButtonHeight, "-1");
	Button_Minus1Ptr->labelfont(FL_COURIER);
	Button_Minus1Ptr->labelsize(12);
	Button_Minus1Ptr->callback(buttonMinusOneCallback, this);

	end();
	updateDisplay();
}

void setpoint_adjustment_group::activateButtons() {
	Button_Plus1Ptr->activate();
	Button_Plus01Ptr->activate();
	Button_Minus01Ptr->activate();
	Button_Minus1Ptr->activate();
}

void setpoint_adjustment_group::deactivateButtons() {
	Button_Plus1Ptr->deactivate();
	Button_Plus01Ptr->deactivate();
	Button_Minus01Ptr->deactivate();
	Button_Minus1Ptr->deactivate();
}

void setpoint_adjustment_group::setTitle(const char *Title) {
	TitlePtr->label(Title);
	TitlePtr->redraw();
}

void setpoint_adjustment_group::updateDisplay() {
	static char Buffer[16];

	int16_t CurrentValue;
	if (SetpointNumber == 1) {
		CurrentValue = atomic_load_explicit(&ReceivedSetpoint1, std::memory_order_acquire);
	}
	else {
		CurrentValue = atomic_load_explicit(&ReceivedSetpoint2, std::memory_order_acquire);
	}

	double DisplayValue = static_cast<double>(CurrentValue) / 100.0;
	snprintf(Buffer, sizeof(Buffer), "%.2f", DisplayValue);
	ValueDisplayPtr->label(strdup(Buffer));
	ValueDisplayPtr->redraw();
}

/// @brief Exports a new setpoint value for the specified setpoint index, taking into account the growth value and
/// handling both multiclick and single click scenarios.
/// @param SetpointIndex The index of the setpoint to be adjusted (1 or 2)
/// @param Growth The amount by which to adjust the setpoint (positive or negative)
void setpoint_adjustment_group::exportNewSetpoint(uint8_t SetpointIndex, int16_t Growth) {
	int16_t NewValue;
	int16_t NewSetpointValue;
	if (SetpointIndex == 1) {
		NewValue = atomic_load_explicit(&ReceivedSetpoint1, std::memory_order_acquire) + Growth;
		NewSetpointValue = atomic_load_explicit(&ValueToBeSentSetpoint1, std::memory_order_acquire) + Growth;
		bool IsBeingAdjusted = !atomic_load_explicit(&IsFreshReceivedSetpoint1, std::memory_order_acquire);
		if (IsBeingAdjusted) {
			// multiclick case
			if (NewSetpointValue > ABSOLUTE_MAXIMUM_SETTING) {
				NewSetpointValue = ABSOLUTE_MAXIMUM_SETTING;
			}
			if (NewSetpointValue < ABSOLUTE_MINIMUM_SETTING) {
				NewSetpointValue = ABSOLUTE_MINIMUM_SETTING;
			}
			atomic_store_explicit(&ValueToBeSentSetpoint1, NewSetpointValue, std::memory_order_release);
			atomic_store_explicit(&CommandToBeSentSetpoint1, true, std::memory_order_release);
			atomic_store_explicit(&IsFreshReceivedSetpoint1, false, std::memory_order_release);
		}
		else {
			// the case of a single click or a slow click
			if (NewValue > ABSOLUTE_MAXIMUM_SETTING) {
				NewValue = ABSOLUTE_MAXIMUM_SETTING;
			}
			if (NewValue < ABSOLUTE_MINIMUM_SETTING) {
				NewValue = ABSOLUTE_MINIMUM_SETTING;
			}
			atomic_store_explicit(&ValueToBeSentSetpoint1, NewValue, std::memory_order_release);
			atomic_store_explicit(&CommandToBeSentSetpoint1, true, std::memory_order_release);
			atomic_store_explicit(&IsFreshReceivedSetpoint1, false, std::memory_order_release);
		}
	}
	else {
		NewValue = atomic_load_explicit(&ReceivedSetpoint2, std::memory_order_acquire) + Growth;
		NewSetpointValue = atomic_load_explicit(&ValueToBeSentSetpoint2, std::memory_order_acquire) + Growth;
		bool IsBeingAdjusted = !atomic_load_explicit(&IsFreshReceivedSetpoint2, std::memory_order_acquire);
		if (IsBeingAdjusted) {
			// multiclick case
			if (NewSetpointValue > ABSOLUTE_MAXIMUM_SETTING) {
				NewSetpointValue = ABSOLUTE_MAXIMUM_SETTING;
			}
			if (NewSetpointValue < ABSOLUTE_MINIMUM_SETTING) {
				NewSetpointValue = ABSOLUTE_MINIMUM_SETTING;
			}
			atomic_store_explicit(&ValueToBeSentSetpoint2, NewSetpointValue, std::memory_order_release);
			atomic_store_explicit(&CommandToBeSentSetpoint2, true, std::memory_order_release);
			atomic_store_explicit(&IsFreshReceivedSetpoint2, false, std::memory_order_release);
		}
		else {
			// the case of a single click or a slow click
			if (NewValue > ABSOLUTE_MAXIMUM_SETTING) {
				NewValue = ABSOLUTE_MAXIMUM_SETTING;
			}
			if (NewValue < ABSOLUTE_MINIMUM_SETTING) {
				NewValue = ABSOLUTE_MINIMUM_SETTING;
			}
			atomic_store_explicit(&ValueToBeSentSetpoint2, NewValue, std::memory_order_release);
			atomic_store_explicit(&CommandToBeSentSetpoint2, true, std::memory_order_release);
			atomic_store_explicit(&IsFreshReceivedSetpoint2, false, std::memory_order_release);
		}
	}
}

void setpoint_adjustment_group::buttonPlusOneCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	setpoint_adjustment_group *pThis = static_cast<setpoint_adjustment_group *>(Data);

	exportNewSetpoint(pThis->SetpointNumber, 100);
}

void setpoint_adjustment_group::buttonPlusTenthCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	setpoint_adjustment_group *pThis = static_cast<setpoint_adjustment_group *>(Data);

	exportNewSetpoint(pThis->SetpointNumber, 10);
}

void setpoint_adjustment_group::buttonMinusTenthCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	setpoint_adjustment_group *pThis = static_cast<setpoint_adjustment_group *>(Data);

	exportNewSetpoint(pThis->SetpointNumber, -10);
}

void setpoint_adjustment_group::buttonMinusOneCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	setpoint_adjustment_group *pThis = static_cast<setpoint_adjustment_group *>(Data);

	exportNewSetpoint(pThis->SetpointNumber, -100);
}

//.................................................................................................

power_indicator::power_indicator(int X, int Y, int W, int H, const char *Label) : Fl_Widget(X, Y, W, H, Label) {}

void power_indicator::draw() {
	const Fl_Color ColorOnActive = fl_rgb_color(0xFF, 0x5F, 0x5B);
	const Fl_Color ColorDimActive = fl_rgb_color(0xE3, 0xD3, 0x7A);
	const Fl_Color ColorOffActive = fl_rgb_color(0x00, 0x91, 0x00);
	const Fl_Color ColorRimActive = fl_rgb_color(0x5C, 0x5C, 0x5C);

	const Fl_Color ColorOnInactive = fl_rgb_color(0xE3, 0x99, 0x90);
	const Fl_Color ColorDimInactive = fl_rgb_color(0xCE, 0xC5, 0xA0);
	const Fl_Color ColorOffInactive = fl_rgb_color(0x8C, 0xB3, 0x81);
	const Fl_Color ColorRimInactive = fl_rgb_color(0xAC, 0xAC, 0xAC);

	const Fl_Color ColorUndefined = fl_rgb_color(0xD0, 0xD8, 0xD0);
	const Fl_Color ColorRimUndefined = fl_rgb_color(0xCC, 0xCC, 0xCC);

	int Diameter = (w() < h()) ? w() : h();
	int PosX = x() + (w() - Diameter) / 2;
	int PosY = y() + (h() - Diameter) / 2;

	if (State == indicator_light_state::UNDEFINED) {
		fl_color(ColorRimUndefined);
	}
	else {
		fl_color(Active ? ColorRimActive : ColorRimInactive);
	}
	fl_pie(PosX, PosY, Diameter, Diameter, 0.0, 360.0);

	int Rim = (Diameter >= 12) ? 4 : 1;
	int Inner = Diameter - 2 * Rim;
	if (Inner > 0) {
		if (indicator_light_state::ON == State) {
			fl_color(Active ? ColorOnActive : ColorOnInactive);
		}
		else if (indicator_light_state::DIM == State) {
			fl_color(Active ? ColorDimActive : ColorDimInactive);
		}
		else if (indicator_light_state::UNDEFINED == State) {
			fl_color(ColorUndefined);
		}
		else {
			fl_color(Active ? ColorOffActive : ColorOffInactive);
		}

		fl_pie(PosX + Rim, PosY + Rim, Inner, Inner, 0.0, 360.0);
	}
}

void power_indicator::setState(indicator_light_state NewState) {
	State = NewState;
	redraw();
}

void power_indicator::activate() {
	Active = true;
	redraw();
}

void power_indicator::deactivate() {
	Active = false;
	redraw();
}

//.............................................................................................
status_bar_group_on_error::status_bar_group_on_error(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
	FramePtr = new Fl_Box(X, Y, W, H);
	FramePtr->box(FL_FLAT_BOX);
	FramePtr->color(FL_RED);

	int FrameThickness = 5;
	ErrorMessagePtr =
	    new Fl_Box(X + FrameThickness, Y + FrameThickness, W - 2 * FrameThickness, H - 2 * FrameThickness);
	ErrorMessagePtr->box(FL_FLAT_BOX);
	ErrorMessagePtr->color(FL_YELLOW);
	ErrorMessagePtr->labelcolor(FL_BLACK);
	ErrorMessagePtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
	ErrorMessagePtr->labelsize(16);
	ErrorMessagePtr->labelfont(FL_HELVETICA_BOLD);

	ErrorMessagePtr->label("Brak transmisji");
	end();
}

status_bar_group_normal::status_bar_group_normal(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
	SeparatorPtr = new Fl_Box(X, Y, W, 1);
	SeparatorPtr->box(FL_FLAT_BOX);
	SeparatorPtr->color(SEPARATOR_COLOR);

	int Space = 5;
	NormalMessagePtr = new Fl_Box(X + Space, Y + Space, W - 2 * Space, H - 2 * Space);
	NormalMessagePtr->box(FL_FLAT_BOX);
	NormalMessagePtr->color(COLOR_BACKGROUND);
	NormalMessagePtr->labelcolor(FL_BLACK);
	NormalMessagePtr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	NormalMessagePtr->labelsize(12);
	NormalMessagePtr->labelfont(FL_HELVETICA);

	NormalMessagePtr->label("Status: OK");
	end();
}

//.............................................................................................

void initializeGraphicWidgets() {
	FailureMessagePtr = new Fl_Box(0, MAIN_MENU_HEIGHT, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, "");
	FailureMessagePtr->hide();

	// the percentage ratio of the height of the first window to that of the second
	int FirstToSecondHeightPercentageRatio = 80; // %
	int UsableHeight = MAIN_WINDOW_HEIGHT - MAIN_MENU_HEIGHT - STATUS_BAR_HEIGHT;
	int SecondHeight = (100 * UsableHeight) / (FirstToSecondHeightPercentageRatio + 200);
	int FirstHeight = UsableHeight - SecondHeight - SecondHeight;
#if 0
	std::cout << "Heights: " << FirstHeight << ", " << SecondHeight << ", " << SecondHeight << '\n';
#endif
	PowerContactorGroupPtr = new power_contactor_group(0, MAIN_MENU_HEIGHT, MAIN_WINDOW_WIDTH, FirstHeight);

	SetpointGroup1Ptr =
	    new setpoint_adjustment_group(0, MAIN_MENU_HEIGHT + FirstHeight, MAIN_WINDOW_WIDTH, SecondHeight, 1);
	SetpointGroup1Ptr->setTitle("Prąd zadany w kwadrupolu górnym");

	SetpointGroup2Ptr = new setpoint_adjustment_group(0, MAIN_MENU_HEIGHT + FirstHeight + SecondHeight,
	                                                  MAIN_WINDOW_WIDTH, SecondHeight, 2);
	SetpointGroup2Ptr->setTitle("Prąd zadany w kwadrupolu dolnym");

	StatusBarErrorPtr = new status_bar_group_on_error(0, MAIN_MENU_HEIGHT + FirstHeight + 2 * SecondHeight,
	                                                  MAIN_WINDOW_WIDTH, STATUS_BAR_HEIGHT);
	StatusBarErrorPtr->hide();

	StatusBarNormalPtr = new status_bar_group_normal(0, MAIN_MENU_HEIGHT + FirstHeight + 2 * SecondHeight,
	                                                 MAIN_WINDOW_WIDTH, STATUS_BAR_HEIGHT);
	StatusBarNormalPtr->show();
}

void showFailureMessageWidget(initialization_failure_codes FailureCodeForGui) {
	static char Buffer[300];
	if (static_cast<uint8_t>(FailureCodeForGui) >= INITIALIZATION_FAILURE_TEXTS_COUNT) {
		FailureCodeForGui = initialization_failure_codes::ANOTHER_ERROR;
	}
	snprintf(Buffer, sizeof(Buffer) - 1,
	         "Błędy podczas startu aplikacji\n%s\n\nUruchom aplikację z parametrem -v w konsoli,\nżeby uzyskać "
	         "dodatkowe informacje",
	         InitializationFailureTextsPtr[static_cast<uint8_t>(FailureCodeForGui)]);
	FailureMessagePtr->label(Buffer);
	FailureMessagePtr->show();
	if (atomic_load_explicit(&VerboseMode, std::memory_order_acquire)) {
		std::cout << "--------------------------------------GUI FAILURE MESSAGE--------------------------------------"
		          << '\n';
		std::cout << Buffer << '\n';
		std::cout << "-----------------------------------------------------------------------------------------------"
		          << '\n';
	}
}

//=================================================================================================
// Definitions of functions related to peripheral thread
//=================================================================================================

void refreshGuiNormal() {
	if (!PowerContactorGroupPtr->active()) {
		PowerContactorGroupPtr->activate();
	}
	if (!SetpointGroup1Ptr->active()) {
		SetpointGroup1Ptr->activate();
	}
	if (!SetpointGroup2Ptr->active()) {
		SetpointGroup2Ptr->activate();
	}

	if (StatusBarErrorPtr->visible()) {
		StatusBarErrorPtr->hide();
	}
	if (!StatusBarNormalPtr->visible()) {
		StatusBarNormalPtr->show();
	}

	indicator_light_state IndicatorCurrentState;
	switch (atomic_load_explicit(&ReceivedMainPowerState, std::memory_order_acquire)) {
	case main_power_state::UNDEFINED:
		IndicatorCurrentState = indicator_light_state::UNDEFINED;
		PowerContactorGroupPtr->setButtonActivity(true);
		PowerContactorGroupPtr->setButtonLabel("?");
		PowerContactorGroupPtr->setStateDescriptionText("Stycznik nieokreślony");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	case main_power_state::ON:
		IndicatorCurrentState = indicator_light_state::ON;
		PowerContactorGroupPtr->setButtonActivity(true);
		PowerContactorGroupPtr->setButtonLabel("Wyłącz");
		PowerContactorGroupPtr->setStateDescriptionText("Stycznik włączony");
		SetpointGroup1Ptr->activateButtons();
		SetpointGroup2Ptr->activateButtons();
		break;
	case main_power_state::OFF:
		IndicatorCurrentState = indicator_light_state::OFF;
		PowerContactorGroupPtr->setButtonActivity(true);
		PowerContactorGroupPtr->setButtonLabel("Włącz");
		PowerContactorGroupPtr->setStateDescriptionText("Stycznik wyłączony");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	case main_power_state::RISING:
	case main_power_state::FALLING:
		IndicatorCurrentState = indicator_light_state::DIM;
		PowerContactorGroupPtr->setButtonActivity(false);
		PowerContactorGroupPtr->setStateDescriptionText("W trakcie przełączania");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	default:
		std::cout << "Unexpected main power state" << '\n';
		PowerContactorGroupPtr->setButtonActivity(true);
		PowerContactorGroupPtr->setStateDescriptionText("Nieznany stan stycznika");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	}
	PowerContactorGroupPtr->setIndicatorState(IndicatorCurrentState);

	// Update setpoint displays
	SetpointGroup1Ptr->updateDisplay();
	SetpointGroup2Ptr->updateDisplay();

	PowerContactorGroupPtr->redraw();
	SetpointGroup1Ptr->redraw();
	SetpointGroup2Ptr->redraw();
	StatusBarNormalPtr->redraw();
}

void refreshGuiOnCriticalError() {
	if (PowerContactorGroupPtr->visible()) {
		PowerContactorGroupPtr->hide();
	}
	if (SetpointGroup1Ptr->visible()) {
		SetpointGroup1Ptr->hide();
	}
	if (SetpointGroup2Ptr->visible()) {
		SetpointGroup2Ptr->hide();
	}
	if (StatusBarErrorPtr->visible()) {
		StatusBarErrorPtr->hide();
	}
	if (StatusBarNormalPtr->visible()) {
		StatusBarNormalPtr->hide();
	}
	if (!FailureMessagePtr->visible()) {
		FailureMessagePtr->show();
	}

	usart_low_level_error CurrentUsartError =
		atomic_load_explicit(&ReceivedUsartErrorCode, std::memory_order_acquire);

	std::string ErrorMessage = "Błąd krytyczny\n";
	if (static_cast<size_t>(CurrentUsartError) < USART_FAILURE_TEXTS_COUNT) {
		ErrorMessage += UsartFailureTextsPtr[static_cast<size_t>(CurrentUsartError)];
	}
	else {
		ErrorMessage += "Nieznany błąd USART";
	}
	FailureMessagePtr->label(ErrorMessage.c_str());
	FailureMessagePtr->redraw();
}

void refreshGuiOnCommunicationError() {
	if (PowerContactorGroupPtr->active()) {
		PowerContactorGroupPtr->deactivate();
	}
	if (SetpointGroup1Ptr->active()) {
		SetpointGroup1Ptr->deactivate();
	}
	if (SetpointGroup2Ptr->active()) {
		SetpointGroup2Ptr->deactivate();
	}

	if (!StatusBarErrorPtr->visible()) {
		StatusBarErrorPtr->show();
	}
	if (StatusBarNormalPtr->visible()) {
		StatusBarNormalPtr->hide();
	}
}

void refreshGui(void *Data) {
	(void)Data; // intentionally unused

	assert(FailureMessagePtr != nullptr);
	assert(PowerContactorGroupPtr != nullptr);
	assert(SetpointGroup2Ptr != nullptr);
	assert(SetpointGroup1Ptr != nullptr);
	assert(ServiceInformationPtr != nullptr);

	uint8_t CommunicationQualityPercentage =
	    atomic_load_explicit(&ReceivedCommunicationQuality, std::memory_order_acquire);
	uint8_t CommunicationQualityTotalPercentage =
	    atomic_load_explicit(&ReceivedTotalCommunicationQuality, std::memory_order_acquire);
	if (0 < StatusLevelForGui.load(std::memory_order_acquire)) {
		static std::string ServiceInformationString;
		ServiceInformationString = "Transmisja: " + std::to_string(CommunicationQualityPercentage) + "%\n(" +
		                           std::to_string(CommunicationQualityTotalPercentage) + "% ogółem)";
		ServiceInformationPtr->label(ServiceInformationString.c_str());
		ServiceInformationPtr->show();
	}
	else {
		ServiceInformationPtr->hide();
	}
	ServiceInformationPtr->redraw();

	// Critical errors check
	if (atomic_load_explicit(&ReceivedUsartErrorCode, std::memory_order_acquire) != usart_low_level_error::NoError) {
		refreshGuiOnCriticalError();
		return;
	}

	// Reset the failure message visibility if no critical errors are present

	if (FailureMessagePtr->visible()) {
		FailureMessagePtr->hide();
	}
	if (!PowerContactorGroupPtr->visible()) {
		PowerContactorGroupPtr->show();
	}
	if (!SetpointGroup1Ptr->visible()) {
		SetpointGroup1Ptr->show();
	}
	if (!SetpointGroup2Ptr->visible()) {
		SetpointGroup2Ptr->show();
	}

	// Communication error check
	if (CommunicationQualityPercentage < COMMUNICATION_QUALITY_THRESHOLD) {
		refreshGuiOnCommunicationError();
		return;
	}
	refreshGuiNormal();
}
