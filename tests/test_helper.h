#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <stdexcept>
#include <windows.h>

void RegisterTest(const std::string& name, std::function<void()> func);

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
