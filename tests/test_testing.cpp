/***************************************************************************
 *            test_testing.cpp
 *
 *  Copyright  2026  Luca Geretti
 *  Author     Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Testing.
 *
 *  Ariadne Testing is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Testing is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Testing. If not, see <https://www.gnu.org/licenses/>.
 */

#include "testing/fixture.hpp"
#include "testing/checks.hpp"

#include <iostream>
#include <stdexcept>
#include <sstream>
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
    std::ostringstream output;
    ConsoleReporter console(output);
    Suite rendered("reporting");
    rendered.add("success", [](Context& t) { t.prove(true, "established"); });
    rendered.add("failure", [](Context& t) { t.expect_equal(2, 3, "numbers differ"); });
    Summary const rendered_summary = rendered.run(console);
    if (rendered_summary.passed != 1 || rendered_summary.failed != 1) return 5;
    std::string const report = output.str();
    if (report.find("[PASS] success") == std::string::npos) return 6;
    if (report.find("[FAIL] failure") == std::string::npos) return 7;
    if (report.find("actual: 2") == std::string::npos) return 8;
    if (report.find("expected: 3") == std::string::npos) return 9;
    if (report.find("reporting: 1 passed, 1 failed") == std::string::npos) return 10;
    Suite exceptions("exceptions");
    exceptions.add("correct type", [](Context& t) { t.expect_throws<std::runtime_error>([] { throw std::runtime_error("yes"); }); });
    exceptions.add("wrong type", [](Context& t) { t.expect_throws<std::runtime_error>([] { throw std::logic_error("wrong"); }); });
    exceptions.add("missing", [](Context& t) { t.expect_exception([] {}); });
    exceptions.add("any type", [](Context& t) { t.expect_exception([] { throw 42; }); });
    exceptions.add("no throw", [](Context& t) { t.expect_no_throw([] {}); });
    exceptions.add("unexpected", [](Context& t) { t.expect_no_throw([] { throw 42; }); });
    CaptureReporter exceptions_reporter;
    Summary es = exceptions.run(exceptions_reporter);
    if (es.passed != 3 || es.failed != 3 || es.errors != 0) return 11;
    Suite mixed_signs("mixed signedness");
    mixed_signs.add("same positive", [](Context& t) {
        t.expect_equal(std::size_t{2}, 2, "size_t versus int");
        t.expect_equal(2, std::size_t{2}, "int versus size_t");
    });
    mixed_signs.add("negative must not equal max unsigned", [](Context& t) {
        t.expect_equal(-1, static_cast<std::size_t>(-1), "negative signed versus max unsigned");
    });
    CaptureReporter sign_reporter;
    Summary const sign_summary = mixed_signs.run(sign_reporter);
    if (sign_summary.passed != 1 || sign_summary.failed != 1) return 15;
    if (sign_reporter.results.size() != 2 ||
        sign_reporter.results[1].outcome != Outcome::Failed) return 16;
    Suite progress("progress");
    std::ostringstream live;
    ConsoleReporter live_reporter(live);
    progress.add("first", [&](Context& t) {
        if (live.str().find("[RUN 1/2] progress / first") == std::string::npos)
            throw std::runtime_error("missing case-start event");
        t.check([&] {
            if (live.str().find("[CHECK 1.1] deferred predicate") == std::string::npos)
                throw std::runtime_error("checker event not flushed before predicate");
            return true;
        }, "deferred predicate");
        t.prove(true, "second assertion");
    });
    progress.add("second", [](Context& t) { t.prove(true, "reset counter"); });
    auto const ps = progress.run(live_reporter);
    if (ps.passed != 2 || ps.errors != 0) return 17;
    if (live.str().find("[CHECK 1.2] second assertion") == std::string::npos) return 18;
    if (live.str().find("[RUN 2/2] progress / second") == std::string::npos) return 19;
    if (live.str().find("[CHECK 2.1] reset counter") == std::string::npos) return 20;
    struct FixtureExample : ContextFixture {
        using ContextFixture::ContextFixture;
        void test_first() {
            ctx().prove(true, "first check");
            ctx().expect_equal(3, 3, "second check");
        }
        void test_second() { ctx().prove(true, "reset check count"); }
        void test_print_only() { }
        void test_throws() { throw std::runtime_error("expected to be caught"); }
    };
    Suite fixture_suite("fixtures");
    ARIADNE_ADD_TEST(fixture_suite, FixtureExample, test_first);
    ARIADNE_ADD_TEST(fixture_suite, FixtureExample, test_second);
    ARIADNE_ADD_TEST_NO_THROW(fixture_suite, FixtureExample, test_print_only);
    ARIADNE_ADD_TEST_NO_THROW(fixture_suite, FixtureExample, test_throws);
    std::ostringstream fixture_out;
    ConsoleReporter fixture_reporter(fixture_out);
    auto const fixture_summary = fixture_suite.run(fixture_reporter);
    if (fixture_summary.passed != 3 || fixture_summary.failed != 1 ||
        fixture_summary.errors != 0) return 25;
    auto const fixture_log = fixture_out.str();
    if (fixture_log.find("[RUN 1/4] fixtures / first") == std::string::npos) return 26;
    if (fixture_log.find("[CHECK 1.2] second check") == std::string::npos) return 27;
    if (fixture_log.find("[RUN 2/4] fixtures / second") == std::string::npos) return 28;
    if (fixture_log.find("[CHECK 2.1] reset check count") == std::string::npos) return 29;
    if (fixture_log.find("[CHECK 3.1] operation completes") == std::string::npos) return 30;
    if (fixture_log.find("[FAIL] throws") == std::string::npos) return 31;
    Suite expressions("macro expressions");
    std::ostringstream expression_output;
    int evaluation_count = 0;
    expressions.add("all macros", [&](Context& t) {
        ARIADNE_PROVE(t, TestLogic::Truth::True);
        ARIADNE_EXPECT_EQUAL(t, ++evaluation_count, 1);
        ARIADNE_EXPECT_SAME(t, TestLogic::Value{2}, TestLogic::Value{2});
        ARIADNE_EXPECT_WITHIN(t, 11, 10, 1);
        ARIADNE_EXPECT_THROWS(t, std::runtime_error, throw std::runtime_error("expected"));
        ARIADNE_EXPECT_EXCEPTION(t, throw 42);
        ARIADNE_EXPECT_NO_THROW(t, (void)0);
    });
    ConsoleReporter expression_reporter(expression_output);
    auto const expression_summary = expressions.run(expression_reporter);
    if (expression_summary.passed != 1 || evaluation_count != 1) return 32;
    auto const expression_log = expression_output.str();
    if (expression_log.find("[CHECK 1.1] TestLogic::Truth::True") == std::string::npos) return 33;
    if (expression_log.find("[CHECK 1.2] ++evaluation_count == 1") == std::string::npos) return 34;
    if (expression_log.find("[CHECK 1.3] TestLogic::Value{2} same TestLogic::Value{2}") == std::string::npos) return 35;
    if (expression_log.find("[CHECK 1.4] 11 within 1 of 10") == std::string::npos) return 36;
    if (expression_log.find("[CHECK 1.7] (void)0 does not throw") == std::string::npos) return 37;
    Suite ordering("deferred evaluation");
    std::ostringstream ordering_output;
    ordering.add("start precedes operands", [&](Context& t) {
        ARIADNE_EXPECT_EQUAL(t, ([&]() {
            if (ordering_output.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::runtime_error("CHECK event missing before operand evaluation");
            return 2;
        }()), 2);
    });
    ConsoleReporter ordering_reporter(ordering_output);
    if (!ordering.run(ordering_reporter).successful()) return 38;
    return 0;
}
