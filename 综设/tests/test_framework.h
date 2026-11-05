#pragma once

#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lltest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

std::vector<TestCase>& Registry();

struct Registrar {
    Registrar(std::string name, std::function<void()> fn);
};

class Failure : public std::exception {
public:
    explicit Failure(std::string message) : message_(std::move(message)) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

void Check(bool condition, const char* expression, const char* file, int line);
int  RunAll();

}  // namespace lltest

#define LL_TEST(name)                                                     \
    static void ll_test_##name();                                         \
    static ::lltest::Registrar ll_reg_##name(#name, &ll_test_##name);     \
    static void ll_test_##name()

#define CHECK(expression)                                                 \
    ::lltest::Check(static_cast<bool>(expression), #expression, __FILE__, \
                    __LINE__)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

#define REQUIRE(expression)                                               \
    do {                                                                  \
        if (!static_cast<bool>(expression)) {                             \
            std::ostringstream oss;                                       \
            oss << __FILE__ << ":" << __LINE__                            \
                << " REQUIRE failed: " #expression;                       \
            throw ::lltest::Failure(oss.str());                           \
        }                                                                 \
    } while (false)
