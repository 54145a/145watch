#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <format>
#include <iostream>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
#include <mmsystem.h>
#include <process.h>
#pragma comment(lib, "winmm.lib")
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std::literals;

using Option = std::array<std::string_view, 2>;
constexpr Option HELP_OPTIONS{"-h", "--help"};
constexpr Option PRECISE_OPTIONS{"-p", "--precise"};
constexpr std::string joinOptions(Option options) {
	return std::views::join_with(options, ", ") |
		   std::ranges::to<std::string>();
}

const std::string HELP_INFO{std::format(R"(Usage: 145watch [options] command
Options:
	{} show help
	{} run command in precise intervals
)",
										joinOptions(HELP_OPTIONS),
										joinOptions(PRECISE_OPTIONS))};
void showHelp() { std::print("{}", HELP_INFO); }
void showHelpAndExit() {
	showHelp();
	std::exit(0);
}

const std::string joinArguments(const char* const argv[]) {
	std::ostringstream oss;
	for (const char* const* p = argv; *p != nullptr; ++p) {
		oss << *p << " ";
	}
	return oss.str();
}

int execute(char* const argv[]) {
#ifdef _WIN32
	return static_cast<int>(_spawnvp(_P_WAIT, argv[0], argv));
#else
	pid_t pid;
	int status;
	if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv, environ) != 0)
		return -1;
	waitpid(pid, &status, 0);
	return WEXITSTATUS(status);
#endif
}

int main(int argc, char* argv[]) {
	if (argc <= 1) showHelpAndExit();
	bool isPrecise{false};
	int index{1};
	auto interval{2000ms};
	while (index < argc) {
		const auto equalToThisArg{
			[&](std::string_view s) { return s == argv[index]; }};
		if (std::ranges::any_of(HELP_OPTIONS, equalToThisArg)) {
			showHelpAndExit();
		} else if (std::ranges::any_of(PRECISE_OPTIONS, equalToThisArg)) {
			isPrecise = true;
		} else
			break;
		index++;
	}
	std::signal(SIGINT, [](int signal) {
		std::println("\nSignal {}", signal);
		std::exit(0);
	});

	const std::string message{std::format(
		"Every {}s: {} ", static_cast<float>(interval.count()) / 1000,
		joinArguments(argv + index))};
	const auto start{std::chrono::steady_clock::now()};
#ifdef _WIN32
	timeBeginPeriod(1);
	std::atexit([]() { timeEndPeriod(1); });
#endif
	for (int count{1};; count++) {
		if (isPrecise)
			std::this_thread::sleep_until(start + interval * count);
		else
			std::this_thread::sleep_for(interval);
		std::println("\n{} {:L%c}", message, std::chrono::system_clock::now());
		execute(argv + index);
	}
}