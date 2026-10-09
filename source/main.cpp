/// @file main.cpp
///
/// Abbreviations:
///		uA = micro amperes
/// 	FSM = finite state machine

#include <atomic>
#include <cassert>
#include <climits> // for PATH_MAX
#include <csignal>
#include <cstdlib>
#include <execinfo.h> // backtrace
#include <iostream>
#include <libgen.h> // for dirname
#include <string>
#include <thread>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H> // to eliminate flickering
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include "gui_widgets.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define DEFAULT_STATUS_LEVEL 1

//.................................................................................................
// Definitions of types
//.................................................................................................

/// This is Esc-proof window (a FLTK standard window is sensitive to Esc)
class WindowEscProof : public Fl_Double_Window {
  public:
	WindowEscProof(int W, int H, const char *title) : Fl_Double_Window(W, H, title) {}
	int handle(int event) override;
};

//.................................................................................................
// Global variables
//.................................................................................................

/// This variable is set if there is argument "-v" or "--verbose" in command line
bool VerboseMode;

/// This variable points to the main application window
WindowEscProof *ApplicationWindow;

int StatusLevelForGui;

std::string ThisApplicationDirectory;

std::string ConfigurationFilePath;

//.................................................................................................
// Local variables
//.................................................................................................

/// This variable is used to locate the configuration file
static std::string *ConfigurationFilePathPtr;

//.................................................................................................
// Local function prototypes
//.................................................................................................

static void criticalHandler(int Signal);

static void setupCriticalSignalHandler();

static void onMainWindowCloseCallback(Fl_Widget *Widget, void *Data);

static FailureCodes mainInitializations(int argc, char **argv);

static FailureCodes determineVerbosity(int argc, char **argv);

static void callbackForMenuItemStatus(Fl_Widget *WidgetPtr, void *);

static void callbackForMenuItemHelp(Fl_Widget *, void *);

static FailureCodes determineApplicationPath(char *Argv0);

//.................................................................................................
// The main application
//.................................................................................................

int main(int argc, char **argv) {
	setupCriticalSignalHandler();

	FailureCodes ErrorCode = mainInitializations(argc, argv);

	// Main window of the application
	Fl::scheme("gtk+");
	ApplicationWindow =
	    new WindowEscProof(MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, "Kwadrupole Linia Pionowa");
	ApplicationWindow->begin();
	ApplicationWindow->color(COLOR_BACKGROUND);
	ApplicationWindow->callback(onMainWindowCloseCallback); // Window close event is handled
	const std::string WindowIconPath = ThisApplicationDirectory + "/find-location-symbolic.png";
	Fl_PNG_Image *WindowIcon = new Fl_PNG_Image(WindowIconPath.c_str());
	if ((nullptr != WindowIcon) && (nullptr != WindowIcon->data()) && (WindowIcon->w() > 0) && (WindowIcon->h() > 0)) {
		ApplicationWindow->icon(WindowIcon);
	}
	else {
		delete WindowIcon;
	}

	// Menu
	Fl_Menu_Bar MenuWidget(0, 0, MAIN_WINDOW_WIDTH, MAIN_MENU_HEIGHT);
	MenuWidget.box(FL_FLAT_BOX);

	MenuWidget.add("Narzędzia/Status/Ukryty", 0, callbackForMenuItemStatus, (void *)0, FL_MENU_RADIO);
	int indexOfMenuItemStatusNormal = MenuWidget.add("Narzędzia/Status/Normalny", 0, callbackForMenuItemStatus, (void *)1, FL_MENU_RADIO);
	MenuWidget.add("Narzędzia/Status/Serwisowy", 0, callbackForMenuItemStatus, (void *)2, FL_MENU_RADIO);
	MenuWidget.add("Pomoc/Instrukcja użytkowania", 0, callbackForMenuItemHelp);

	Fl_Menu_Item *MenuItems = const_cast<Fl_Menu_Item *>(MenuWidget.menu());
	MenuWidget.setonly(&MenuItems[indexOfMenuItemStatusNormal]);
	MenuWidget.textsize(12);

	StatusLevelForGui = DEFAULT_STATUS_LEVEL;

	// initializeFailureMessageWidget();

	if (FailureCodes::NO_FAILURE == ErrorCode) {
		initializeGraphicWidgets();
	}
	else {
		showFailureMessageWidget(ErrorCode);
	}

	ApplicationWindow->end();
	ApplicationWindow->show();

	Fl::lock(); // Enable multi-threading support in FLTK; register a callback function for Fl::awake()

	// if (FailureCodes::NO_FAILURE == ErrorCode) {
	// 	serialCommunicationStart();
	// }

	return Fl::run();
}

//.................................................................................................
// Function definitions
//.................................................................................................

/// Overlay handle() method
int WindowEscProof::handle(int event) {
	if (event == FL_KEYDOWN) {              // Check if it is a key event
		if (Fl::event_key() == FL_Escape) { // Check if it is the Esc key
			return 1;                       // Block the default behavior
		}
	}
	return Fl_Window::handle(event); // For other events, call the default handler
}

/// This function is used to save the log file in case of SIGSEGV and so on
static void criticalHandler(int Signal) {
	void *Frames[100];
	int NumberOfFrames = backtrace(Frames, 100);

	FILE *LogFileHandler = fopen("backtrace_Faraday_cups.log", "a");
	if (nullptr != LogFileHandler) {
		time_t TimeNow = time(nullptr);
		fprintf(LogFileHandler, "\n=== Backtrace (");
		if (SIGSEGV == Signal) {
			fprintf(LogFileHandler, "signal SIGSEGV");
		}
		else if (SIGABRT == Signal) {
			fprintf(LogFileHandler, "signal SIGABRT");
		}
		else if (SIGFPE == Signal) {
			fprintf(LogFileHandler, "signal SIGFPE");
		}
		else if (SIGILL == Signal) {
			fprintf(LogFileHandler, "signal SIGILL");
		}
		else if (SIGBUS == Signal) {
			fprintf(LogFileHandler, "signal SIGBUS");
		}
		else {
			fprintf(LogFileHandler, "signal %d", Signal);
		}
		fprintf(LogFileHandler, ") at %s\n", ctime(&TimeNow));
		char **Symbols = backtrace_symbols(Frames, NumberOfFrames);
		if (nullptr != Symbols) {
			for (int i = 0; i < NumberOfFrames; i++) {
				fprintf(LogFileHandler, "%s\n", Symbols[i]);
			}
			free(Symbols);
		}
		fclose(LogFileHandler);
	}
	signal(Signal, SIG_DFL);
	kill(getpid(), Signal);
}

// this function hooks up the function criticalHandler()
static void setupCriticalSignalHandler() {
	signal(SIGSEGV, criticalHandler);
	signal(SIGABRT, criticalHandler);
	signal(SIGFPE, criticalHandler);
	signal(SIGILL, criticalHandler);
	signal(SIGBUS, criticalHandler);
}

// Window close event is handled here
static void onMainWindowCloseCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget; // intentionally unused
	(void)Data;   // intentionally unused

	if (VerboseMode) {
		std::cout << "Zamykanie aplikacji" << '\n';
	}
//	serialCommunicationExit();
	ApplicationWindow->hide(); // close the application
}

static FailureCodes mainInitializations(int argc, char **argv) {
//	initializeSerialCommunicationModule();

	FailureCodes FailureCode = determineVerbosity(argc, argv);

	if (FailureCodes::NO_FAILURE == FailureCode) {
		FailureCode = determineApplicationPath(argv[0]);
	}
	// if (FailureCodes::NO_FAILURE == FailureCode) {
	// 	FailureCode = configurationFileParsing();
	// }
//	if (FailureCodes::NO_FAILURE == FailureCode) {
//		FailureCode = initializeModbus();
//	}
	// for (int Cup = 0; Cup < CUPS_NUMBER; Cup++) {
	// 	for (int J = 0; J < MODBUS_INPUTS_PER_CUP; J++) {
	// 		int TemporaryRegisterIndex = Cup * MODBUS_INPUTS_PER_CUP + J;
	// 		assert(TemporaryRegisterIndex < MODBUS_INPUT_REGISTERS_NUMBER);
	// 		atomic_store_explicit(&ModbusInputRegisters[TemporaryRegisterIndex], 0xFFFF, std::memory_order_release);
	// 	}
	// }
	// for (int Cup = 0; Cup < CUPS_NUMBER; Cup++) {
	// 	for (int J = 0; J < MODBUS_COILS_PER_CUP; J++) {
	// 		int TemporaryRegisterIndex = Cup * MODBUS_COILS_PER_CUP + J;
	// 		assert(TemporaryRegisterIndex < MODBUS_COILS_NUMBER);
	// 		atomic_store_explicit(&ModbusCoilsReadout[TemporaryRegisterIndex], false, std::memory_order_release);
	// 	}
	// }
	return FailureCode;
}

static FailureCodes determineVerbosity(int argc, char **argv) {
	for (int J = 1; J < argc; J++) {
		std::string Argument = argv[J];
		if (Argument == "-v") {
			VerboseMode = true;

#if 0 // debugging
            std::string Argument0 = argv[0];
        	std::cout << "Wywołanie programu: " << Argument0 << '\n';
#endif
		}
		else {
			std::cout << "Nieznany argument: " << Argument << '\n';
			return FailureCodes::ERROR_COMMAND_LINE_SYNTAX;
		}
	}
	if (VerboseMode) {
		std::cout << "Tryb \"verbose\"" << '\n';
	}
	return FailureCodes::NO_FAILURE;
}

static void callbackForMenuItemStatus(Fl_Widget *WidgetPtr, void *) {
	auto *TemporaryMenu = static_cast<Fl_Menu_Bar *>(WidgetPtr);
	const Fl_Menu_Item *TemporaryMenuItem = TemporaryMenu->mvalue();
	if (nullptr == TemporaryMenuItem) {
		return;
	}

	StatusLevelForGui = static_cast<int>(reinterpret_cast<intptr_t>(TemporaryMenuItem->user_data()));
	if (VerboseMode) {
		std::cout << "Opcja Status ustawiona na wartość: " << StatusLevelForGui << '\n';
	}
}

static void callbackForMenuItemHelp(Fl_Widget *, void *) {
	const char *PdfFileName = "kwadrupole-linia-pionowa-v.2.pdf";

	std::string DisplayPdfCommand = "xdg-open \"" + ThisApplicationDirectory + "/" + std::string(PdfFileName) + "\"";

	int Result = std::system(DisplayPdfCommand.c_str());
	if (Result != 0) {
		fl_alert("Nie udało się otworzyć pliku PDF.");
	}
}

/// The function searches for the directory where the executable file is located
/// @return code defined in FailureCodes
static FailureCodes determineApplicationPath(char *Argv0) {
	char Path[PATH_MAX];
	ConfigurationFilePathPtr = nullptr;

	if (realpath(Argv0, Path) != nullptr) {
		ThisApplicationDirectory = dirname(Path);
		ConfigurationFilePath = ThisApplicationDirectory;
		ConfigurationFilePathPtr = &ConfigurationFilePath;
		if (VerboseMode) {
#if 0
        	std::cout << "PATH_MAX= " << PATH_MAX << '\n';
#endif
			std::cout << " Katalog programu: " << ThisApplicationDirectory << '\n' 
			<< " Data kompilacji: " << __DATE__ << " " << __TIME__ << '\n';
		}
	}
	else {
		std::cerr << "Nie udało się uzyskać ścieżki do programu." << '\n';
		return FailureCodes::ERROR_SETTINGS_UNABLE_TO_OBTAIN_PATH;
	}
	return FailureCodes::NO_FAILURE;
}

