#pragma once
#include <string>
#include <vector>

namespace Tests {

struct TestCase {
    const char* name;
    void (*run)();
};

std::vector<TestCase>& Registry();

struct Registrar {
    Registrar(const char* name, void (*run)()) { Registry().push_back({name, run}); }
};

void RecordFailure(const char* file, int line, const std::string& expression);

}

#define TEST(name)                                                 \
    static void name();                                            \
    static const Tests::Registrar name##Registrar(#name, name);    \
    static void name()

#define CHECK(condition)                                           \
    do {                                                           \
        if (!(condition))                                          \
            Tests::RecordFailure(__FILE__, __LINE__, #condition);  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        if (!((actual) == (expected)))                                     \
            Tests::RecordFailure(__FILE__, __LINE__,                       \
                                 std::string(#actual " == " #expected));   \
    } while (0)

#define CHECK_THROWS(statement)                                    \
    do {                                                           \
        bool threw = false;                                        \
        try {                                                      \
            statement;                                             \
        } catch (...) {                                            \
            threw = true;                                          \
        }                                                          \
        if (!threw)                                                \
            Tests::RecordFailure(__FILE__, __LINE__,               \
                                 std::string(#statement " throws")); \
    } while (0)
