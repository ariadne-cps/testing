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
#include <future>
#include <mutex>
#include <condition_variable>
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

Truth ordered(Value a, Value b) {
    if (a.value == 0 || b.value == 0) return Truth::Indeterminate;
    return a.value < b.value ? Truth::True : Truth::False;
}
Truth positive(Value v) {
    if (v.value == 0) return Truth::Indeterminate;
    return v.value > 0 ? Truth::True : Truth::False;
}
Truth operator<(Value a, Value b) { return ordered(a,b); }

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
    ARIADNE_ADD_TEST_TO(fixture_suite, FixtureExample, test_first);
    ARIADNE_ADD_TEST_TO(fixture_suite, FixtureExample, test_second);
    ARIADNE_ADD_TEST_NO_THROW_TO(fixture_suite, FixtureExample, test_print_only);
    ARIADNE_ADD_TEST_NO_THROW_TO(fixture_suite, FixtureExample, test_throws);
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
        ARIADNE_PROVE_CTX(t, TestLogic::Truth::True);
        ARIADNE_EXPECT_EQUAL_CTX(t, ++evaluation_count, 1);
        ARIADNE_EXPECT_SAME_CTX(t, TestLogic::Value{2}, TestLogic::Value{2});
        ARIADNE_EXPECT_WITHIN_CTX(t, 11, 10, 1);
        ARIADNE_EXPECT_THROWS_CTX(t, std::runtime_error, throw std::runtime_error("expected"));
        ARIADNE_EXPECT_EXCEPTION_CTX(t, throw 42);
        ARIADNE_EXPECT_NO_THROW_CTX(t, (void)0);
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
        ARIADNE_EXPECT_EQUAL_CTX(t, ([&]() {
            if (ordering_output.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::runtime_error("CHECK event missing before operand evaluation");
            return 2;
        }()), 2);
    });
    ConsoleReporter ordering_reporter(ordering_output);
    if (!ordering.run(ordering_reporter).successful()) return 38;
    // D01: relational comparisons preserve built-in signedness and ADL evidence.
    Suite relations("relations");
    CaptureReporter relation_capture;
    relations.add("built-in relations", [](Context& t) {
        ARIADNE_EXPECT_NOT_EQUAL_CTX(t, -1, static_cast<std::size_t>(-1));
        ARIADNE_EXPECT_LESS_CTX(t, -1, 0u);
        ARIADNE_EXPECT_LESS_EQUAL_CTX(t, -1, 0u);
        ARIADNE_EXPECT_GREATER_CTX(t, 0u, -1);
        ARIADNE_EXPECT_GREATER_EQUAL_CTX(t, 0u, -1);
        ARIADNE_EXPECT_LESS_CTX(t, 7u, 9);
        ARIADNE_EXPECT_GREATER_EQUAL_CTX(t, 9u, 9);
    });
    relations.add("generic ADL", [](Context& t) {
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{1}, TestLogic::Value{2});
        ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{2}, TestLogic::Value{1});
        ARIADNE_EXPECT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{3});
        ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{-3});
        ARIADNE_EXPECT_LESS_CTX(t, TestLogic::Value{1}, TestLogic::Value{2});
    });
    relations.add("indeterminate remains indeterminate", [](Context& t) {
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{0}, TestLogic::Value{1});
        ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{0}, TestLogic::Value{1});
        ARIADNE_EXPECT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{0});
        ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{0});
        ARIADNE_EXPECT_LESS_CTX(t, TestLogic::Value{0}, TestLogic::Value{1});
    });
    relations.add("failed relation keeps operands", [](Context& t) {
        ARIADNE_EXPECT_LESS_CTX(t, 5, 3);
        ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{1}, TestLogic::Value{2});
    });
    auto const relation_summary = relations.run(relation_capture);
    if (relation_summary.passed != 2 || relation_summary.inconclusive != 1 ||
        relation_summary.failed != 1 || relation_summary.errors != 0) return 40;
    if (relation_capture.results.size() != 4) return 41;
    auto const& indeterminate = relation_capture.results[2];
    if (indeterminate.assertions.size() != 5) return 42;
    for (auto const& assertion : indeterminate.assertions)
        if (assertion.outcome != Outcome::Inconclusive) return 43;
    auto const& failures = relation_capture.results[3].assertions;
    if (failures.size() != 2 || failures[0].actual != "5" ||
        failures[0].expected != "3" || failures[0].description != "5 < 3") return 44;
    if (failures[1].description.find("not ordered(") != 0 ||
        failures[1].outcome != Outcome::Failed) return 45;

    Suite evaluation("single evaluation");
    std::ostringstream live_comparisons;
    int order = 0;
    evaluation.add("left, right, predicate", [&](Context& t) {
        ARIADNE_EXPECT_LESS_CTX(t, ([&] {
            if (order++ != 0 || live_comparisons.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::logic_error("left operand evaluated before CHECK");
            return 1;
        }()), ([&] {
            if (order++ != 1) throw std::logic_error("wrong operand order");
            return 2;
        }()));
        t.check_binary([&](int left, int right) {
            if (order++ != 4) throw std::logic_error("predicate timing");
            return left == right;
        }, [&] {
            if (order++ != 2 || live_comparisons.str().find("[CHECK 1.2]") == std::string::npos)
                throw std::logic_error("left predicate operand before CHECK");
            return 3;
        }, [&] {
            if (order++ != 3) throw std::logic_error("wrong predicate argument order");
            return 3;
        }, "ordered predicate");
    });
    ConsoleReporter live_relation_reporter(live_comparisons);
    if (!evaluation.run(live_relation_reporter).successful() || order != 5) return 46;
    if (live_comparisons.str().find("[CHECK 1.2] ordered predicate") == std::string::npos) return 47;
    // D02: an expected indeterminate value must not become an inconclusive test.
    Suite expectations("evidence expectations");
    CaptureReporter expected_capture;
    expectations.add("exact expectations", [](Context& t) {
        ARIADNE_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::True);
        ARIADNE_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::True);
        ARIADNE_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::Indeterminate);
        ARIADNE_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::False);
        ARIADNE_EXPECT_INDETERMINATE_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("failed and inconclusive proof", [](Context& t) {
        ARIADNE_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::False);
        ARIADNE_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("inconclusive refutation", [](Context& t) {
        ARIADNE_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("false possibility and wrong expectation", [](Context& t) {
        ARIADNE_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::False);
        ARIADNE_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::True);
        ARIADNE_EXPECT_INDETERMINATE_CTX(t, TestLogic::Truth::True);
    });
    auto const expected_sum = expectations.run(expected_capture);
    if (expected_sum.passed != 1 || expected_sum.failed != 2 ||
        expected_sum.inconclusive != 1 || expected_sum.errors != 0) return 48;
    if (expected_capture.results.size() != 4) return 49;
    auto const& evidence_pass = expected_capture.results[0].assertions;
    if (evidence_pass.size() != 5) return 50;
    for (auto const& assertion : evidence_pass)
        if (assertion.outcome != Outcome::Passed) return 51;
    if (evidence_pass[2].actual != "indeterminate" ||
        evidence_pass[2].expected != "possibly true") return 52;
    if (expected_capture.results[1].assertions[1].outcome != Outcome::Inconclusive)
        return 53;
    if (expected_capture.results[2].assertions[0].outcome != Outcome::Inconclusive)
        return 54;
    if (expected_capture.results[3].assertions[0].actual != "false") return 55;
    if (expected_capture.results[0].assertions[3].expected != "not possibly true") return 56;
    if (expected_capture.results[2].assertions[0].expected != "not possibly true") return 57;
    if (expected_capture.results[3].assertions.size() != 3 ||
        expected_capture.results[3].assertions[1].actual != "true" ||
        expected_capture.results[3].assertions[1].outcome != Outcome::Failed) return 58;
    Suite gate_cases("exit policy");
    CaptureReporter gate_reporter;
    gate_cases.add("valid proof", [](Context& c) { c.prove(true, "certain"); });
    gate_cases.add("unproved assertion", [](Context& c) {
        c.prove(TestLogic::Truth::Indeterminate, "not decided");
    });
    gate_cases.add("not executed", [](Context& c) { c.skip("environment unavailable"); });
    auto const gate_summary = gate_cases.run(gate_reporter);
    if (gate_summary.passed != 1 || gate_summary.inconclusive != 1 ||
        gate_summary.skipped != 1 || gate_summary.successful()) return 59;
    if (gate_summary.acceptable({})) return 60;
    AcceptancePolicy permit_inconclusive;
    permit_inconclusive.inconclusive = InconclusivePolicy::NonBlocking;
    if (!gate_summary.acceptable(permit_inconclusive)) return 61;
    permit_inconclusive.skipped = SkippedPolicy::Blocking;
    if (gate_summary.acceptable(permit_inconclusive)) return 62;
    if (gate_reporter.results[1].outcome != Outcome::Inconclusive ||
        gate_reporter.results[2].outcome != Outcome::Skipped) return 63;
    Summary only_inconclusive;
    only_inconclusive.inconclusive = 1;
    if (only_inconclusive.acceptable(permit_inconclusive)) return 64;
    Summary with_failure = gate_summary;
    with_failure.failed = 1;
    if (with_failure.acceptable({InconclusivePolicy::NonBlocking, SkippedPolicy::NonBlocking})) return 65;
    Summary with_error = gate_summary;
    with_error.errors = 1;
    if (with_error.acceptable({InconclusivePolicy::NonBlocking, SkippedPolicy::NonBlocking})) return 66;
    // A successfully checked solver status UNKNOWN is not test Inconclusive.
    enum class SolverState { Sat, Unsat, Unknown };
    enum class UnknownReason { None, ResourceExhausted };
    Suite solver_states("solver states");
    CaptureReporter solver_state_reporter;
    solver_states.add("expected UNKNOWN due to budget", [](Context& c) {
        auto const state = SolverState::Unknown;
        auto const reason = UnknownReason::ResourceExhausted;
        c.expect_equal(state, SolverState::Unknown, "solver returned UNKNOWN");
        c.expect_equal(reason, UnknownReason::ResourceExhausted, "reason is resource exhaustion");
    });
    auto const solver_summary = solver_states.run(solver_state_reporter);
    if (!solver_summary.successful() || solver_state_reporter.results.size() != 1 ||
        solver_state_reporter.results[0].assertions.size() != 2 ||
        solver_state_reporter.results[0].assertions[0].outcome != Outcome::Passed) return 67;
    Suite diagnostics("diagnostics");
    std::ostringstream diag_output;
    diagnostics.add("nested oracle", [&](Context& c) {
        auto file_scope = c.scope("oracle", "floatmp_rounding.tsv");
        {
            auto row_scope = c.scope("row", 21);
            if (diag_output.str().find("[SCOPE 1] oracle=floatmp_rounding.tsv / row=21") == std::string::npos)
                throw std::runtime_error("scope not flushed before work");
            c.note("input prepared");
            c.inspect("expected", 42);
            c.warning("reference is approximate");
            c.measure("calculation", [&] { c.prove(false, "deliberate failure"); });
        }
        c.prove(true, "restored to file scope");
    });
    diagnostics.add("unwinding", [](Context& c) {
        auto outer = c.scope("phase=outer");
        auto inner = c.scope("row=error");
        throw std::runtime_error("intentional nested error");
    });
    diagnostics.add("scope restored across cases", [](Context& c) {
        c.prove(true, "new case has no labels");
    });
    ConsoleReporter diag_reporter(diag_output, ConsoleDetail::Concise);
    auto const diag_summary = diagnostics.run(diag_reporter);
    if (diag_summary.failed != 1 || diag_summary.errors != 1 ||
        diag_summary.passed != 1) return 68;
    if (diag_output.str().find("[WARN] oracle=floatmp_rounding.tsv / row=21 / reference is approximate") ==
        std::string::npos) return 69;
    if (diag_output.str().find("context: oracle=floatmp_rounding.tsv / row=21") ==
        std::string::npos) return 70;
    if (diag_output.str().find("diagnostic: input prepared") == std::string::npos ||
        diag_output.str().find("diagnostic: calculation") == std::string::npos) return 71;
    if (diag_output.str().find("context: phase=outer / row=error") == std::string::npos) return 72;
    if (diag_output.str().find("[CHECK 3.1] new case has no labels") == std::string::npos) return 73;
    Suite bounded_waits("bounded waits");
    CaptureReporter bounded_reporter;
    bounded_waits.add("ready future and immediate condition", [](Context& c) {
        std::promise<int> p;
        auto f = p.get_future();
        p.set_value(42);
        c.expect_ready(f, std::chrono::milliseconds(10), "future ready");
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually(cv, lock, [] { return true; },
                            std::chrono::milliseconds(10), "predicate already true");
    });
    bounded_waits.add("future timeout and deferred", [](Context& c) {
        std::promise<int> p;
        auto f = p.get_future();
        c.expect_ready(f, std::chrono::milliseconds(0), "future not ready");
        auto deferred = std::async(std::launch::deferred, [] { return 3; });
        c.expect_ready(deferred, std::chrono::milliseconds(0), "deferred is not ready");
    });
    bounded_waits.add("condition timeout", [](Context& c) {
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually(cv, lock, [] { return false; },
                            std::chrono::milliseconds(0), "condition never true");
    });
    auto const bounded_summary = bounded_waits.run(bounded_reporter);
    if (bounded_summary.passed != 1 || bounded_summary.failed != 2 ||
        bounded_summary.errors != 0 || bounded_reporter.results.size() != 3) return 74;
    if (bounded_reporter.results[1].assertions[0].actual != "timeout" ||
        bounded_reporter.results[1].assertions[1].actual != "deferred") return 75;
    if (bounded_reporter.results[2].assertions[0].actual != "timeout") return 76;
    // Exercise the compact one-suite entry point and explicit fixture macros.
    struct CompactFixture : ContextFixture {
        using ContextFixture::ContextFixture;
        void test_compact() {
            int evaluations = 0;
            ARIADNE_EXPECT_EQUAL(++evaluations, 1);
            ARIADNE_PROVE(evaluations == 1);
            ARIADNE_EXPECT_NO_THROW((void)evaluations);
        }
        void test_no_throw() { ARIADNE_EXPECT_GREATER(2, 1); }
    };
    {
        ARIADNE_TEST_SUITE("compact");
        ARIADNE_ADD_TEST(CompactFixture, test_compact);
        ARIADNE_ADD_TEST_NO_THROW(CompactFixture, test_no_throw);
        std::ostringstream compact_output;
        if (::Ariadne::Testing::run_suite(ariadne_test_suite, compact_output) != 0) return 77;
        if (compact_output.str().find("[PASS] compact") == std::string::npos) return 78;
    }
    return 0;
}
