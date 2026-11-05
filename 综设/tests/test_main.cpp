#include "test_framework.h"

#include <iostream>

namespace lltest {
namespace {
int g_failures = 0;
}

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

Registrar::Registrar(std::string name, std::function<void()> fn) {
    Registry().push_back(TestCase{std::move(name), std::move(fn)});
}

void Check(bool condition, const char* expression, const char* file, int line) {
    if (condition) return;
    ++g_failures;
    std::cerr << "  FAIL " << file << ":" << line << "  " << expression << "\n";
}

int RunAll() {
    int failed_tests = 0;
    for (const TestCase& test : Registry()) {
        const int before = g_failures;
        std::cout << "[ RUN      ] " << test.name << "\n";
        try {
            test.fn();
        } catch (const Failure& e) {
            ++g_failures;
            std::cerr << "  FAIL " << e.what() << "\n";
        } catch (const std::exception& e) {
            ++g_failures;
            std::cerr << "  FAIL unexpected exception: " << e.what() << "\n";
        } catch (...) {
            ++g_failures;
            std::cerr << "  FAIL unknown exception\n";
        }
        if (g_failures != before) {
            ++failed_tests;
            std::cout << "[  FAILED  ] " << test.name << "\n";
        } else {
            std::cout << "[       OK ] " << test.name << "\n";
        }
    }

    std::cout << "\n" << Registry().size() << " tests, " << failed_tests
              << " failed, " << g_failures << " checks failed\n";
    return failed_tests == 0 ? 0 : 1;
}

}  // namespace lltest

int main() { return lltest::RunAll(); }
