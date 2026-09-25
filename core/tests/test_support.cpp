#include "test_support.h"

#include <cstdio>
#include <string>
#include <vector>

namespace dazzle_test {
namespace {

struct Test {
  std::string name;
  TestFunction function;
};

std::vector<Test>& Tests() {
  static std::vector<Test> tests;
  return tests;
}

std::vector<std::string>& Failures() {
  static std::vector<std::string> failures;
  return failures;
}

std::string& RunningTest() {
  static std::string name;
  return name;
}

}  // namespace

void Register(const std::string& name, TestFunction function) {
  Tests().push_back({name, function});
}

void Fail(const std::string& file, int line, const std::string& message) {
  Failures().push_back(RunningTest() + ": " + file + ":" + std::to_string(line) + ": " + message);
}

int RunAll() {
  int failed_tests = 0;
  for (const Test& test : Tests()) {
    RunningTest() = test.name;
    const size_t before = Failures().size();
    test.function();
    if (Failures().size() == before) {
      std::printf("ok   %s\n", test.name.c_str());
    } else {
      ++failed_tests;
      std::printf("FAIL %s\n", test.name.c_str());
      for (size_t index = before; index < Failures().size(); ++index) {
        std::printf("       %s\n", Failures()[index].c_str());
      }
    }
  }
  std::printf("%d test(s), %d failed, %d failed check(s)\n", static_cast<int>(Tests().size()),
              failed_tests, static_cast<int>(Failures().size()));
  return failed_tests;
}

}  // namespace dazzle_test

int main() {
  return dazzle_test::RunAll() == 0 ? 0 : 1;
}
