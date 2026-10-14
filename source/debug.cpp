#include "debug.h"

#include <cerrno>
#include <poll.h>
#include <stdexcept>
#include <system_error>
#include <termios.h>
#include <unistd.h>

namespace {

class ConsoleMode {
  public:
	ConsoleMode() {
		if (!isatty(STDIN_FILENO)) {
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

int readConsoleKeyNonBlocking() {
	static const ConsoleMode Mode;
	(void)Mode;

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