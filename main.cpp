#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

using namespace std::literals;

constexpr std::string_view VERSION{"1.0"};

using Option = std::array<std::string_view, 2>;
constexpr Option PRECISE_OPTIONS{"-p", "--precise"};
constexpr Option INTERVAL_OPTIONS{"-n", "--interval"};
constexpr Option BEEP_OPTIONS{"-b", "--beep"};
constexpr Option HELP_OPTIONS{"-h", "--help"};
constexpr Option VERSION_OPTIONS{"-v", "--version"};
constexpr std::string joinOptions(Option options) {
	return std::views::join_with(options, ", ") |
		   std::ranges::to<std::string>();
}

const std::string HELP_INFO{
	std::format(R"(Usage: 145watch [options] (<command> | false)
Options:
	{}	beep if command has a non-zero exit
	{} <seconds>	seconds to wait between updates, minimum is 0.1
	{}	run command in precise intervals

{}	show help and exit
{}	show version info and exit
)",
				joinOptions(BEEP_OPTIONS), joinOptions(INTERVAL_OPTIONS),
				joinOptions(PRECISE_OPTIONS), joinOptions(HELP_OPTIONS),
				joinOptions(VERSION_OPTIONS))};
void showHelp() { std::print("{}", HELP_INFO); }
void showHelpAndExit(int status = 0) {
	showHelp();
	std::exit(status);
}

const std::string joinArguments(const int argc, const char* const argv[]) {
	return std::views::counted(argv, argc) |
		   std::views::transform([](const char* arg) {
			   return std::string_view{arg}.contains(' ')
						  ? std::format("\"{}\"", arg)
						  : std::string{arg};
		   }) |
		   std::views::join_with(std::string_view{" "}) |
		   std::ranges::to<std::string>();
}

int execute(const std::string& command) {
	if (command == "false") {
		return 1;
	};
	return std::system(command.c_str());
}

void beep() {
#ifdef _WIN32
	std::println("beep");
	if (!Beep(750, 100)) {
		std::println("Error {}", GetLastError());
	}
#else
	std::cout << '\a' << std::flush;
#endif
}
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
			if (std::ranges::any_of(HELP_OPTIONS, equalToThisArg)) {
				showHelpAndExit();
			} else if (std::ranges::any_of(VERSION_OPTIONS, equalToThisArg)) {
				std::println("145watch by 145a {}", VERSION);
				return 0;
			} else if (std::ranges::any_of(PRECISE_OPTIONS, equalToThisArg)) {
				isPrecise = true;
			} else if (std::ranges::any_of(INTERVAL_OPTIONS, equalToThisArg)) {
				index++;
				if (index >= argc) {
					std::println("Interval option requires an argument.");
					showHelpAndExit();
				}
				double seconds = std::stod(argv[index]);
				if (seconds <= 0) {
					throw std::invalid_argument{"Invalid interval."};
				}

				interval =
					std::chrono::milliseconds{static_cast<int>(seconds * 1000)};
				if (interval.count() < 100)
					throw std::range_error{
						"Interval too small (or sooooooooooooooooooooooooooooo "
						"big that it overflowed, unlikely though)."};
			} else if (std::ranges::any_of(BEEP_OPTIONS, equalToThisArg)) {
				enableBeep = true;
			} else
				throw std::invalid_argument(
					std::format("Unknown option: {}", argv[index]));
			index++;
		}
	} catch (const std::exception& e) {
		std::println("Error: {}", e.what());
		showHelpAndExit(1);
	}
	if (index == argc) showHelpAndExit();

	const std::string command{joinArguments(argc - index, argv + index)};
	const std::string message{std::format(
		"Every {}s: {} ", static_cast<float>(interval.count()) / 1000,
		joinArguments(argc - index, argv + index))};
	const auto start{std::chrono::steady_clock::now()};

#ifdef _WIN32
	if (isPrecise) {
		timeBeginPeriod(1);
		std::atexit([]() { timeEndPeriod(1); });
	}
#endif
	std::signal(SIGINT, [](int) { std::exit(0); });
	for (int count{1};; count++) {
		std::println("\n{} {:L%c}", message, std::chrono::system_clock::now());
		if (enableBeep && execute(command) != 0) {
			beep();
		}
		if (isPrecise) {
			std::this_thread::sleep_until(start + interval * count);
		} else {
			std::this_thread::sleep_for(interval);
		}
	}
}