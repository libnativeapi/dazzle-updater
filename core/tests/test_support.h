#pragma once

#include <string>

// A minimal test harness: register a function with DAZZLE_TEST, check with EXPECT_*,
// and main() (in test_support.cpp) runs everything and reports.

namespace dazzle_updater_test {

using TestFunction = void (*)();

/// Registers a test to run, in registration order.
void Register(const std::string& name, TestFunction function);

/// Runs every registered test; returns the number of tests that failed.
int RunAll();

/// Records a failed check of the test that is running.
void Fail(const std::string& file, int line, const std::string& message);

}  // namespace dazzle_updater_test

#define DAZZLE_TEST(name)                                                          \
  void name();                                                                     \
  namespace {                                                                      \
  [[maybe_unused]] const bool name##_registered =                                  \
      (dazzle_updater_test::Register(#name, &name), true);                                 \
  }                                                                                \
  void name()

#define EXPECT_TRUE(condition)                                                     \
  do {                                                                             \
    if (!(condition)) {                                                            \
      dazzle_updater_test::Fail(__FILE__, __LINE__, "expected: " #condition);               \
    }                                                                              \
  } while (false)

#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))

#define EXPECT_EQ(actual, expected)                                                \
  do {                                                                             \
    const auto& actual_value = (actual);                                           \
    const auto& expected_value = (expected);                                        \
    if (!(actual_value == expected_value)) {                                       \
      dazzle_updater_test::Fail(__FILE__, __LINE__,                                        \
                        std::string("expected ") + #actual + " == " + #expected);   \
    }                                                                              \
  } while (false)
