#include "test_framework.h"
#include "../src/core/app_state.h"
#include "../src/logic/word_lists.h"
#include <cstdio>
#include <exception>

AppState g_state;

namespace Tests {

static int s_failureCount = 0;

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void RecordFailure(const char* file, int line, const std::string& expression) {
    s_failureCount++;
    std::printf("    FAILED %s:%d: %s\n", file, line, expression.c_str());
}

}

int main() {
    WordLists::LoadAll();

    int failedTestCount = 0;
    for (const Tests::TestCase& test : Tests::Registry()) {
        const int failuresBefore = Tests::s_failureCount;
        try {
            test.run();
        } catch (const std::exception& error) {
            Tests::RecordFailure(__FILE__, __LINE__,
                                 std::string("unexpected exception: ") + error.what());
        } catch (...) {
            Tests::RecordFailure(__FILE__, __LINE__, "unexpected exception");
        }
        const bool passed = Tests::s_failureCount == failuresBefore;
        if (!passed)
            failedTestCount++;
        std::printf("%s  %s\n", passed ? "PASS" : "FAIL", test.name);
    }

    std::printf("\n%zu tests, %d failed\n", Tests::Registry().size(), failedTestCount);
    return failedTestCount == 0 ? 0 : 1;
}
