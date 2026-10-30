/***************************************************************************
 *            testing.hpp
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

#ifndef ARIADNE_TESTING_TESTING_HPP
#define ARIADNE_TESTING_TESTING_HPP

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Ariadne::Testing {

enum class Evidence { False, True, Indeterminate };
enum class Outcome { Passed, Failed, Inconclusive, Skipped, Error };

// The relation is selected at compile time; predicate evidence stays three-valued.
enum class Comparison { NotEqual, Less, LessEqual, Greater, GreaterEqual };

// Desired evidence, not an algorithm's return code or an operational outcome.
enum class EvidenceExpectation { DefinitelyTrue, PossiblyTrue, NotPossibly, Indeterminate };

struct AssertionRecord {
    Outcome outcome;
    std::string description;
    std::string actual;
    std::string expected;
    std::string detail;
    std::string file;
    std::uint_least32_t line;
    std::size_t check_index = 0;
};

struct CaseResult {
    std::string name;
    Outcome outcome = Outcome::Passed;
    std::vector<AssertionRecord> assertions;
    std::chrono::nanoseconds duration{};
    std::string suite_name;
    std::size_t case_index = 0;
    std::size_t case_total = 0;
};

// Exit-gate policy only: assertion outcomes and summary counts never change.
enum class InconclusivePolicy { Blocking, NonBlocking };
enum class SkippedPolicy { NonBlocking, Blocking };

struct AcceptancePolicy {
    InconclusivePolicy inconclusive = InconclusivePolicy::Blocking;
    SkippedPolicy skipped = SkippedPolicy::NonBlocking;
};

struct Summary {
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t inconclusive = 0;
    std::size_t skipped = 0;
    std::size_t errors = 0;

    // Even an opt-in permissive exit gate requires at least one actual pass.
    bool acceptable(AcceptancePolicy policy) const {
        return passed > 0 && failed == 0 && errors == 0
            && (inconclusive == 0 || policy.inconclusive == InconclusivePolicy::NonBlocking)
            && (skipped == 0 || policy.skipped == SkippedPolicy::NonBlocking);
    }
    bool successful() const { return acceptable({}); }
};

class Reporter {
  public:
    virtual ~Reporter() = default;
    // Optional lifecycle notifications, compatible with existing reporters.
    virtual void case_started(CaseResult const&) { }
    virtual void check_started(CaseResult const&, std::size_t, std::string_view) { }
    virtual void case_finished(CaseResult const& result) = 0;
    virtual void suite_finished(std::string_view name, Summary const& summary) = 0;
};

class ConsoleReporter final : public Reporter {
  public:
    explicit ConsoleReporter(std::ostream& output);
    void case_started(CaseResult const& result) override;
    void check_started(CaseResult const& result, std::size_t index,
                       std::string_view description) override;
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::ostream& _output;
};

namespace detail {

template<class T>
Evidence evidence(T const& value) {
    if constexpr (requires { definitely(value); possibly(value); }) {
        bool const definite = static_cast<bool>(definitely(value));
        bool const possible = static_cast<bool>(possibly(value));
        if (definite && !possible) {
            throw std::logic_error("Inconsistent logical evidence: definitely true but not possibly true");
        }
        if (definite) return Evidence::True;
        if (!possible) return Evidence::False;
        return Evidence::Indeterminate;
    } else {
        return static_cast<bool>(value) ? Evidence::True : Evidence::False;
    }
}

// Negating a proof must not turn uncertainty into a boolean success.
inline Evidence negate(Evidence value) {
    switch (value) {
        case Evidence::True: return Evidence::False;
        case Evidence::False: return Evidence::True;
        case Evidence::Indeterminate: return Evidence::Indeterminate;
        default: throw std::logic_error("Invalid mathematical evidence");
    }
}

template<class T>
std::string render(T const& value) {
    if constexpr (requires(std::ostream& os) { os << value; }) {
        std::ostringstream os;
        os << std::boolalpha << value;
        return os.str();
    } else {
        return "<unprintable>";
    }
}

template<class T>
auto magnitude(T const& value) {
    if constexpr (requires { mag(value); }) {
        return mag(value);
    } else {
        using std::abs;
        return abs(value);
    }
}

} // namespace detail

class Context {
  public:
    explicit Context(CaseResult& result, Reporter* reporter = nullptr)
        : _result(result), _reporter(reporter) { }

    template<class T>
    void prove(T const& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        record(outcome(detail::evidence(predicate)), description, {}, {}, {}, location, index);
    }

    // Test a specified mathematical evidence classification. A deliberately
    // expected indeterminate result is a passing test, not a proven statement.
    template<class PredicateFn>
    void expect_evidence(PredicateFn&& predicate_fn, EvidenceExpectation expected,
                         std::string_view description,
                         std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const actual = detail::evidence(std::invoke(std::forward<PredicateFn>(predicate_fn)));
        Outcome status = Outcome::Failed;
        switch (expected) {
            case EvidenceExpectation::DefinitelyTrue:
                status = outcome(actual);
                break;
            case EvidenceExpectation::PossiblyTrue:
                status = actual == Evidence::False ? Outcome::Failed : Outcome::Passed;
                break;
            case EvidenceExpectation::NotPossibly:
                status = actual == Evidence::False ? Outcome::Passed
                       : actual == Evidence::Indeterminate ? Outcome::Inconclusive : Outcome::Failed;
                break;
            case EvidenceExpectation::Indeterminate:
                status = actual == Evidence::Indeterminate ? Outcome::Passed : Outcome::Failed;
                break;
            default:
                status = Outcome::Error;
                break;
        }
        auto const evidence_name = [](Evidence value) -> std::string {
            switch (value) {
                case Evidence::True: return "true";
                case Evidence::False: return "false";
                case Evidence::Indeterminate: return "indeterminate";
                default: return "invalid";
            }
        };
        auto const expected_name = [](EvidenceExpectation value) -> std::string {
            switch (value) {
                case EvidenceExpectation::DefinitelyTrue: return "definitely true";
                case EvidenceExpectation::PossiblyTrue: return "possibly true";
                case EvidenceExpectation::NotPossibly: return "not possibly true";
                case EvidenceExpectation::Indeterminate: return "indeterminate";
                default: return "invalid";
            }
        };
        record(status, description, evidence_name(actual), expected_name(expected),
               {}, location, index);
    }

    // Deferred operands: emit CHECK before evaluating each expression once.
    template<class ActualFn, class ExpectedFn>
    void check_equal(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                     std::string_view description,
                     std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        auto const verdict = [&]() {
            using A = std::remove_cvref_t<decltype(actual)>;
            using B = std::remove_cvref_t<decltype(expected)>;
            if constexpr (std::is_integral_v<A> && std::is_integral_v<B>
                          && !std::is_same_v<A,bool> && !std::is_same_v<B,bool>) {
                return detail::evidence(std::cmp_equal(actual,expected));
            } else {
                return detail::evidence(actual == expected);
            }
        }();
        record(outcome(verdict), description,
               detail::render(actual), detail::render(expected), {}, location, index);
    }

    // Each relation is selected at compile time and evaluates its operands once,
    // left-to-right, after the live CHECK notification.
    template<Comparison Relation, class ActualFn, class ExpectedFn>
    void check_compare(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                       std::string_view description,
                       std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        using A = std::remove_cvref_t<decltype(actual)>;
        using B = std::remove_cvref_t<decltype(expected)>;
        constexpr bool built_in_integers =
            std::is_integral_v<A> && std::is_integral_v<B>
            && !std::is_same_v<A,bool> && !std::is_same_v<B,bool>;
        auto const verdict = [&]() -> Evidence {
            if constexpr (Relation == Comparison::NotEqual) {
                if constexpr (built_in_integers) return detail::evidence(!std::cmp_equal(actual, expected));
                else return detail::evidence(actual != expected);
            } else if constexpr (Relation == Comparison::Less) {
                if constexpr (built_in_integers) return detail::evidence(std::cmp_less(actual, expected));
                else return detail::evidence(actual < expected);
            } else if constexpr (Relation == Comparison::LessEqual) {
                if constexpr (built_in_integers) return detail::evidence(std::cmp_less_equal(actual, expected));
                else return detail::evidence(actual <= expected);
            } else if constexpr (Relation == Comparison::Greater) {
                if constexpr (built_in_integers) return detail::evidence(std::cmp_greater(actual, expected));
                else return detail::evidence(actual > expected);
            } else {
                static_assert(Relation == Comparison::GreaterEqual);
                if constexpr (built_in_integers) return detail::evidence(std::cmp_greater_equal(actual, expected));
                else return detail::evidence(actual >= expected);
            }
        }();
        record(outcome(verdict), description,
               detail::render(actual), detail::render(expected), {}, location, index);
    }

    template<class Predicate, class OperandFn>
    void check_unary(Predicate&& predicate, OperandFn&& operand_fn,
                     std::string_view description, bool negate = false,
                     std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) operand = std::invoke(std::forward<OperandFn>(operand_fn));
        auto verdict = detail::evidence(std::invoke(std::forward<Predicate>(predicate), operand));
        if (negate) verdict = detail::negate(verdict);
        record(outcome(verdict), description, detail::render(operand), {}, {}, location, index);
    }

    template<class Predicate, class LeftFn, class RightFn>
    void check_binary(Predicate&& predicate, LeftFn&& left_fn, RightFn&& right_fn,
                      std::string_view description, bool negate = false,
                      std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) left = std::invoke(std::forward<LeftFn>(left_fn));
        decltype(auto) right = std::invoke(std::forward<RightFn>(right_fn));
        auto verdict = detail::evidence(std::invoke(std::forward<Predicate>(predicate), left, right));
        if (negate) verdict = detail::negate(verdict);
        record(outcome(verdict), description,
               detail::render(left), detail::render(right), {}, location, index);
    }

    template<class A, class B>
    void expect_equal(A const& actual, B const& expected,
                      std::string_view description = "equality",
                      std::source_location location = std::source_location::current()) {
        check_equal([&]() -> A const& { return actual; },
                    [&]() -> B const& { return expected; }, description, location);
    }

    template<class ActualFn, class ExpectedFn>
    void check_same(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                    std::string_view description,
                    std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        auto const relation = same(actual, expected); // ADL: numerical domain
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected), {}, location, index);
    }

    template<class A, class B>
    void expect_same(A const& actual, B const& expected,
                     std::string_view description = "identity",
                     std::source_location location = std::source_location::current()) {
        check_same([&]() -> A const& { return actual; },
                   [&]() -> B const& { return expected; }, description, location);
    }

    template<class ActualFn, class ExpectedFn, class ToleranceFn>
    void check_within(ActualFn&& actual_fn, ExpectedFn&& expected_fn, ToleranceFn&& tolerance_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        decltype(auto) tolerance = std::invoke(std::forward<ToleranceFn>(tolerance_fn));
        auto const error = detail::magnitude(actual - expected);
        auto const relation = (error <= tolerance);
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected),
               "error=" + detail::render(error) + ", tolerance=" + detail::render(tolerance), location, index);
    }

    template<class A, class B, class Tolerance>
    void expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description = "bounded error",
                       std::source_location location = std::source_location::current()) {
        check_within([&]() -> A const& { return actual; },
                     [&]() -> B const& { return expected; },
                     [&]() -> Tolerance const& { return tolerance; }, description, location);
    }

    template<class Exception, class Callable>
    void expect_throws(Callable&& action, std::string_view description = "expected exception",
                       std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Exception const&) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, std::string("unexpected exception: ") + e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception type", location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }
    template<class Callable>
    void expect_exception(Callable&& action, std::string_view description = "exception expected",
                          std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (...) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }
    template<class Callable>
    void expect_no_throw(Callable&& action, std::string_view description = "no exception expected",
                         std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception", location, index); return; }
        record(Outcome::Passed, description, {}, {}, {}, location, index);
    }

    // Deferred checker: emit progress before evaluating its potentially costly predicate.
    template<class Callable>
    void check(Callable&& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const value = std::invoke(std::forward<Callable>(predicate));
        record(outcome(detail::evidence(value)), description, {}, {}, {}, location, index);
    }

    void skip(std::string_view reason,
              std::source_location location = std::source_location::current());

  private:
    friend class Suite;
    static Outcome outcome(Evidence value);
    std::size_t begin_check(std::string_view description);
    void record(Outcome outcome, std::string_view description, std::string actual,
                std::string expected, std::string detail, std::source_location location,
                std::size_t index);
    CaseResult& _result;
    Reporter* _reporter;
    std::size_t _check_count = 0;
};

class Suite {
  public:
    explicit Suite(std::string name) : _name(std::move(name)) { }
    void add(std::string name, std::function<void(Context&)> test);
    Summary run(Reporter& reporter) const;
  private:
    struct TestCase {
        std::string name;
        std::function<void(Context&)> run;
    };
    std::string _name;
    std::vector<TestCase> _tests;
};

} // namespace Ariadne::Testing

#endif // ARIADNE_TESTING_TESTING_HPP
