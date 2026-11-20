/// @file gui_widgets.cpp

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include "gui_widgets.h"

//=================================================================================================
// Preprocessor directives
//=================================================================================================

#define SEPARATOR_COLOR 0x2A

//=================================================================================================
// Definitions of types
//=================================================================================================

//=================================================================================================
// Local constants
//=================================================================================================

//=================================================================================================
// Local variables
//=================================================================================================

static Fl_Box *FailureMessagePtr;

static Fl_Box *SeparatorPtr;
static power_indicator *PowerIndicatorPtr;
static Fl_Button *PowerButtonPtr;
static Fl_Box *PowerStateDescriptionPtr;

static setpoint_adjustment_group *SetpointGroup1Ptr;
static setpoint_adjustment_group *SetpointGroup2Ptr;

//=================================================================================================
// Local function prototypes
//=================================================================================================

static const char *getErrorDescription(initialization_failure_codes Error);
static void powerButtonCallback(Fl_Widget *Widget, void *Data);

//=================================================================================================
// Function definitions
//=================================================================================================

/// @brief Callback function for the power button. Toggles between CommandToBeSentPowerOn and CommandToBeSentPowerOff.
static void powerButtonCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget;
	(void)Data;

	// Get current power state
	main_power_state CurrentState = atomic_load_explicit(&ReceivedMainPowerState, std::memory_order_acquire);
#if 0
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

//=================================================================================================
// setpoint_adjustment_group implementation
//=================================================================================================

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
	UnitLabel->labelfont(FL_COURIER_BOLD);
	UnitLabel->labelsize(22);
	UnitLabel->labelcolor(FL_BLACK);
	UnitLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	// Center: buttons arranged vertically
	int ButtonGroupStartX = X + 40 + ValueDisplayWidth + AmpereLabelWidth + 70;
	int ButtonGroupStartY = Y + VerticalCenterPadding + TitleSpace;

	Button_Plus1Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY, ButtonWidth, ButtonHeight, "+1");
	Button_Plus1Ptr->labelfont(FL_COURIER_BOLD);
	Button_Plus1Ptr->labelsize(12);
	Button_Plus1Ptr->callback(buttonPlusOneCallback, this);

	Button_Plus01Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + ButtonHeight + ButtonSpacing, ButtonWidth,
	                                 ButtonHeight, "+0.1");
	Button_Plus01Ptr->labelfont(FL_COURIER_BOLD);
	Button_Plus01Ptr->labelsize(12);
	Button_Plus01Ptr->callback(buttonPlusTenthCallback, this);

	Button_Minus01Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + 2 * (ButtonHeight + ButtonSpacing),
	                                  ButtonWidth, ButtonHeight, "-0.1");
	Button_Minus01Ptr->labelfont(FL_COURIER_BOLD);
	Button_Minus01Ptr->labelsize(12);
	Button_Minus01Ptr->callback(buttonMinusTenthCallback, this);

	Button_Minus1Ptr = new Fl_Button(ButtonGroupStartX, ButtonGroupStartY + 3 * (ButtonHeight + ButtonSpacing),
	                                 ButtonWidth, ButtonHeight, "-1");
	Button_Minus1Ptr->labelfont(FL_COURIER_BOLD);
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

power_indicator::power_indicator(int X, int Y, int W, int H, const char *Label) : Fl_Widget(X, Y, W, H, Label) {}

void power_indicator::setBrightness(indicator_light_state NewState) {
	if (State != NewState) {
		State = NewState;
		redraw();
	}
}

void power_indicator::draw() {
	const Fl_Color ColorOn = fl_rgb_color(0xFF, 0x5F, 0x5B); // 0xFF, 0x3C, 0x3C
	const Fl_Color ColorDim = fl_rgb_color(0xE3, 0xD3, 0x7A);
	const Fl_Color ColorOff = fl_rgb_color(0x00, 0x91, 0x00); // 0x00, 0x7A, 0x00
	const Fl_Color ColorUndefined = fl_rgb_color(0xD0, 0xD8, 0xD0);
	const Fl_Color ColorRim = fl_rgb_color(0x5C, 0x5C, 0x5C);
	const Fl_Color ColorRimUndefined = fl_rgb_color(0xCC, 0xCC, 0xCC);

	int Diameter = (w() < h()) ? w() : h();
	int PosX = x() + (w() - Diameter) / 2;
	int PosY = y() + (h() - Diameter) / 2;

	fl_color(State == indicator_light_state::UNDEFINED ? ColorRimUndefined : ColorRim);
	fl_pie(PosX, PosY, Diameter, Diameter, 0.0, 360.0);

	int Rim = (Diameter >= 12) ? 4 : 1;
	int Inner = Diameter - 2 * Rim;
	if (Inner > 0) {
		if (indicator_light_state::ON == State) {
			fl_color(ColorOn);
		}
		else if (indicator_light_state::DIM == State) {
			fl_color(ColorDim);
		}
		else if (indicator_light_state::UNDEFINED == State) {
			fl_color(ColorUndefined);
		}
		else {
			fl_color(ColorOff);
		}

		fl_pie(PosX + Rim, PosY + Rim, Inner, Inner, 0.0, 360.0);
	}
}

void initializeGraphicWidgets() {
	FailureMessagePtr =
	    new Fl_Box((MAIN_WINDOW_WIDTH * 1) / 16, 40, (MAIN_WINDOW_WIDTH * 14) / 16, MAIN_WINDOW_HEIGHT, "");
	FailureMessagePtr->hide();

	SeparatorPtr = new Fl_Box(0, MAIN_MENU_HEIGHT, MAIN_WINDOW_WIDTH, 1, nullptr);
	SeparatorPtr->box(FL_FLAT_BOX);
	SeparatorPtr->color(SEPARATOR_COLOR);

	Fl_Box *ContactorTitle = new Fl_Box(0, MAIN_MENU_HEIGHT + 1 + 2, MAIN_WINDOW_WIDTH, 20, "Stycznik mocy");
	ContactorTitle->labelfont(FL_HELVETICA);
	ContactorTitle->labelsize(16);
	ContactorTitle->labelcolor(FL_BLACK);
	ContactorTitle->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	PowerIndicatorPtr = new power_indicator(60, 60, 46, 46);

	PowerButtonPtr = new Fl_Button(180, 65, 90, 34, "");
	PowerButtonPtr->callback(powerButtonCallback);

	PowerStateDescriptionPtr = new Fl_Box(0, 60 + 46 + 5, (MAIN_WINDOW_WIDTH * 9) / 20, 20, "");
	PowerStateDescriptionPtr->labelfont(FL_HELVETICA);
	PowerStateDescriptionPtr->labelsize(12);
	PowerStateDescriptionPtr->labelcolor(FL_BLACK);
	PowerStateDescriptionPtr->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

	// First setpoint adjustment group: middle 1/3 of main window
	SetpointGroup1Ptr =
	    new setpoint_adjustment_group(0, MAIN_WINDOW_HEIGHT / 3, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT / 3, 1);
	SetpointGroup1Ptr->setTitle("Prąd zadany w kwadrupolu górnym");

	// Second setpoint adjustment group: bottom 1/3 of main window
	SetpointGroup2Ptr =
	    new setpoint_adjustment_group(0, (MAIN_WINDOW_HEIGHT * 2) / 3, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT / 3, 2);
	SetpointGroup2Ptr->setTitle("Prąd zadany w kwadrupolu dolnym");

	(void)PowerIndicatorPtr;
}

void showFailureMessageWidget(initialization_failure_codes FailureCodeForGui) {
	static char Buffer[300];
	snprintf(Buffer, sizeof(Buffer) - 1,
	         "Błędy podczas startu aplikacji\n%s\n\nUruchom aplikację z parametrem -v w konsoli,\nżeby uzyskać "
	         "dodatkowe informacje",
	         getErrorDescription(FailureCodeForGui));
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

// TODO write once again
static const char *getErrorDescription(initialization_failure_codes Error) {
	switch (Error) {
	case initialization_failure_codes::NO_FAILURE:
		return "Brak błędów";
	case initialization_failure_codes::ERROR_COMMAND_LINE_SYNTAX:
		return "Błąd składniowy w linii komendy";
	case initialization_failure_codes::ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH:
		return "Nie można uzyskać ścieżki do programu";
	case initialization_failure_codes::ERROR_SETTINGS_UNABLE_TO_OPEN_FILE:
		return "Błąd otwierania pliku konfiguracyjnego";
	case initialization_failure_codes::ERROR_SETTINGS_PORT_NAME_NOT_FOUND:
		return "Błąd nazwy portu szeregowego w pliku konfiguracyjnym";
	case initialization_failure_codes::ERROR_SETTINGS_REDUNDANT_PORT_NAME:
		return "Nadmiarowa nazwa portu szeregowego w pliku konfiguracyjnym";
	case initialization_failure_codes::ERROR_DEVICE_NAME_MISMATCH:
		return "Nieprawidłowa nazwa urządzenia odczytana z Modbus";
	case initialization_failure_codes::ERROR_DEVICE_TIME_STAMP_MISMATCH:
		return "Nieprawidłowa sygnatura czasowa urządzenia odczytana z Modbus";
	case initialization_failure_codes::ANOTHER_ERROR:
		return "Błąd ogólny";
	default:
		return "Nieznany błąd";
	}
}

//=================================================================================================
// Definitions of functions related to peripheral thread
//=================================================================================================

void refreshWholeGui(void *Data) {
	(void)Data; // intentionally unused

	indicator_light_state CurrentState;

	switch (atomic_load_explicit(&ReceivedMainPowerState, std::memory_order_acquire)) {
	case main_power_state::UNDEFINED:
		CurrentState = indicator_light_state::UNDEFINED;
		PowerButtonPtr->activate();
		PowerButtonPtr->label("?");
		PowerStateDescriptionPtr->label("Stycznik nieokreślony");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	case main_power_state::ON:
		CurrentState = indicator_light_state::ON;
		PowerButtonPtr->activate();
		PowerButtonPtr->label("Wyłącz");
		PowerStateDescriptionPtr->label("Stycznik włączony");
		SetpointGroup1Ptr->activateButtons();
		SetpointGroup2Ptr->activateButtons();
		break;
	case main_power_state::OFF:
		CurrentState = indicator_light_state::OFF;
		PowerButtonPtr->activate();
		PowerButtonPtr->label("Włącz");
		PowerStateDescriptionPtr->label("Stycznik wyłączony");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	case main_power_state::RISING:
	case main_power_state::FALLING:
		CurrentState = indicator_light_state::DIM;
		PowerButtonPtr->deactivate();
		PowerStateDescriptionPtr->label("W trakcie przełączania");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	default:
		std::cout << "Unexpected main power state" << '\n';
		PowerButtonPtr->activate();
		PowerStateDescriptionPtr->label("Nieznany stan stycznika");
		SetpointGroup1Ptr->deactivateButtons();
		SetpointGroup2Ptr->deactivateButtons();
		break;
	}

	PowerIndicatorPtr->setBrightness(CurrentState);

	// Update setpoint displays
	if (SetpointGroup1Ptr != nullptr) {
		SetpointGroup1Ptr->updateDisplay();
	}
	if (SetpointGroup2Ptr != nullptr) {
		SetpointGroup2Ptr->updateDisplay();
	}
}
