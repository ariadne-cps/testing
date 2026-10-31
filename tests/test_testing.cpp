/***************************************************************************
 *            test_testing.cpp
 *
 *  Copyright  2026  Luca Geretti
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

#include "ariadne-testing.hpp"

#include <iostream>
#include <future>
#include <functional>
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
        void test_const_context() {
            FixtureExample const& self = *this;
            ctx().prove(&self.ctx() == &ctx(),
                        "const fixture accessor refers to the active context");
        }
    };
    Suite fixture_suite("fixtures");
    ARIADNE_TESTING_ADD_TEST_TO(fixture_suite, FixtureExample, test_first);
    ARIADNE_TESTING_ADD_TEST_TO(fixture_suite, FixtureExample, test_second);
    ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(fixture_suite, FixtureExample, test_print_only);
    ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(fixture_suite, FixtureExample, test_throws);
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

    // Exercise explicit fixture labels without the test_ prefix using the
    // same FixtureExample instantiation as the existing prefix cases.
    Suite named_fixture("explicit fixture naming");
    add_fixture_test<FixtureExample>(named_fixture, "custom_label",
        &FixtureExample::test_const_context, FixtureCheck::NoThrow);
    CaptureReporter named_fixture_reporter;
    auto const named_fixture_summary = named_fixture.run(named_fixture_reporter);
    if (named_fixture_summary.passed != 1 || named_fixture_summary.failed != 0 ||
        named_fixture_summary.errors != 0 ||
        named_fixture_reporter.results.size() != 1 ||
        named_fixture_reporter.results[0].name != "custom_label" ||
        named_fixture_reporter.results[0].assertions.size() != 2 ||
        named_fixture_reporter.results[0].assertions[0].outcome != Outcome::Passed)
        return 79;

    Suite expressions("macro expressions");
    std::ostringstream expression_output;
    int evaluation_count = 0;
    expressions.add("all macros", [&](Context& t) {
        ARIADNE_TESTING_PROVE_CTX(t, TestLogic::Truth::True);
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(t, ++evaluation_count, 1);
        ARIADNE_TESTING_EXPECT_SAME_CTX(t, TestLogic::Value{2}, TestLogic::Value{2});
        ARIADNE_TESTING_EXPECT_WITHIN_CTX(t, 11, 10, 1);
        ARIADNE_TESTING_EXPECT_THROWS_CTX(t, std::runtime_error, throw std::runtime_error("expected"));
        ARIADNE_TESTING_EXPECT_EXCEPTION_CTX(t, throw 42);
        ARIADNE_TESTING_EXPECT_NO_THROW_CTX(t, (void)0);
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
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(t, ([&]() {
            if (ordering_output.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::runtime_error("CHECK event missing before operand evaluation");
            return 2;
        }()), 2);
    });
    ConsoleReporter ordering_reporter(ordering_output);
    if (!ordering.run(ordering_reporter).successful()) return 38;
    // Rerun the SAME macro expansion without a console reporter.
    // The operand's CHECK-presence precondition now throws; the runner
    // must report that evaluation error instead of silently passing.
    ordering_output.str("");
    ordering_output.clear();
    CaptureReporter ordering_without_console;
    auto const no_console_summary = ordering.run(ordering_without_console);
    if (no_console_summary.errors != 1 || no_console_summary.passed != 0 ||
        ordering_without_console.results.size() != 1 ||
        ordering_without_console.results[0].assertions.size() != 1 ||
        ordering_without_console.results[0].assertions[0].outcome != Outcome::Error ||
        ordering_without_console.results[0].assertions[0].detail !=
            "CHECK event missing before operand evaluation")
        return 89;
    // D01: relational comparisons preserve built-in signedness and ADL evidence.
    Suite relations("relations");
    CaptureReporter relation_capture;
    relations.add("built-in relations", [](Context& t) {
        ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(t, -1, static_cast<std::size_t>(-1));
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, -1, 0u);
        ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(t, -1, 0u);
        ARIADNE_TESTING_EXPECT_GREATER_CTX(t, 0u, -1);
        ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(t, 0u, -1);
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, 7u, 9);
        ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(t, 9u, 9);
    });
    relations.add("generic ADL", [](Context& t) {
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{1}, TestLogic::Value{2});
        ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{2}, TestLogic::Value{1});
        ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{3});
        ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{-3});
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, TestLogic::Value{1}, TestLogic::Value{2});
    });
    relations.add("indeterminate remains indeterminate", [](Context& t) {
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{0}, TestLogic::Value{1});
        ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{0}, TestLogic::Value{1});
        ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{0});
        ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX(t, positive, TestLogic::Value{0});
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, TestLogic::Value{0}, TestLogic::Value{1});
    });
    relations.add("failed relation keeps operands", [](Context& t) {
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, 5, 3);
        ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(t, ordered, TestLogic::Value{1}, TestLogic::Value{2});
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
    int right_order_offset = 0;
    evaluation.add("left, right, predicate", [&](Context& t) {
        ARIADNE_TESTING_EXPECT_LESS_CTX(t, ([&] {
            if (order++ != 0 || live_comparisons.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::logic_error("left operand evaluated before CHECK");
            order += right_order_offset;
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

    // The existing single-evaluation macro expansion is exercised with
    // three genuine operand failures, not copied into new template
    // instantiations (which would leave the original branches uncovered).
    // First: an incorrect initial evaluation order.
    order = 7;
    CaptureReporter wrong_initial_order;
    auto const initial_order_summary = evaluation.run(wrong_initial_order);
    if (initial_order_summary.errors != 1 ||
        wrong_initial_order.results.size() != 1 ||
        wrong_initial_order.results[0].assertions.size() != 1 ||
        wrong_initial_order.results[0].assertions[0].detail !=
            "left operand evaluated before CHECK")
        return 90;

    // Second: the left operand's progress precondition fails when no
    // console CHECK output is available, even with a correct order.
    live_comparisons.str("");
    live_comparisons.clear();
    order = 0;
    CaptureReporter missing_check_output;
    auto const missing_check_summary = evaluation.run(missing_check_output);
    if (missing_check_summary.errors != 1 ||
        missing_check_output.results.size() != 1 ||
        missing_check_output.results[0].assertions.size() != 1 ||
        missing_check_output.results[0].assertions[0].detail !=
            "left operand evaluated before CHECK")
        return 91;

    // Third: the right operand sees a bad order. The adjustable offset
    // emulates an intervening evaluation while the lexical macro site
    // remains the same; this also verifies that CHECK was announced.
    right_order_offset = 1;
    order = 0;
    // The guard observes the original console stream, allowing the
    // left operand to succeed and the right operand to detect the fault.
    auto const right_order_summary = evaluation.run(live_relation_reporter);
    if (right_order_summary.errors != 1 ||
        live_comparisons.str().find("wrong operand order") == std::string::npos)
        return 92;
    // D02: an expected indeterminate value must not become an inconclusive test.
    Suite expectations("evidence expectations");
    CaptureReporter expected_capture;
    expectations.add("exact expectations", [](Context& t) {
        ARIADNE_TESTING_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::True);
        ARIADNE_TESTING_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::True);
        ARIADNE_TESTING_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::Indeterminate);
        ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::False);
        ARIADNE_TESTING_EXPECT_INDETERMINATE_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("failed and inconclusive proof", [](Context& t) {
        ARIADNE_TESTING_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::False);
        ARIADNE_TESTING_EXPECT_DEFINITELY_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("inconclusive refutation", [](Context& t) {
        ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::Indeterminate);
    });
    expectations.add("false possibility and wrong expectation", [](Context& t) {
        ARIADNE_TESTING_EXPECT_POSSIBLY_CTX(t, TestLogic::Truth::False);
        ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX(t, TestLogic::Truth::True);
        ARIADNE_TESTING_EXPECT_INDETERMINATE_CTX(t, TestLogic::Truth::True);
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
    diagnostics.add("moved scope retains ownership", [](Context& c) {
        {
            auto first = c.scope("moved-owner");
            auto second = std::move(first);
            c.prove(true, "scope still active after move");
        }
        c.prove(true, "moved scope closed at end of owner lifetime");
    });
    diagnostics.add("scope restored across cases", [](Context& c) {
        c.prove(true, "new case has no labels");
    });
    ConsoleReporter diag_reporter(diag_output, ConsoleDetail::Concise);
    auto const diag_summary = diagnostics.run(diag_reporter);
    if (diag_summary.failed != 1 || diag_summary.errors != 1 ||
        diag_summary.passed != 2) return 68;
    if (diag_output.str().find("[WARN] oracle=floatmp_rounding.tsv / row=21 / reference is approximate") ==
        std::string::npos) return 69;
    if (diag_output.str().find("context: oracle=floatmp_rounding.tsv / row=21") ==
        std::string::npos) return 70;
    if (diag_output.str().find("diagnostic: input prepared") == std::string::npos ||
        diag_output.str().find("diagnostic: calculation") == std::string::npos) return 71;
    if (diag_output.str().find("context: phase=outer / row=error") == std::string::npos) return 72;
    if (diag_output.str().find("[CHECK 3.1] scope still active after move") ==
        std::string::npos) return 82;
    if (diag_output.str().find("[CHECK 3.2] moved scope closed at end of owner lifetime") ==
        std::string::npos) return 83;
    if (diag_output.str().find("[CHECK 4.1] new case has no labels") == std::string::npos) return 73;
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
            ARIADNE_TESTING_EXPECT_EQUAL(++evaluations, 1);
            ARIADNE_TESTING_PROVE(evaluations == 1);
            ARIADNE_TESTING_EXPECT_NO_THROW((void)evaluations);
        }
        void test_no_throw() { ARIADNE_TESTING_EXPECT_GREATER(2, 1); }
    };
    {
        ARIADNE_TESTING_SUITE("compact");
        ARIADNE_TESTING_ADD_TEST(CompactFixture, test_compact);
        ARIADNE_TESTING_ADD_TEST_NO_THROW(CompactFixture, test_no_throw);
        std::ostringstream compact_output;
        if (::Ariadne::Testing::run_suite(ariadne_test_suite, compact_output) != 0) return 77;
        if (compact_output.str().find("[PASS] compact") == std::string::npos) return 78;

        // Preserve the conventional registration above, and separately verify
        // the API's handling of an explicitly supplied, unprefixed label.
        Suite compact_alias("compact aliases");
        add_fixture_test<CompactFixture>(compact_alias, "manual_label",
            &CompactFixture::test_compact, FixtureCheck::Direct);
        CaptureReporter alias_capture;
        auto const alias_summary = compact_alias.run(alias_capture);
        if (alias_summary.passed != 1 || alias_summary.failed != 0 ||
            alias_capture.results.size() != 1 ||
            alias_capture.results[0].name != "manual_label" ||
            alias_capture.results[0].assertions.size() != 3)
            return 80;
    }
    // A failed proof must make run_suite return a nonzero exit status while
    // retaining the original FAIL diagnostics.
    Suite rejected("expected rejection");
    rejected.add("disproved", [](Context& t) { t.prove(false, "false proof"); });
    std::ostringstream rejected_output;
    if (run_suite(rejected, rejected_output) != 1 ||
        rejected_output.str().find("[FAIL] disproved") == std::string::npos)
        return 81;

    // Suite registration rejects both independent invalid public inputs:
    // an empty name and an empty callable. Neither may append a case.
    Suite validation("registration contracts");
    bool rejected_empty_name = false;
    bool rejected_empty_body = false;
    try {
        validation.add("", [](Context&) {});
    } catch (std::invalid_argument const&) {
        rejected_empty_name = true;
    }
    try {
        validation.add("no action", std::function<void(Context&)>{});
    } catch (std::invalid_argument const&) {
        rejected_empty_body = true;
    }
    if (!rejected_empty_name || !rejected_empty_body) return 84;

    // An unhandled non-std C++ exception is a case Error, not a program
    // termination. A following valid test must still run normally.
    validation.add("nonstandard exception", [](Context&) { throw 42; });
    validation.add("after nonstandard exception", [](Context& t) {
        t.prove(true, "runner continued");
    });
    CaptureReporter contract_reporter;
    auto const contract_summary = validation.run(contract_reporter);
    if (contract_summary.passed != 1 || contract_summary.failed != 0 ||
        contract_summary.inconclusive != 0 || contract_summary.skipped != 0 ||
        contract_summary.errors != 1 || contract_reporter.results.size() != 2)
        return 85;
    if (contract_reporter.results[0].name != "nonstandard exception" ||
        contract_reporter.results[0].outcome != Outcome::Error ||
        contract_reporter.results[0].assertions.size() != 1 ||
        contract_reporter.results[0].assertions[0].description !=
            "Unhandled non-standard exception" ||
        contract_reporter.results[0].assertions[0].outcome != Outcome::Error ||
        contract_reporter.results[0].assertions[0].check_index != 0 ||
        contract_reporter.results[1].name != "after nonstandard exception" ||
        contract_reporter.results[1].outcome != Outcome::Passed ||
        contract_reporter.results[1].assertions.size() != 1 ||
        contract_reporter.results[1].assertions[0].description != "runner continued")
        return 86;

    // Negative wait durations are invalid arguments and must be reported
    // without invoking a condition predicate or blocking on the future.
    Suite invalid_waits("negative wait durations");
    bool evaluated_condition = false;
    std::promise<int> unresolved;
    auto unresolved_future = unresolved.get_future();
    invalid_waits.add("reject negative timeouts", [&](Context& c) {
        c.expect_ready(unresolved_future, std::chrono::milliseconds(-1),
                       "negative future timeout");
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually(cv, lock, [&] {
            evaluated_condition = true;
            return true;
        }, std::chrono::milliseconds(-1), "negative condition timeout");
    });
    invalid_waits.add("continue after invalid timeout", [](Context& c) {
        c.prove(true, "later case still runs");
    });
    CaptureReporter invalid_waits_capture;
    auto const invalid_waits_summary = invalid_waits.run(invalid_waits_capture);
    if (invalid_waits_summary.passed != 1 || invalid_waits_summary.errors != 1 ||
        invalid_waits_summary.failed != 0 || invalid_waits_capture.results.size() != 2 ||
        evaluated_condition)
        return 87;
    auto const& invalid_wait_assertions = invalid_waits_capture.results[0].assertions;
    if (invalid_wait_assertions.size() != 2 ||
        invalid_wait_assertions[0].outcome != Outcome::Error ||
        invalid_wait_assertions[1].outcome != Outcome::Error ||
        invalid_wait_assertions[0].detail != "negative timeout" ||
        invalid_wait_assertions[1].detail != "negative timeout" ||
        invalid_waits_capture.results[1].outcome != Outcome::Passed)
        return 88;

    // One predicate type, twelve logically distinct evidence-expectation
    // combinations. The table verifies the public three-valued contract,
    // including outcomes that are allowed rather than definitely proven.
    struct EvidenceScenario {
        std::string name;
        TestLogic::Truth actual;
        EvidenceExpectation expected;
        Outcome outcome;
        std::string observed_name;
        std::string expected_name;
    };
    std::vector<EvidenceScenario> const evidence_scenarios{
        {"definitely true when true", TestLogic::Truth::True,
            EvidenceExpectation::DefinitelyTrue, Outcome::Passed, "true", "definitely true"},
        {"definitely true when false", TestLogic::Truth::False,
            EvidenceExpectation::DefinitelyTrue, Outcome::Failed, "false", "definitely true"},
        {"definitely true when unknown", TestLogic::Truth::Indeterminate,
            EvidenceExpectation::DefinitelyTrue, Outcome::Inconclusive, "indeterminate", "definitely true"},
        {"possibly true when true", TestLogic::Truth::True,
            EvidenceExpectation::PossiblyTrue, Outcome::Passed, "true", "possibly true"},
        {"possibly true when false", TestLogic::Truth::False,
            EvidenceExpectation::PossiblyTrue, Outcome::Failed, "false", "possibly true"},
        {"possibly true when unknown", TestLogic::Truth::Indeterminate,
            EvidenceExpectation::PossiblyTrue, Outcome::Passed, "indeterminate", "possibly true"},
        {"not possibly when true", TestLogic::Truth::True,
            EvidenceExpectation::NotPossibly, Outcome::Failed, "true", "not possibly true"},
        {"not possibly when false", TestLogic::Truth::False,
            EvidenceExpectation::NotPossibly, Outcome::Passed, "false", "not possibly true"},
        {"not possibly when unknown", TestLogic::Truth::Indeterminate,
            EvidenceExpectation::NotPossibly, Outcome::Inconclusive, "indeterminate", "not possibly true"},
        {"expect unknown when true", TestLogic::Truth::True,
            EvidenceExpectation::Indeterminate, Outcome::Failed, "true", "indeterminate"},
        {"expect unknown when false", TestLogic::Truth::False,
            EvidenceExpectation::Indeterminate, Outcome::Failed, "false", "indeterminate"},
        {"expect unknown when unknown", TestLogic::Truth::Indeterminate,
            EvidenceExpectation::Indeterminate, Outcome::Passed, "indeterminate", "indeterminate"}
    };
    auto evaluate_evidence = [](Context& c, EvidenceScenario const& scenario) {
        c.expect_evidence([&] { return scenario.actual; }, scenario.expected,
                          "three-valued evidence expectation");
    };
    Suite evidence_matrix("all supported evidence expectations");
    std::size_t evidence_passes = 0, evidence_failures = 0, evidence_unknowns = 0;
    for (auto const& scenario : evidence_scenarios) {
        auto const* entry = &scenario;
        evidence_matrix.add(entry->name, [&, entry](Context& c) {
            evaluate_evidence(c, *entry);
        });
        if (entry->outcome == Outcome::Passed) ++evidence_passes;
        else if (entry->outcome == Outcome::Failed) ++evidence_failures;
        else if (entry->outcome == Outcome::Inconclusive) ++evidence_unknowns;
    }
    CaptureReporter evidence_report;
    auto const evidence_summary = evidence_matrix.run(evidence_report);
    if (evidence_summary.passed != evidence_passes ||
        evidence_summary.failed != evidence_failures ||
        evidence_summary.inconclusive != evidence_unknowns ||
        evidence_summary.errors != 0 || evidence_summary.skipped != 0 ||
        evidence_report.results.size() != evidence_scenarios.size()) {
        std::cerr << "evidence expectation matrix: wrong outcome counts\n";
        return 93;
    }
    for (std::size_t i = 0; i != evidence_scenarios.size(); ++i) {
        auto const& scenario = evidence_scenarios[i];
        auto const& result = evidence_report.results[i];
        if (result.name != scenario.name || result.outcome != scenario.outcome ||
            result.assertions.size() != 1 ||
            result.assertions[0].outcome != scenario.outcome ||
            result.assertions[0].actual != scenario.observed_name ||
            result.assertions[0].expected != scenario.expected_name ||
            result.assertions[0].description != "three-valued evidence expectation") {
            std::cerr << "evidence expectation matrix: incorrect result for "
                      << scenario.name << '\n';
            return 94;
        }
    }


    // Verify the console reporter's complete outcome vocabulary, including
    // inconclusive proofs and explicitly skipped cases.
    Suite all_outcomes("all outcome classifications");
    all_outcomes.add("proved", [](Context& c) { c.prove(true, "established"); });
    all_outcomes.add("refuted", [](Context& c) { c.prove(false, "not established"); });
    all_outcomes.add("uncertain", [](Context& c) {
        c.prove(TestLogic::Truth::Indeterminate, "undecidable proposition");
    });
    all_outcomes.add("not applicable", [](Context& c) {
        c.skip("precondition unavailable");
    });
    all_outcomes.add("unexpected exception", [](Context&) {
        throw std::runtime_error("unhandled operation");
    });
    std::ostringstream outcome_output;
    ConsoleReporter outcome_reporter(outcome_output, ConsoleDetail::All);
    auto const all_summary = all_outcomes.run(outcome_reporter);
    if (all_summary.passed != 1 || all_summary.failed != 1 ||
        all_summary.inconclusive != 1 || all_summary.skipped != 1 ||
        all_summary.errors != 1)
        return 95;
    auto const outcome_log = outcome_output.str();
    if (outcome_log.find("[PASS] proved") == std::string::npos ||
        outcome_log.find("[FAIL] refuted") == std::string::npos ||
        outcome_log.find("[INCONCLUSIVE] uncertain") == std::string::npos ||
        outcome_log.find("[SKIP] not applicable") == std::string::npos ||
        outcome_log.find("[ERROR] unexpected exception") == std::string::npos ||
        outcome_log.find("detail: unhandled operation") == std::string::npos)
        return 96;

    // Diagnostics must be streamed in full in All mode. In Concise mode
    // successful notes stay hidden, warnings remain visible, and a failed
    // case retains its diagnostic context and measured values.
    Suite diagnostic_modes("reporter detail modes");
    diagnostic_modes.add("successful diagnostics", [](Context& c) {
        c.note("preparation");
        c.inspect("input", 42);
        c.elapsed("initialization", std::chrono::nanoseconds(123));
        {
            auto scope = c.scope("stage");
            c.warning("approximate oracle");
            c.note("inside stage");
        }
        c.prove(true, "successful operation");
    });
    diagnostic_modes.add("failed diagnostics", [](Context& c) {
        c.note("failure trace");
        c.inspect("observed", 7);
        c.elapsed("failed stage", std::chrono::nanoseconds(5));
        c.prove(false, "expected failed check");
    });
    std::ostringstream all_details;
    ConsoleReporter all_detail_reporter(all_details, ConsoleDetail::All);
    auto const all_details_summary = diagnostic_modes.run(all_detail_reporter);
    if (all_details_summary.passed != 1 || all_details_summary.failed != 1 ||
        all_details_summary.errors != 0)
        return 97;
    auto const full_detail_log = all_details.str();
    if (full_detail_log.find("[NOTE] preparation") == std::string::npos ||
        full_detail_log.find("[VALUE] input = 42") == std::string::npos ||
        full_detail_log.find("[TIME] initialization = 123 ns") == std::string::npos ||
        full_detail_log.find("[SCOPE 1] stage") == std::string::npos ||
        full_detail_log.find("[WARN] stage / approximate oracle") == std::string::npos ||
        full_detail_log.find("[NOTE] stage / inside stage") == std::string::npos ||
        full_detail_log.find("[NOTE] failure trace") == std::string::npos ||
        full_detail_log.find("[VALUE] observed = 7") == std::string::npos ||
        full_detail_log.find("[TIME] failed stage = 5 ns") == std::string::npos)
        return 98;

    std::ostringstream concise_details;
    ConsoleReporter concise_detail_reporter(concise_details, ConsoleDetail::Concise);
    auto const concise_summary = diagnostic_modes.run(concise_detail_reporter);
    if (concise_summary.passed != 1 || concise_summary.failed != 1 ||
        concise_summary.errors != 0)
        return 99;
    auto const concise_log = concise_details.str();
    if (concise_log.find("[NOTE] preparation") != std::string::npos ||
        concise_log.find("[VALUE] input") != std::string::npos ||
        concise_log.find("[TIME] initialization") != std::string::npos ||
        concise_log.find("[WARN] stage / approximate oracle") == std::string::npos ||
        concise_log.find("diagnostic: failure trace") == std::string::npos ||
        concise_log.find("diagnostic: observed = 7") == std::string::npos ||
        concise_log.find("diagnostic: failed stage = 5 ns") == std::string::npos ||
        concise_log.find("diagnostic: preparation") != std::string::npos)
        return 100;

    // Context is also usable without a Reporter. Event and assertion
    // recording must not depend on callback notifications.
    CaseResult manual_result;
    Context manual_context(manual_result);
    manual_context.note("manual context");
    bool rejected_blank_scope = false;
    try {
        auto invalid = manual_context.scope("");
        (void)invalid;
    } catch (std::invalid_argument const&) {
        rejected_blank_scope = true;
    }
    manual_context.prove(true, "manual assertion");
    if (!rejected_blank_scope || manual_result.diagnostics.size() != 1 ||
        manual_result.diagnostics[0].kind != DiagnosticKind::Note ||
        manual_result.diagnostics[0].description != "manual context" ||
        manual_result.assertions.size() != 1 ||
        manual_result.assertions[0].check_index != 1 ||
        manual_result.assertions[0].outcome != Outcome::Passed ||
        manual_result.outcome != Outcome::Passed)
        return 101;


    // A measured operation must emit Elapsed diagnostics even when it
    // fails or aborts a case. It must preserve the original exception
    // classification and must not execute the success continuation.
    enum class TimedBehavior { Complete, ThrowStandard, ThrowNonstandard, AbortRequirement };
    Suite measured_operations("measured operation outcomes");
    int operation_calls = 0;
    int successful_continuations = 0;
    auto run_measured = [&](Context& c, TimedBehavior behavior) {
        c.measure("measured operation", [&] {
            ++operation_calls;
            switch (behavior) {
                case TimedBehavior::Complete:
                    return;
                case TimedBehavior::ThrowStandard:
                    throw std::runtime_error("timed standard failure");
                case TimedBehavior::ThrowNonstandard:
                    throw 42;
                case TimedBehavior::AbortRequirement:
                    ARIADNE_TESTING_REQUIRE_CTX(c, false);
                    return;
                default:
                    throw std::logic_error("unknown timed test behavior");
            }
        });
        ++successful_continuations;
        c.prove(true, "measured operation completed");
    };
    measured_operations.add("completed", [&](Context& c) {
        run_measured(c, TimedBehavior::Complete);
    });
    measured_operations.add("standard exception", [&](Context& c) {
        run_measured(c, TimedBehavior::ThrowStandard);
    });
    measured_operations.add("nonstandard exception", [&](Context& c) {
        run_measured(c, TimedBehavior::ThrowNonstandard);
    });
    measured_operations.add("failed prerequisite", [&](Context& c) {
        run_measured(c, TimedBehavior::AbortRequirement);
    });
    CaptureReporter measured_capture;
    auto const measured_summary = measured_operations.run(measured_capture);
    if (measured_summary.passed != 1 || measured_summary.failed != 1 ||
        measured_summary.errors != 2 || measured_summary.inconclusive != 0 ||
        measured_summary.skipped != 0 || measured_capture.results.size() != 4 ||
        operation_calls != 4 || successful_continuations != 1)
        return 102;
    for (auto const& case_result : measured_capture.results) {
        if (case_result.diagnostics.size() != 1 ||
            case_result.diagnostics[0].kind != DiagnosticKind::Elapsed ||
            case_result.diagnostics[0].description != "measured operation" ||
            case_result.diagnostics[0].duration.count() < 0)
            return 103;
    }
    auto const& measured_ok = measured_capture.results[0];
    auto const& measured_std = measured_capture.results[1];
    auto const& measured_nonstd = measured_capture.results[2];
    auto const& measured_abort = measured_capture.results[3];
    if (measured_ok.outcome != Outcome::Passed ||
        measured_ok.assertions.size() != 1 ||
        measured_ok.assertions[0].description != "measured operation completed" ||
        measured_std.outcome != Outcome::Error ||
        measured_std.assertions.size() != 1 ||
        measured_std.assertions[0].description != "Unhandled std::exception" ||
        measured_std.assertions[0].detail != "timed standard failure" ||
        measured_nonstd.outcome != Outcome::Error ||
        measured_nonstd.assertions.size() != 1 ||
        measured_nonstd.assertions[0].description != "Unhandled non-standard exception" ||
        measured_abort.outcome != Outcome::Failed ||
        measured_abort.assertions.size() != 1 ||
        measured_abort.assertions[0].outcome != Outcome::Failed ||
        measured_abort.assertions[0].description != "false")
        return 104;


    // Imported assertion results may lack a source location. The public
    // console reporter must retain the useful failure diagnostics without
    // manufacturing a file:line reference.
    CaseResult imported_case;
    imported_case.name = "imported assertion";
    imported_case.outcome = Outcome::Failed;
    AssertionRecord imported_assertion{};
    imported_assertion.outcome = Outcome::Failed;
    imported_assertion.description = "imported validation failed";
    imported_assertion.actual = "7";
    imported_assertion.expected = "8";
    imported_assertion.detail = "reference mismatch";
    imported_assertion.check_index = 1;
    imported_case.assertions.push_back(imported_assertion);
    std::ostringstream imported_output;
    ConsoleReporter imported_reporter(imported_output);
    imported_reporter.case_finished(imported_case);
    auto const imported_log = imported_output.str();
    if (imported_log.find("[FAIL] imported assertion") == std::string::npos ||
        imported_log.find("FAIL: imported validation failed") == std::string::npos ||
        imported_log.find("actual: 7") == std::string::npos ||
        imported_log.find("expected: 8") == std::string::npos ||
        imported_log.find("detail: reference mismatch") == std::string::npos ||
        imported_log.find("\n    at ") != std::string::npos)
        return 105;

    // The same record with known provenance must instead retain the
    // exact source location, ensuring both sides of the contract.
    imported_case.assertions[0].file = "imported-test.cpp";
    imported_case.assertions[0].line = 37;
    std::ostringstream located_output;
    ConsoleReporter located_reporter(located_output);
    located_reporter.case_finished(imported_case);
    if (located_output.str().find("at imported-test.cpp:37") == std::string::npos)
        return 106;

    return 0;
}
