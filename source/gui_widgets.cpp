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

class OnErrorGroup : public Fl_Group {
  public:
	OnErrorGroup(int X, int Y, int W, int H, const char *L = nullptr);
	void draw() override;

  private:
	RunTimeFailureCodes CurrentRuntimeFailureCode;
	Fl_Box *ErrorTextBoxPtr;
};

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

static const char* getErrorDescription(InitializationFailureCodes Error);

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
	const Fl_Color ColorUndefined = fl_rgb_color(0x70+0x60, 0x70+0x08+0x60, 0x70+0x60);
	const Fl_Color ColorRim = fl_rgb_color(0x4C, 0x4C, 0x4C);
	const Fl_Color ColorRimUndefined = fl_rgb_color(0x4C+0x80, 0x4C+0x80, 0x4C+0x80);

	int Diameter = (w() < h()) ? w() : h();
	int PosX = x() + (w() - Diameter) / 2;
	int PosY = y() + (h() - Diameter) / 2;

	fl_color(State == IndicatorLightState::UNDEFINED ? ColorRimUndefined : ColorRim);
	fl_pie(PosX, PosY, Diameter, Diameter, 0.0, 360.0);

	int Rim = (Diameter >= 12) ? 3 : 1;
	int Inner = Diameter - 2 * Rim;
	if (Inner > 0) {
		fl_color(State == IndicatorLightState::ON ? ColorOn : (State == IndicatorLightState::DIM ? ColorDim : 
			(State == IndicatorLightState::UNDEFINED ? ColorUndefined : ColorOff)));
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

	PowerIndicator* PowerIndicator2Ptr = new PowerIndicator(75, 50, 30, 30);
	PowerIndicator2Ptr->setBrightness(IndicatorLightState::UNDEFINED);

	PowerIndicator* PowerIndicator3Ptr = new PowerIndicator(125, 50, 30, 30);
	PowerIndicator3Ptr->setBrightness(IndicatorLightState::ON);
}

void showFailureMessageWidget(InitializationFailureCodes FailureCodeForGui) {
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

// TODO write once again
static const char* getErrorDescription(InitializationFailureCodes Error) {
	switch (Error) {
		case InitializationFailureCodes::NO_FAILURE:
			return "Brak błędów";
		case InitializationFailureCodes::ERROR_COMMAND_LINE_SYNTAX:
			return "Błąd składniowy w linii komendy";
		case InitializationFailureCodes::ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH:
			return "Nie można uzyskać ścieżki do programu";
		case InitializationFailureCodes::ERROR_SETTINGS_UNABLE_TO_OPEN_FILE:
			return "Błąd otwierania pliku konfiguracyjnego";
		case InitializationFailureCodes::ERROR_SETTINGS_PORT_NAME_NOT_FOUND:
			return "Błąd nazwy portu szeregowego w pliku konfiguracyjnym";
		case InitializationFailureCodes::ERROR_SETTINGS_REDUNDANT_PORT_NAME:
			return "Nadmiarowa nazwa portu szeregowego w pliku konfiguracyjnym";
		case InitializationFailureCodes::ERROR_DEVICE_NAME_MISMATCH:
			return "Nieprawidłowa nazwa urządzenia odczytana z Modbus";
		case InitializationFailureCodes::ERROR_DEVICE_TIME_STAMP_MISMATCH:
			return "Nieprawidłowa sygnatura czasowa urządzenia odczytana z Modbus";
		case InitializationFailureCodes::ANOTHER_ERROR:
			return "Błąd ogólny";
		default:
			return "Nieznany błąd";
	}
}
