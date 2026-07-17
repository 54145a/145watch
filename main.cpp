#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <print>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifdef _WIN32
#include <mmsystem.h>
#include <tlhelp32.h>
#endif

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
#ifdef _WIN32
	return watch::ShellType::CMD;
#else
	return watch::ShellType::POSIX_GENERIC;
#endif
}

struct ExecuteResult {
	std::string output;
	int exitCode = 0;
};
ExecuteResult execute(const std::string &command) {
	ExecuteResult result;
	char buffer[128];
	std::string full_cmd = command + " 2>&1";
#ifdef _WIN32
	FILE *pipe = _popen(full_cmd.c_str(), "r");
#else
	FILE *pipe = popen(full_cmd.c_str(), "r");
#endif
	if (!pipe) {
		result.output = "Error: Failed to run command.";
		result.exitCode = -1;
		return result;
	}
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result.output += buffer;
	}
#ifdef _WIN32
	result.exitCode = _pclose(pipe);
#else
	result.exitCode = pclose(pipe);
#endif
	return result;
}
ExecuteResult executeInShell(const std::string &command,
							 watch::ShellType shellType) {
	if (command == "false")
		return {"", 1};
	switch (shellType) {
	case watch::ShellType::POWERSHELL:
		return execute(std::format("pwsh -Command \"{}\"", command));
#ifdef _WIN32
	case watch::ShellType::POWERSHELL_LEGACY:
		return execute(std::format("powershell -Command \"{}\"", command));
	case watch::ShellType::POSIX_GENERIC:
		return execute(std::format("sh -c \"{}\"", escapeQuotes(command)));
	case watch::ShellType::POSIX_GITBASH: {
		static const std::string gitExePath = getGitBashExePathEnvString();
		if (gitExePath.empty())
			throw std::runtime_error{
				"EXEPATH environment variable is not set for Git Bash."};
		return execute(std::format("\"\"{}\\sh.exe\" -c \"{}\"\"", gitExePath,
								   escapeQuotes(command)));
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

static std::sig_atomic_t stop_requested = 0;
int main(int argc, char* argv[]) {
	using namespace ftxui;
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
	std::signal(SIGINT, [](int) { stop_requested = 1; });

	auto screen = ScreenInteractive::Fullscreen();

	std::string latestCommandOutput_{};
	int lastExitCode_ = 0;
	std::mutex dataMutex;
	std::atomic<bool> loop_started{false};
	std::chrono::time_point<std::chrono::steady_clock> lastBeginTime_,
		lastFinishTime_;
	std::thread refreshThread([&]() {
		while (!loop_started.load(std::memory_order_acquire) &&
			   stop_requested == 0) {
			std::this_thread::sleep_for(1ms);
		}
		if (stop_requested) {
			screen.ExitLoopClosure()();
			return;
		}
		int count{1};
		while (stop_requested == 0) {
			auto begin = std::chrono::steady_clock::now();
			auto result = watch::executeInShell(command, shellType);
			auto finish = std::chrono::steady_clock::now();
			{
				std::lock_guard<std::mutex> lock(dataMutex);
				latestCommandOutput_ = result.output;
				lastExitCode_ = result.exitCode;
				lastBeginTime_ = begin;
				lastFinishTime_ = finish;
			}
			screen.PostEvent(Event::Custom);
			if (enableBeep && result.exitCode != 0) {
				std::cout << '\a' << std::flush;
			}
			if (isPrecise) {
				std::this_thread::sleep_until(start + interval * count);
			} else {
				std::this_thread::sleep_for(interval);
			}
			++count;
		}
		screen.ExitLoopClosure()();
	});
	screen.Loop(Renderer([&]() {
		if (stop_requested) {
			screen.ExitLoopClosure()();
			return text("Exiting...");
		}
		loop_started.store(true, std::memory_order_release);
		std::string cmdOutput;
		int exitCode;
		double durationSec;
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			cmdOutput = latestCommandOutput_;
			exitCode = lastExitCode_;
			durationSec =
				std::chrono::duration<double>(lastFinishTime_ - lastBeginTime_)
					.count();
		}
		auto header_right = vbox({

			text(
				std::format(
					"{:L%c}",
					std::chrono::zoned_time{
						std::chrono::current_zone(),
						std::chrono::system_clock::now()
					}
				)
			),
			text(std::format("in {:.3f}s ({})", durationSec, exitCode)) |
				align_right
		});

		auto header{vbox({
			hbox({text(message), filler(), header_right}),
		})};
		return vbox({header, text(cmdOutput)});
	}));
	if (refreshThread.joinable()) {
		refreshThread.join();
	}
}