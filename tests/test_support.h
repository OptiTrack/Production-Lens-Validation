#pragma once

// ---------------------------------------------------------------------------
// Minimal self-contained unit test harness.
//
// Deliberately dependency free: CI can build and run the suite on any platform
// with just a C++17 compiler and CMake, with no package manager or network
// download involved. Tests are declared with TEST(name) and register
// themselves; the runner in test_support.cpp executes them.
//
//   TEST(my_thing_works) {
//       CHECK_EQ(2 + 2, 4);
//   }
//
// A failing CHECK aborts that test only; the remaining tests still run.
// ---------------------------------------------------------------------------

#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
	std::string name;
	std::function<void()> fn;
};

/// All registered tests, in declaration order.
std::vector<TestCase>& Registry();

/// Registers a test at static initialization time.
struct Registrar {
	Registrar(std::string name, std::function<void()> fn);
};

/// Thrown by a failing check to abort the current test.
struct CheckFailure {
	std::string message;
};

[[noreturn]] void ReportFailure(const char* file, int line, const std::string& message);

/// Renders a value for failure messages, falling back to a placeholder for
/// types that have no operator<<.
template <typename T>
std::string Describe(const T& value) {
	std::ostringstream out;
	out << value;
	return out.str();
}

inline std::string Describe(bool value) { return value ? "true" : "false"; }

/// Entry point used by the test executable. Supported arguments:
///   --list            print every registered test name
///   --filter <name>   run only tests whose name contains <name>
/// Returns 0 when every selected test passed, 1 on failure, 2 when the filter
/// selected nothing (which catches stale test names in CMake).
int RunAll(int argc, char** argv);

}  // namespace testing

#define TEST(test_name)                                                    \
	static void test_name();                                               \
	static const ::testing::Registrar test_registrar_##test_name{          \
	    #test_name, test_name};                                            \
	static void test_name()

#define CHECK_TRUE(expr)                                                   \
	do {                                                                   \
		if (!(expr)) {                                                     \
			::testing::ReportFailure(__FILE__, __LINE__,                   \
			                         "expected true: " #expr);             \
		}                                                                  \
	} while (false)

#define CHECK_FALSE(expr)                                                  \
	do {                                                                   \
		if ((expr)) {                                                      \
			::testing::ReportFailure(__FILE__, __LINE__,                   \
			                         "expected false: " #expr);            \
		}                                                                  \
	} while (false)

#define CHECK_EQ(actual, expected)                                         \
	do {                                                                   \
		const auto& check_actual_ = (actual);                              \
		const auto& check_expected_ = (expected);                          \
		if (!(check_actual_ == check_expected_)) {                          \
			::testing::ReportFailure(                                      \
			    __FILE__, __LINE__,                                        \
			    std::string(#actual) + " == " + #expected + "\n    actual:   " + \
			        ::testing::Describe(check_actual_) +                   \
			        "\n    expected: " + ::testing::Describe(check_expected_)); \
		}                                                                  \
	} while (false)

#define CHECK_NEAR(actual, expected, tolerance)                            \
	do {                                                                   \
		const double check_actual_ = static_cast<double>(actual);           \
		const double check_expected_ = static_cast<double>(expected);      \
		const double check_tol_ = static_cast<double>(tolerance);           \
		if (!(std::fabs(check_actual_ - check_expected_) <= check_tol_)) {  \
			::testing::ReportFailure(                                      \
			    __FILE__, __LINE__,                                        \
			    std::string(#actual) + " ~= " + #expected +                \
			        "\n    actual:    " + ::testing::Describe(check_actual_) + \
			        "\n    expected:  " + ::testing::Describe(check_expected_) + \
			        "\n    tolerance: " + ::testing::Describe(check_tol_)); \
		}                                                                  \
	} while (false)

#define CHECK_LE(lhs, rhs)                                                 \
	do {                                                                   \
		const auto& check_lhs_ = (lhs);                                    \
		const auto& check_rhs_ = (rhs);                                    \
		if (!(check_lhs_ <= check_rhs_)) {                                 \
			::testing::ReportFailure(                                      \
			    __FILE__, __LINE__,                                        \
			    std::string(#lhs) + " <= " + #rhs + "\n    lhs: " +        \
			        ::testing::Describe(check_lhs_) + "\n    rhs: " +      \
			        ::testing::Describe(check_rhs_));                      \
		}                                                                  \
	} while (false)

#define CHECK_GT(lhs, rhs)                                                 \
	do {                                                                   \
		const auto& check_lhs_ = (lhs);                                    \
		const auto& check_rhs_ = (rhs);                                    \
		if (!(check_lhs_ > check_rhs_)) {                                   \
			::testing::ReportFailure(                                      \
			    __FILE__, __LINE__,                                        \
			    std::string(#lhs) + " > " + #rhs + "\n    lhs: " +         \
			        ::testing::Describe(check_lhs_) + "\n    rhs: " +      \
			        ::testing::Describe(check_rhs_));                      \
		}                                                                  \
	} while (false)
