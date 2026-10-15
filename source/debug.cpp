/// @file debug.cpp
/// @brief Provides non-blocking console input functionality.

#include "debug.h"

#include <iostream>
#include <cerrno>
#include <poll.h>
#include <stdexcept>
#include <system_error>
#include <termios.h>
#include <unistd.h>
#include <cassert>

namespace {

class ConsoleMode {
  public:
	ConsoleMode() {
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

	~ConsoleMode() {
		tcsetattr(STDIN_FILENO, TCSANOW, &OriginalMode);
	}

	ConsoleMode(const ConsoleMode &) = delete;
	ConsoleMode &operator=(const ConsoleMode &) = delete;

  private:
	termios OriginalMode{};
};

} // namespace


ConsoleMode *ConsoleModePtr = nullptr;


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



void peripheralsInitializations() {
    if (ConsoleModePtr == nullptr) {
        ConsoleModePtr = new ConsoleMode();
    }
}

/// @brief Handles a single tick of peripheral operations.
/// @param TerminationSignal tells the function whether it should prepare to terminate the peripheral operations
/// @return true if the peripheral operations should continue, false if they can be terminated
bool debuggingPeripheralsTick(bool TerminationSignal) {
	if (TerminationSignal) {
		delete ConsoleModePtr;
		ConsoleModePtr = nullptr;
		return false;
	}

	assert(ConsoleModePtr != nullptr);
	static int PreviousKey = -1;
	int Key = readConsoleKeyNonBlocking();
	if (Key != PreviousKey){
		if ((Key >= (int)' ') && (Key <= (int)'~')){
			std::cout << "Key changed: " << (char)Key << '\n';
		}
		else {
			std::cout << "Key changed: " << Key << '\n';
		}
	}
	PreviousKey = Key;

	return true; // Continue running peripherals by default
}