#include <iostream>
#include <vector>
#include <string>
#include <functional>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestCase>& GetTestCases() {
    static std::vector<TestCase> testCases;
    return testCases;
}

void RegisterTest(const std::string& name, std::function<void()> func) {
    GetTestCases().push_back({name, func});
}

#define TEST(suite, name) \
    void test_##suite##_##name(); \
    struct Register_##suite##_##name { \
        Register_##suite##_##name() { \
            RegisterTest(#suite "." #name, test_##suite##_##name); \
        } \
    } g_reg_##suite##_##name; \
    void test_##suite##_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Assertion failed: " #expr); \
    }

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

#include <windows.h>

int wmain() {
    // Set console output to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    int passed = 0;
    int failed = 0;
    auto& tests = GetTestCases();

    std::cout << "Running " << tests.size() << " tests..." << std::endl;

    for (const auto& test : tests) {
        try {
            test.func();
            std::cout << "[PASS] " << test.name << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << test.name << " - " << e.what() << std::endl;
            failed++;
        } catch (...) {
            std::cout << "[FAIL] " << test.name << " - Unknown exception" << std::endl;
            failed++;
        }
    }

    std::cout << "------------------------------------" << std::endl;
    std::cout << "Tests completed: " << passed << " passed, " << failed << " failed." << std::endl;

    return (failed == 0) ? 0 : 1;
}
