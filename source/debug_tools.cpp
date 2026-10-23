/// @file debug_tools.cpp
/// @brief Provides non-blocking console input functionality.

#include <cassert>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <system_error>
#include <termios.h>
#include <unistd.h>

#include "debug_tools.h"
#include "multithread_resources.h"
#include "peripheral_thread.h"

// Module under test:
#include "usart_master.h"

//=================================================================================================
// Classes
//=================================================================================================

namespace {

class console_mode {
  public:
	console_mode() {
		if (0 == isatty(STDIN_FILENO)) {
			throw std::runtime_error("Standard input is not a terminal");
		}

		if (tcgetattr(STDIN_FILENO, &OriginalMode) == -1) {
			throw std::system_error(errno, std::generic_category(), "tcgetattr");
		}

		termios NonCanonicalMode = OriginalMode;
		NonCanonicalMode.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
		NonCanonicalMode.c_cc[VMIN] = 0;
		NonCanonicalMode.c_cc[VTIME] = 0;

		if (tcsetattr(STDIN_FILENO, TCSANOW, &NonCanonicalMode) == -1) {
			throw std::system_error(errno, std::generic_category(), "tcsetattr");
		}
	}

	~console_mode() { tcsetattr(STDIN_FILENO, TCSANOW, &OriginalMode); }

	console_mode(const console_mode &) = delete;
	console_mode &operator=(const console_mode &) = delete;

  private:
	termios OriginalMode{};
};

} // namespace

//=================================================================================================
// Local variables
//=================================================================================================

static console_mode *ConsoleModePtr = nullptr;

//=================================================================================================
// Function prototypes
//=================================================================================================

int readConsoleKeyNonBlocking();

/// Reads one byte from the console without waiting.
/// Returns the byte value, or -1 when no input is available.
int readConsoleKeyNonBlocking() {
	pollfd Input{STDIN_FILENO, POLLIN, 0};
	int PollResult;
	do {
		PollResult = poll(&Input, 1, 0);
	} while (PollResult == -1 && errno == EINTR);

	if (PollResult == -1) {
		throw std::system_error(errno, std::generic_category(), "poll");
	}
	if (PollResult == 0) {
		return -1;
	}
	if ((Input.revents & POLLIN) == 0) {
		throw std::runtime_error("Console input is no longer available");
	}

	unsigned char Key;
	ssize_t ReadResult;
	do {
		ReadResult = read(STDIN_FILENO, &Key, sizeof(Key));
	} while (ReadResult == -1 && errno == EINTR);

	if (ReadResult == -1) {
		throw std::system_error(errno, std::generic_category(), "read");
	}
	return ReadResult == 1 ? Key : -1;
}

void debugPeripheralsInitializations() {
	if (ConsoleModePtr == nullptr) {
		ConsoleModePtr = new console_mode();
	}
}

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they can be terminated
bool debuggingPeripheralsTick(bool TerminationSignal) {
	assert(ConsoleModePtr != nullptr);
	static int PreviousKey = -1;
	int Key = readConsoleKeyNonBlocking();
	bool KeepMeRunning = true;
	int JustPressedKey = -1;
	if ((PreviousKey == -1) && (Key != -1)) {
		JustPressedKey = Key;
	}
	PreviousKey = Key;

#if 0 // selftest
	if ((JustPressedKey >= (int)' ') && (JustPressedKey <= (int)'~')) {
		std::cout << "Key pressed: " << (char)JustPressedKey << '\n';
	}
	else {
		if (JustPressedKey != -1) {
			std::cout << "Key pressed: " << JustPressedKey << '\n';
		}
	}
	KeepMeRunning = !TerminationSignal; // instantly stop if termination signal is received
#endif

#if 0 // test of mechanism using the AwakeRefreshWholeGui variable
	if (JustPressedKey == 'S') {
		main_power_state PsuState = atomic_load_explicit(&SharedMainPowerState, std::memory_order_acquire);
		switch (PsuState) {
		case main_power_state::OFF:
			PsuState = main_power_state::RISING;
			break;
		case main_power_state::RISING:
			PsuState = main_power_state::ON;
			break;
		case main_power_state::ON:
			PsuState = main_power_state::FALLING;
			break;
		default:
			PsuState = main_power_state::OFF;
			break;
		}
		atomic_store_explicit(&SharedMainPowerState, PsuState, std::memory_order_release);
		AwakeRefreshWholeGui = true;
		std::cout << "Key pressed: " << (char)JustPressedKey << " | SharedMainPowerState changed: " << static_cast<int>(PsuState) << '\n';
	}
	else if (JustPressedKey == 's') {
		atomic_store_explicit(&SharedMainPowerState, main_power_state::UNDEFINED, std::memory_order_release);
		AwakeRefreshWholeGui = true;
		std::cout << "Key pressed: " << (char)JustPressedKey << " | SharedMainPowerState changed: " << static_cast<int>(main_power_state::UNDEFINED)
		          << " (UNDEFINED)" << '\n';
	}
	else if ((JustPressedKey >= (int)' ') && (JustPressedKey <= (int)'~')) {
		std::cout << "Key pressed: " << (char)JustPressedKey << '\n';
	}
	else {
		if (JustPressedKey != -1) {
			std::cout << "Key pressed: " << JustPressedKey << '\n';
		}
	}
	KeepMeRunning = !TerminationSignal; // instantly stop if termination signal is received
#endif

#if 1
	KeepMeRunning = usartMasterDebuggingTick(TerminationSignal, JustPressedKey);
#endif

	if (!KeepMeRunning) {
		delete ConsoleModePtr;
		ConsoleModePtr = nullptr;
		std::cout << "Terminating debugging peripherals tick properly." << '\n';
		return false;
	}

	return KeepMeRunning;
}