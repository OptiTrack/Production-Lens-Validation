#include "test_support.h"

#include <cstring>
#include <iostream>

namespace testing {

std::vector<TestCase>& Registry() {
	// Function local static: safe regardless of static initialization order.
	static std::vector<TestCase> registry;
	return registry;
}

Registrar::Registrar(std::string name, std::function<void()> fn) {
	Registry().push_back(TestCase{std::move(name), std::move(fn)});
}

void ReportFailure(const char* file, int line, const std::string& message) {
	std::ostringstream out;
	out << file << ":" << line << ": " << message;
	throw CheckFailure{out.str()};
}

int RunAll(int argc, char** argv) {
	std::string filter;
	bool listOnly = false;

	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--list") == 0) {
			listOnly = true;
		} else if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
			filter = argv[++i];
		} else {
			std::cerr << "usage: " << argv[0] << " [--list] [--filter <name>]\n";
			return 2;
		}
	}

	if (listOnly) {
		for (const auto& test : Registry()) {
			std::cout << test.name << "\n";
		}
		return 0;
	}

	int passed = 0;
	int failed = 0;
	int selected = 0;

	for (const auto& test : Registry()) {
		if (!filter.empty() && test.name.find(filter) == std::string::npos) {
			continue;
		}
		++selected;

		try {
			test.fn();
			std::cout << "[ PASS ] " << test.name << "\n";
			++passed;
		} catch (const CheckFailure& failure) {
			std::cout << "[ FAIL ] " << test.name << "\n"
			          << "         " << failure.message << "\n";
			++failed;
		} catch (const std::exception& ex) {
			std::cout << "[ FAIL ] " << test.name
			          << "\n         unexpected exception: " << ex.what() << "\n";
			++failed;
		} catch (...) {
			std::cout << "[ FAIL ] " << test.name
			          << "\n         unexpected non-standard exception\n";
			++failed;
		}
	}

	if (selected == 0) {
		std::cerr << "error: no test matched";
		if (!filter.empty()) {
			std::cerr << " filter '" << filter << "'";
		}
		std::cerr << " (" << Registry().size() << " tests registered)\n";
		return 2;
	}

	std::cout << "\n" << passed << " passed, " << failed << " failed, "
	          << selected << " selected of " << Registry().size()
	          << " registered.\n";

	return failed == 0 ? 0 : 1;
}

}  // namespace testing

int main(int argc, char** argv) { return testing::RunAll(argc, argv); }
