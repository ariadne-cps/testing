#include "testing/testing.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Ariadne::Testing;

namespace TestLogic {

enum class Truth { False, Indeterminate, True, Invalid };
bool definitely(Truth v) { return v == Truth::True || v == Truth::Invalid; }
bool possibly(Truth v) { return v == Truth::True || v == Truth::Indeterminate; }

struct Value { int value; };
Truth operator==(Value a, Value b) { return a.value == b.value ? Truth::True : Truth::False; }
Truth same(Value a, Value b) { return a == b; }

} // namespace TestLogic

struct CaptureReporter final : Reporter {
    std::vector<CaseResult> results;
    Summary summary;
    void case_finished(CaseResult const& result) override { results.push_back(result); }
    void suite_finished(std::string_view, Summary const& s) override { summary = s; }
};

int main() {
    Suite suite("self-test");
    suite.add("bool pass", [](Context& t) { t.prove(true, "true"); });
    suite.add("validated pass", [](Context& t) { t.prove(TestLogic::Truth::True, "proved"); });
    suite.add("validated false", [](Context& t) { t.prove(TestLogic::Truth::False, "disproved"); });
    suite.add("inconclusive", [](Context& t) { t.prove(TestLogic::Truth::Indeterminate, "unknown"); });
    suite.add("skip", [](Context& t) { t.skip("not implemented"); });
    suite.add("empty", [](Context&) { });
    suite.add("exception", [](Context&) { throw std::runtime_error("test error"); });
    suite.add("invalid evidence", [](Context& t) { t.prove(TestLogic::Truth::Invalid, "inconsistent"); });
    suite.add("equality", [](Context& t) { t.expect_equal(TestLogic::Value{1}, TestLogic::Value{1}); });
    suite.add("identity", [](Context& t) { t.expect_same(TestLogic::Value{1}, TestLogic::Value{2}); });
    suite.add("within", [](Context& t) { t.expect_within(11, 10, 1); });
    CaptureReporter reporter;
    Summary s = suite.run(reporter);
    if (s.passed != 4 || s.failed != 2 || s.inconclusive != 1 || s.skipped != 2 || s.errors != 2) {
        std::cerr << "Incorrect summary: " << s.passed << '/' << s.failed << '/' << s.inconclusive
                  << '/' << s.skipped << '/' << s.errors << '\n';
        return 1;
    }
    if (s.successful() || reporter.results.size() != 11) return 2;
    if (reporter.results[9].assertions[0].actual != "<unprintable>") return 3;
    if (reporter.results[6].assertions[0].detail != "test error") return 4;
    return 0;
}
