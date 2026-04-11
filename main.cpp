#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <cctype>
#include <charconv>
#ifdef _WIN32
#include <mmsystem.h>
#include <tlhelp32.h>
#endif

#include <curses.h>

using namespace std::literals;

#ifndef WATCH_PROJECT_VERSION
#define WATCH_PROJECT_VERSION "unknown"
#endif
namespace watch {
constexpr std::string_view VERSION{WATCH_PROJECT_VERSION};

using Option = std::array<std::string_view, 2>;
constexpr Option PRECISE_OPTIONS{"-p", "--precise"};
constexpr Option INTERVAL_OPTIONS{"-n", "--interval"};
constexpr Option BEEP_OPTIONS{"-b", "--beep"};
constexpr Option HELP_OPTIONS{"-h", "--help"};
constexpr Option VERSION_OPTIONS{"-v", "--version"};
constexpr std::string joinOptions(Option options) {
	/*return std::views::join_with(options, ", ") |
		   std::ranges::to<std::string>();**/
	return std::string(options[0]) + ", " + std::string(options[1]);
}

const std::string HELP_INFO{
	std::format(R"(Usage: watch [options] (<command> | false)
Options:
	{} beep if command has a non-zero exit
	{} <seconds> seconds to wait between updates, minimum is 0.1
	{} run command in precise intervals

{} show help and exit
{} show version info and exit
)",
				joinOptions(BEEP_OPTIONS), joinOptions(INTERVAL_OPTIONS),
				joinOptions(PRECISE_OPTIONS), joinOptions(HELP_OPTIONS),
				joinOptions(VERSION_OPTIONS))};
void showHelp() { std::print("{}", HELP_INFO); }
void showHelpAndExit(int status = 0) {
	showHelp();
	std::exit(status);
}

/*const std::string joinArguments(const int argc, const char* const argv[]) {
	return std::views::counted(argv, argc) |
		   std::views::transform([](const char* arg) {
			   return std::string_view{arg}.contains(' ')
						  ? std::format("\"{}\"", arg)
						  : std::string{arg};
		   }) |
		   std::views::join_with(std::string_view{" "}) |
		   std::ranges::to<std::string>();
}*/

const std::string joinArguments(const int argc, const char* const argv[]) {
	std::ostringstream oss;
	std::ranges::copy(std::views::counted(argv, argc) |
						  std::views::transform([](const char* arg) {
							  return std::string_view{arg}.contains(' ')
										 ? std::format("\"{}\"", arg)
										 : std::string{arg};
						  }),
					  std::ostream_iterator<std::string>(oss, " "));
	std::string result = oss.str();
	if (!result.empty() && result.back() == ' ') {
		result.pop_back();
	}
	return result;
}

enum class ShellType {
	CMD,
	POWERSHELL_LEGACY,
	POWERSHELL,
	POSIX_GENERIC,
	POSIX_GITBASH
};

const std::string getEnvString(const std::string& name) {
	const char* value = std::getenv(name.c_str());
	return value ? std::string{value} : std::string{};
}

#ifdef _WIN32
static const std::string getGitBashExePathEnvString() {
	std::string gitExePath = getEnvString("EXEPATH");
	if (!gitExePath.empty() && gitExePath.back() == '\\') {
		gitExePath.pop_back();
	}
	return gitExePath;
}

static const std::string getParentProcessName() {
	DWORD currentPid = GetCurrentProcessId();
	DWORD parentPid = 0;
	std::string parentName;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return "";

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &pe32)) {
		do {
			if (pe32.th32ProcessID == currentPid) {
				parentPid = pe32.th32ParentProcessID;
				break;
			}
		} while (Process32Next(snapshot, &pe32));
	}

	if (parentPid != 0) {
		if (Process32First(snapshot, &pe32)) {
			do {
				if (pe32.th32ProcessID == parentPid) {
					parentName = pe32.szExeFile;
					break;
				}
			} while (Process32Next(snapshot, &pe32));
		}
	}

	CloseHandle(snapshot);

	if (!parentName.empty()) {
		std::ranges::transform(parentName, parentName.begin(),
							   [](unsigned char c) { return std::tolower(c); });
	}
	return parentName;
}
#endif

const std::string escapeQuotes(const std::string& cmd) {
	std::string res = cmd;
	size_t pos = 0;
	while ((pos = res.find('"', pos)) != std::string::npos) {
		res.replace(pos, 1, "\\\"");
		pos += 2;
	}
	return res;
}

watch::ShellType detectShell() {
#ifdef _WIN32
	static const std::string parentName = watch::getParentProcessName();

	if (parentName == "powershell.exe") {
		return watch::ShellType::POWERSHELL_LEGACY;
	}
	if (parentName == "pwsh.exe") {
		return watch::ShellType::POWERSHELL;
	}
	if ((!watch::getEnvString("MSYSTEM").empty()) &&
		(!watch::getGitBashExePathEnvString().empty()))
		return watch::ShellType::POSIX_GITBASH;
#endif
	if (!watch::getEnvString("SHELL").empty()) {
		return watch::ShellType::POSIX_GENERIC;
	}
#ifdef WIN32
	return watch::ShellType::CMD;
#else
	return watch::ShellType::POSIX_GENERIC;
#endif
}

int execute(const std::string& command) {
	if (command == "false") {
		return 1;
	};
	// std::println("Executing: {}", command);
	return std::system(command.c_str());
}

int executeInShell(const std::string& command, watch::ShellType shellType) {
	switch (shellType) {
		case watch::ShellType::POWERSHELL:
			return execute(std::format("pwsh -Command \"{}\"", command));
#ifdef WIN32
		case watch::ShellType::POWERSHELL_LEGACY:
			return execute(std::format("powershell -Command \"{}\"", command));
		case watch::ShellType::POSIX_GENERIC: {
			return execute(std::format("sh -c \"{}\"", escapeQuotes(command)));
		}
		case watch::ShellType::POSIX_GITBASH: {
			static const std::string gitExePath = getGitBashExePathEnvString();
			if (gitExePath.empty())
				throw std::runtime_error{
					"EXEPATH environment variable is not set for Git Bash."};
			return execute(std::format("\"\"{}\\sh.exe\" -c \"{}\"\"",
									   gitExePath, escapeQuotes(command)));
		}
#endif
		default:
			return execute(command);
	}
}
/*
void beep() {
#ifdef _WIN32
	std::thread([]() {
		if (!Beep(750, 100)) {
			std::println("Error {}", GetLastError());
		}
	}).detach();
#else
	std::cout << '\a' << std::flush;
#endif
}*/

static bool parseSecondsArg(const char* arg, double& out) {
	std::string_view sv{arg};
	double tmp{};
	auto res = std::from_chars(sv.data(), sv.data() + sv.size(), tmp);
	if (res.ec != std::errc() || res.ptr != sv.data() + sv.size()) return false;
	if (!std::isfinite(tmp) || tmp <= 0.0) return false;
	out = tmp;
	return true;
}
}  // namespace watch
int main(int argc, char* argv[]) {
	bool enableBeep{false};
	bool isPrecise{false};
	int index{1};
	auto interval{2000ms};
	try {
		while (index < argc) {
			if (argv[index][0] != '-') break;
			const auto equalToThisArg{
				[&](std::string_view s) { return s == argv[index]; }};
			if (std::ranges::any_of(watch::HELP_OPTIONS, equalToThisArg)) {
				watch::showHelpAndExit();
			} else if (std::ranges::any_of(watch::VERSION_OPTIONS,
										   equalToThisArg)) {
				std::println("145watch by 145a {}", watch::VERSION);
				return 0;
			} else if (std::ranges::any_of(watch::PRECISE_OPTIONS,
										   equalToThisArg)) {
				isPrecise = true;
			} else if (std::ranges::any_of(watch::INTERVAL_OPTIONS,
										   equalToThisArg)) {
				index++;
				if (index >= argc) {
					std::println(
						"Interval option "
						"requires an argument.");
					watch::showHelpAndExit();
				}
				const char* arg = argv[index];
				double seconds{};
				if (!watch::parseSecondsArg(arg, seconds)) {
					throw std::invalid_argument{"Invalid interval."};
				}

				using ms = std::chrono::milliseconds;
				const long double max_seconds =
					static_cast<long double>(
						std::numeric_limits<ms::rep>::max()) /
					1000.0L;
				if (static_cast<long double>(seconds) > max_seconds) {
					throw std::range_error{
						"Interval tooooooooo "
						"large (would "
						"overflow)."};
				}
				auto dur = std::chrono::duration<double>(seconds);
				auto msec = std::chrono::duration_cast<ms>(
					dur + std::chrono::microseconds(500));
				if (msec.count() < 100)
					throw std::range_error{"Interval too small."};
				interval = msec;
			} else if (std::ranges::any_of(watch::BEEP_OPTIONS,
										   equalToThisArg)) {
				enableBeep = true;
			} else
				throw std::invalid_argument(
					std::format("Unknown option: {}", argv[index]));
			index++;
		}
	} catch (const std::exception& e) {
		std::println("Error: {}", e.what());
		watch::showHelpAndExit(1);
	}
	if (index == argc) watch::showHelpAndExit();

	const watch::ShellType shellType = watch::detectShell();
	std::println("Shell type: {}", static_cast<int>(shellType));

	const std::string command{watch::joinArguments(argc - index, argv + index)};
	const std::string message{
		std::format("Every {}s: {} ",
					static_cast<float>(interval.count()) / 1000, command)};
	const auto start{std::chrono::steady_clock::now()};

#ifdef _WIN32
	if (isPrecise) {
		timeBeginPeriod(1);
		std::atexit([]() { timeEndPeriod(1); });
	}
#endif
	std::signal(SIGINT, [](int) { std::exit(0); });

	initscr();
	// TODO: implement TUI
	endwin();

	for (int count{1};; count++) {
		std::println("\n{} {:L%c}", message, std::chrono::system_clock::now());
		int rc = executeInShell(command, shellType);
		if (enableBeep && rc != 0) {
			beep();
		}
		if (isPrecise) {
			std::this_thread::sleep_until(start + interval * count);
		} else {
			std::this_thread::sleep_for(interval);
		}
	}
}
