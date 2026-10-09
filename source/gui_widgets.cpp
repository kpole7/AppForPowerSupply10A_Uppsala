/// @file gui_widgets.c

#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include "gui_widgets.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................


//.................................................................................................
// Definitions of types
//.................................................................................................


//.................................................................................................
// Local constants
//.................................................................................................


//.................................................................................................
// Local variables
//.................................................................................................

static Fl_Box *GeneralStatusTextBoxPtr;
static Fl_Box *FailureMessagePtr;

//.................................................................................................
// Local function prototypes
//.................................................................................................

static const char* getErrorDescription(FailureCodes Error);

//.................................................................................................
// Function definitions
//.................................................................................................

PowerIndicator::PowerIndicator(int X, int Y, int W, int H, const char *Label) :
		Fl_Widget(X, Y, W, H, Label), State(IndicatorLightState::OFF) {
}

void PowerIndicator::setBrightness(IndicatorLightState NewState) {
	if (State != NewState) {
		State = NewState;
		redraw();
	}
}

void PowerIndicator::draw() {
	const Fl_Color ColorOn = fl_rgb_color(0x40, 0xFF, 0x40);
	const Fl_Color ColorDim = fl_rgb_color(0x70, 0x70+0x30, 0x70);
	const Fl_Color ColorOff = fl_rgb_color(0x70, 0x70+0x08, 0x70);
	const Fl_Color ColorRim = fl_rgb_color(0x4C, 0x4C, 0x4C);

	int Diameter = (w() < h()) ? w() : h();
	int PosX = x() + (w() - Diameter) / 2;
	int PosY = y() + (h() - Diameter) / 2;

	fl_color(ColorRim);
	fl_pie(PosX, PosY, Diameter, Diameter, 0.0, 360.0);

	int Rim = (Diameter >= 12) ? 3 : 1;
	int Inner = Diameter - 2 * Rim;
	if (Inner > 0) {
		fl_color(State == IndicatorLightState::ON ? ColorOn : (State == IndicatorLightState::DIM ? ColorDim : ColorOff));
		fl_pie(PosX + Rim, PosY + Rim, Inner, Inner, 0.0, 360.0);
	}
}

void initializeGraphicWidgets() {
	int GeneralStatusTextBoxPositionX = 130; 
	GeneralStatusTextBoxPtr = new Fl_Box(GeneralStatusTextBoxPositionX, 1, 
		MAIN_WINDOW_WIDTH - GeneralStatusTextBoxPositionX, 20, "Tu powinny być różne dane");
	GeneralStatusTextBoxPtr->hide();
	GeneralStatusTextBoxPtr->labelfont(FL_COURIER);
	GeneralStatusTextBoxPtr->labelsize(8);
	GeneralStatusTextBoxPtr->labelcolor(FL_BLACK);
	GeneralStatusTextBoxPtr->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
#if 0 // debugging
	GeneralStatusTextBoxPtr->color(FL_CYAN);
	GeneralStatusTextBoxPtr->box(FL_FLAT_BOX);
#endif

	FailureMessagePtr = new Fl_Box((MAIN_WINDOW_WIDTH * 1) / 16, 40, 
		(MAIN_WINDOW_WIDTH * 14) / 16, MAIN_WINDOW_HEIGHT, "");
	FailureMessagePtr->hide();

	PowerIndicator* PowerIndicatorPtr = new PowerIndicator(25, 50, 30, 30);
	(void)PowerIndicatorPtr;

}

void showFailureMessageWidget(FailureCodes FailureCodeForGui) {
	static char Buffer[300];
	snprintf(Buffer, sizeof(Buffer) - 1, 
		"Błędy podczas startu aplikacji\n%s\n\nUruchom aplikację z parametrem -v w konsoli,\nżeby uzyskać dodatkowe informacje", 
		getErrorDescription(FailureCodeForGui));
	FailureMessagePtr->label(Buffer);
	FailureMessagePtr->show();
	if (VerboseMode) {
		std::cout << "--------------------------------------GUI FAILURE MESSAGE--------------------------------------" << '\n';
		std::cout << Buffer << '\n';
		std::cout << "-----------------------------------------------------------------------------------------------" << '\n';
	}
}

// TODO Napisać od nowa
static const char* getErrorDescription(FailureCodes Error) {
	switch (Error) {
		case FailureCodes::NO_FAILURE:
			return "Brak błędów";
		case FailureCodes::ERROR_COMMAND_LINE_SYNTAX:
			return "Błąd składniowy w linii komendy";
		case FailureCodes::ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH:
			return "Nie można uzyskać ścieżki do programu";
		case FailureCodes::ERROR_SETTINGS_UNABLE_TO_OPEN_FILE:
			return "Błąd otwierania pliku konfiguracyjnego";
		case FailureCodes::ERROR_SETTINGS_PORT_NAME_NOT_FOUND:
			return "Błąd nazwy portu szeregowego w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_REDUNDANT_PORT_NAME:
			return "Nadmiarowa nazwa portu szeregowego w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_CUP_INSERTING_TIMEOUTS_NOT_FOUND:
			return "Nie znaleziono opisu limitów czasu wsuwania kubków w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_CUP_WITHDRAWING_TIMEOUTS_NOT_FOUND:
			return "Nie znaleziono opisu limitów czasu schowania kubków w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_CALIBRATION_CURRENTS_NOT_FOUND:
			return "Nie znaleziono prądów kalibracyjnych w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_CALIBRATION_ADC_READINGS_NOT_FOUND:
			return "Nie znaleziono danych kalibracyjnych w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_REDUNDANT_CUP_NAME:
			return "Nadmiarowa nazwa kubka w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_REDUNDANT_PARAMETER_DEFINITION:
			return "Nadmiarowa deklaracja parametru w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_REDUNDANT_CURRENT_DEFINITION:
			return "Nadmiarowa deklaracja prądu kalibracyjnego w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_REDUNDANT_ADC_READING:
			return "Nadmiarowa deklaracja odczytu ADC";
		case FailureCodes::ERROR_SETTINGS_CONVERTION_TO_NUMBER:
			return "Błąd konwersji liczby w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_VALUE_OUT_OF_RANGE:
			return "Niepoprawna wartość liczby w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_TOO_HIGH_CURRENT_VALUE:
			return "Prąd kalibracyjny przekracza maksymalną wartość w pliku konfiguracyjnym";
		case FailureCodes::ERROR_SETTINGS_INCORRECT_CUP_OR_CHANNEL_INDEX:
			return "Niepoprawny indeks kubka lub kanału w pliku konfiguracyjnym";
		case FailureCodes::ERROR_MODBUS_INITIALIZATION_1:
			return "Błąd inicjalizacji Modbus 1";
		case FailureCodes::ERROR_MODBUS_INITIALIZATION_2:
			return "Błąd inicjalizacji Modbus 2";
		case FailureCodes::ERROR_MODBUS_OPENING:
			return "Błąd otwierania Modbus";
		case FailureCodes::ERROR_MODBUS_READING:
			return "Błąd odczytu Modbus";
		case FailureCodes::ERROR_MODBUS_WRITING:
			return "Błąd zapisu Modbus";
		case FailureCodes::ERROR_MODBUS_FRAME_READ:
			return "Błąd ramki odczytu Modbus";
		case FailureCodes::ERROR_DEVICE_NAME_MISMATCH:
			return "Nieprawidłowa nazwa urządzenia odczytana z Modbus";
		case FailureCodes::ERROR_DEVICE_TIME_STAMP_MISMATCH:
			return "Nieprawidłowa sygnatura czasowa urządzenia odczytana z Modbus";
		case FailureCodes::ANOTHER_ERROR:
			return "Błąd ogólny";
		default:
			return "Nieznany błąd";
	}
}
