/***************************************************************************
 *            testing.tpl.hpp
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

/** @file testing/testing.tpl.hpp
 *  @brief Template definitions for the Testing core API.
 */
#ifndef ARIADNE_TESTING_TESTING_TPL_HPP
#define ARIADNE_TESTING_TESTING_TPL_HPP

#include "testing/testing.hpp"

namespace Ariadne::Testing {

namespace Detail {

template<class T>
Evidence evidence(T const& value){
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, Evidence>) {
        return value;
    } else if constexpr (requires { definitely(value); possibly(value); }) {
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

template<class T>
std::string render(T const& value){
    if constexpr (requires(std::ostream& os) { os << value; }) {
        std::ostringstream os;
        os << std::boolalpha << value;
        return os.str();
    } else {
        return "<unprintable>";
    }
}

template<class T>
constexpr auto comparable_integer(T value){
    using V = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V, char> || std::is_same_v<V, wchar_t> ||
                  std::is_same_v<V, char8_t> || std::is_same_v<V, char16_t> ||
                  std::is_same_v<V, char32_t>) {
        if constexpr (std::is_signed_v<V>) return static_cast<std::intmax_t>(value);
        else return static_cast<std::uintmax_t>(value);
    } else {
        return value;
    }
}

template<class Actual, class Expected>
Evidence equal_evidence(Actual const& actual, Expected const& expected){
    using A = std::remove_cvref_t<Actual>;
    using B = std::remove_cvref_t<Expected>;
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>
                  && !std::is_same_v<A, bool> && !std::is_same_v<B, bool>) {
        return evidence(std::cmp_equal(comparable_integer(actual), comparable_integer(expected)));
    } else {
        return evidence(actual == expected);
    }
}

struct EqualityObservation {
    Evidence verdict;
    std::string actual;
    std::string expected;
};

template<class A, class B>
EqualityObservation observe_equal(A const& actual, B const& expected){
    auto const verdict = equal_evidence(actual, expected);
    return {verdict, render(actual), render(expected)};
}

template<Comparison Relation, class A, class B>
EqualityObservation observe_compare(A const& actual, B const& expected) {
    using Left = std::remove_cvref_t<A>;
    using Right = std::remove_cvref_t<B>;
    constexpr bool built_in_integers =
        std::is_integral_v<Left> && std::is_integral_v<Right> &&
        !std::is_same_v<Left, bool> && !std::is_same_v<Right, bool>;
    constexpr bool boolean_arithmetic =
        (std::is_same_v<Left, bool> || std::is_same_v<Right, bool>) &&
        std::is_arithmetic_v<Left> && std::is_arithmetic_v<Right>;
    decltype(auto) lhs = [&]() -> decltype(auto) {
        if constexpr (boolean_arithmetic)
            return static_cast<std::common_type_t<int, Left, Right>>(actual);
        else return (actual);
    }();
    decltype(auto) rhs = [&]() -> decltype(auto) {
        if constexpr (boolean_arithmetic)
            return static_cast<std::common_type_t<int, Left, Right>>(expected);
        else return (expected);
    }();
    Evidence verdict;
    if constexpr (Relation == Comparison::NotEqual) {
        if constexpr (built_in_integers)
            verdict = evidence(!std::cmp_equal(lhs, rhs));
        else verdict = evidence(lhs != rhs);
    } else if constexpr (Relation == Comparison::Less) {
        if constexpr (built_in_integers)
            verdict = evidence(std::cmp_less(lhs, rhs));
        else verdict = evidence(lhs < rhs);
    } else if constexpr (Relation == Comparison::LessEqual) {
        if constexpr (built_in_integers)
            verdict = evidence(std::cmp_less_equal(lhs, rhs));
        else verdict = evidence(lhs <= rhs);
    } else if constexpr (Relation == Comparison::Greater) {
        if constexpr (built_in_integers)
            verdict = evidence(std::cmp_greater(lhs, rhs));
        else verdict = evidence(lhs > rhs);
    } else {
        static_assert(Relation == Comparison::GreaterEqual);
        if constexpr (built_in_integers)
            verdict = evidence(std::cmp_greater_equal(lhs, rhs));
        else verdict = evidence(lhs >= rhs);
    }
    return {verdict, render(actual), render(expected)};
}

template<class Callable>
auto capture_callable(Callable&& callable){
    if constexpr (std::is_lvalue_reference_v<Callable>)
        return std::ref(callable);
    else
        return std::forward<Callable>(callable);
}

template<class T>
auto magnitude(T const& value){
    if constexpr (requires { mag(value); }) {
        return mag(value);
    } else {
        using std::abs;
        return abs(value);
    }
}

} // namespace Detail

template<class T>
    Context::Scope Context::scope(std::string name, T const& value,
                std::source_location location){
        return scope(std::move(name) + "=" + Detail::render(value), location);
    }

template<class T>
    void Context::inspect(std::string name, T const& value,
                 std::source_location location){
        emit(DiagnosticKind::Value, std::move(name), Detail::render(value), {}, location);
    }

template<class Callable>
    void Context::measure(std::string description, Callable&& operation,
                 std::source_location location){
        static_assert(std::is_void_v<std::invoke_result_t<Callable&>>,
                      "measure() requires a void callable");
        auto const start = std::chrono::steady_clock::now();
        try {
            std::invoke(std::forward<Callable>(operation));
        } catch (...) {
            elapsed(description, std::chrono::steady_clock::now() - start, location);
            throw;
        }
        elapsed(description, std::chrono::steady_clock::now() - start, location);
    }

template<class T>
    void Context::prove(T const& predicate, std::string_view description,
               std::source_location location){
        auto const index = begin_check(description);
        record(outcome(Detail::evidence(predicate)), description, {}, {}, {}, location, index);
    }

template<class PredicateFn>
    void Context::expect_evidence(PredicateFn&& predicate_fn, EvidenceExpectation expected,
                         std::string_view description,
                         std::source_location location){
        auto const index = begin_check(description);
        auto const actual = Detail::evidence(std::invoke(std::forward<PredicateFn>(predicate_fn)));
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

template<class ActualFn, class ExpectedFn>
    void Context::check_equal(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                     std::string_view description,
                     std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        auto const verdict = Detail::equal_evidence(actual, expected);
        record(outcome(verdict), description,
               Detail::render(actual), Detail::render(expected), {}, location, index);
    }

template<class ComparisonFn>
    void Context::check_equal_snapshot(ComparisonFn&& comparison_fn,
                     std::string_view description,
                     std::source_location location){
        auto const index = begin_check(description);
        auto observation = std::invoke(std::forward<ComparisonFn>(comparison_fn));
        record(outcome(observation.verdict), description,
               std::move(observation.actual), std::move(observation.expected),
               {}, location, index);
    }

template<Comparison Relation, class ComparisonFn>
    void Context::check_compare_snapshot(ComparisonFn&& comparison_fn,
                     std::string_view description,
                     std::source_location location){
        auto const index = begin_check(description);
        auto observation = std::invoke(std::forward<ComparisonFn>(comparison_fn));
        record(outcome(observation.verdict), description,
               std::move(observation.actual), std::move(observation.expected),
               {}, location, index);
    }

template<class ActualFn, class ExpectedFn, class ComparatorFn, class PathFn>
    void Context::check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, PathFn&& path_fn,
                      std::string_view description,
                      std::source_location location){
        auto const check_index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        static_assert(std::ranges::input_range<decltype(actual)>);
        static_assert(std::ranges::input_range<decltype(expected)>);
        decltype(auto) comparator = std::invoke(std::forward<ComparatorFn>(comparator_fn));
        decltype(auto) path = std::invoke(std::forward<PathFn>(path_fn));

        auto a = std::ranges::begin(actual);
        auto e = std::ranges::begin(expected);
        auto const a_end = std::ranges::end(actual);
        auto const e_end = std::ranges::end(expected);
        std::size_t actual_count = 0;
        std::size_t expected_count = 0;
        std::optional<std::size_t> first_failure;
        std::optional<std::size_t> first_unknown;
        std::string failure_actual;
        std::string failure_expected;
        std::string unknown_actual;
        std::string unknown_expected;
        auto snippet = [](auto const& value) {
            std::string text = Detail::render(value);
            constexpr std::size_t max_length = 96;
            if (text.size() > max_length) {
                text.resize(max_length - 3);
                text += "...";
            }
            return text;
        };
        while (a != a_end && e != e_end) {
            auto const comparison = std::invoke(comparator, *a, *e);
            auto const verdict = [&] {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(comparison)>, Evidence>)
                    return comparison;
                else return Detail::evidence(comparison);
            }();
            if (verdict == Evidence::False && !first_failure) {
                first_failure = actual_count;
                failure_actual = snippet(*a);
                failure_expected = snippet(*e);
            } else if (verdict == Evidence::Indeterminate && !first_unknown) {
                first_unknown = actual_count;
                unknown_actual = snippet(*a);
                unknown_expected = snippet(*e);
            }
            ++a;
            ++e;
            ++actual_count;
            ++expected_count;
        }
        if (a != a_end && !first_failure) {
            first_failure = actual_count;
            failure_actual = snippet(*a);
            failure_expected = "<end>";
        }
        if (e != e_end && !first_failure) {
            first_failure = expected_count;
            failure_actual = "<end>";
            failure_expected = snippet(*e);
        }
        for (; a != a_end; ++a) ++actual_count;
        for (; e != e_end; ++e) ++expected_count;

        // A remaining tail always sets first_failure above, so differing
        // counts cannot occur without it. Earlier uncertainty never masks
        // a definite mismatch or a missing element.
        auto const status = first_failure ? Outcome::Failed
                          : first_unknown ? Outcome::Inconclusive
                          : Outcome::Passed;
        std::string info = "actual_count=" + std::to_string(actual_count)
                         + ", expected_count=" + std::to_string(expected_count);
        auto const mismatch = first_failure ? first_failure : first_unknown;
        std::string actual_detail = "count=" + std::to_string(actual_count);
        std::string expected_detail = "count=" + std::to_string(expected_count);
        if (mismatch) {
            info += ", first_mismatch_index=" + std::to_string(*mismatch);
            info += ", path=" + Detail::render(std::invoke(path, *mismatch));
            actual_detail += ", value=" + (first_failure ? failure_actual : unknown_actual);
            expected_detail += ", value=" + (first_failure ? failure_expected : unknown_expected);
        }
        if (first_unknown && first_failure) {
            info += ", first_indeterminate_index=" + std::to_string(*first_unknown);
        }
        record(status, description, std::move(actual_detail),
               std::move(expected_detail), std::move(info), location, check_index);
    }

template<class ActualFn, class ExpectedFn, class ComparatorFn>
    void Context::check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, std::string_view description,
                      std::source_location location){
        check_ranges(std::forward<ActualFn>(actual_fn),
                     std::forward<ExpectedFn>(expected_fn),
                     std::forward<ComparatorFn>(comparator_fn),
                     [] { return [](std::size_t index) { return std::to_string(index); }; },
                     description, location);
    }

template<class ActualFn, class ExpectedFn>
    void Context::check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      std::string_view description,
                      std::source_location location){
        check_ranges(std::forward<ActualFn>(actual_fn),
                     std::forward<ExpectedFn>(expected_fn),
                     [] { return [](auto const& a, auto const& b) {
                         return Detail::equal_evidence(a, b);
                     }; },
                     description, location);
    }

template<Comparison Relation, class ActualFn, class ExpectedFn>
    void Context::check_compare(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                       std::string_view description,
                       std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        using A = std::remove_cvref_t<decltype(actual)>;
        using B = std::remove_cvref_t<decltype(expected)>;
        constexpr bool built_in_integers =
            std::is_integral_v<A> && std::is_integral_v<B>
            && !std::is_same_v<A,bool> && !std::is_same_v<B,bool>;
        // Built-in arithmetic comparisons promote bool to int. Apply the
        // same promotion explicitly so MSVC does not reject bool relational
        // operators when warnings are treated as errors. Domain-specific
        // operators still receive their original types for ADL.
        constexpr bool boolean_arithmetic =
            (std::is_same_v<A, bool> || std::is_same_v<B, bool>) &&
            std::is_arithmetic_v<A> && std::is_arithmetic_v<B>;
        decltype(auto) comparable_actual = [&]() -> decltype(auto) {
            if constexpr (boolean_arithmetic)
                return static_cast<std::common_type_t<int, A, B>>(actual);
            else return (actual);
        }();
        decltype(auto) comparable_expected = [&]() -> decltype(auto) {
            if constexpr (boolean_arithmetic)
                return static_cast<std::common_type_t<int, A, B>>(expected);
            else return (expected);
        }();
        auto const verdict = [&]() -> Evidence {
            if constexpr (Relation == Comparison::NotEqual) {
                if constexpr (built_in_integers) return Detail::evidence(!std::cmp_equal(comparable_actual, comparable_expected));
                else return Detail::evidence(comparable_actual != comparable_expected);
            } else if constexpr (Relation == Comparison::Less) {
                if constexpr (built_in_integers) return Detail::evidence(std::cmp_less(comparable_actual, comparable_expected));
                else return Detail::evidence(comparable_actual < comparable_expected);
            } else if constexpr (Relation == Comparison::LessEqual) {
                if constexpr (built_in_integers) return Detail::evidence(std::cmp_less_equal(comparable_actual, comparable_expected));
                else return Detail::evidence(comparable_actual <= comparable_expected);
            } else if constexpr (Relation == Comparison::Greater) {
                if constexpr (built_in_integers) return Detail::evidence(std::cmp_greater(comparable_actual, comparable_expected));
                else return Detail::evidence(comparable_actual > comparable_expected);
            } else {
                static_assert(Relation == Comparison::GreaterEqual);
                if constexpr (built_in_integers) return Detail::evidence(std::cmp_greater_equal(comparable_actual, comparable_expected));
                else return Detail::evidence(comparable_actual >= comparable_expected);
            }
        }();
        record(outcome(verdict), description,
               Detail::render(actual), Detail::render(expected), {}, location, index);
    }

template<class Predicate, class OperandFn>
    void Context::check_unary(Predicate&& predicate, OperandFn&& operand_fn,
                     std::string_view description, bool negate,
                     std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) operand = std::invoke(std::forward<OperandFn>(operand_fn));
        auto verdict = Detail::evidence(std::invoke(std::forward<Predicate>(predicate), operand));
        if (negate) verdict = Detail::negate(verdict);
        record(outcome(verdict), description, Detail::render(operand), {}, {}, location, index);
    }

template<class Predicate, class LeftFn, class RightFn>
    void Context::check_binary(Predicate&& predicate, LeftFn&& left_fn, RightFn&& right_fn,
                      std::string_view description, bool negate,
                      std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) left = std::invoke(std::forward<LeftFn>(left_fn));
        decltype(auto) right = std::invoke(std::forward<RightFn>(right_fn));
        auto verdict = Detail::evidence(std::invoke(std::forward<Predicate>(predicate), left, right));
        if (negate) verdict = Detail::negate(verdict);
        record(outcome(verdict), description,
               Detail::render(left), Detail::render(right), {}, location, index);
    }

template<class A, class B>
    void Context::expect_equal(A const& actual, B const& expected,
                      std::string_view description,
                      std::source_location location){
        check_equal([&]() -> A const& { return actual; },
                    [&]() -> B const& { return expected; }, description, location);
    }

template<class ActualFn, class ExpectedFn>
    void Context::check_same(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                    std::string_view description,
                    std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        auto const relation = same(actual, expected); // ADL: numerical domain
        record(outcome(Detail::evidence(relation)), description,
               Detail::render(actual), Detail::render(expected), {}, location, index);
    }

template<class A, class B>
    void Context::expect_same(A const& actual, B const& expected,
                     std::string_view description,
                     std::source_location location){
        check_same([&]() -> A const& { return actual; },
                   [&]() -> B const& { return expected; }, description, location);
    }

template<class ActualFn, class ExpectedFn, class ToleranceFn>
    void Context::check_within(ActualFn&& actual_fn, ExpectedFn&& expected_fn, ToleranceFn&& tolerance_fn,
                      std::string_view description,
                      std::source_location location){
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        decltype(auto) tolerance = std::invoke(std::forward<ToleranceFn>(tolerance_fn));
        auto const error = Detail::magnitude(actual - expected);
        auto const relation = (error <= tolerance);
        record(outcome(Detail::evidence(relation)), description,
               Detail::render(actual), Detail::render(expected),
               "error=" + Detail::render(error) + ", tolerance=" + Detail::render(tolerance), location, index);
    }

template<class A, class B, class Tolerance>
    void Context::expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description,
                       std::source_location location){
        check_within([&]() -> A const& { return actual; },
                     [&]() -> B const& { return expected; },
                     [&]() -> Tolerance const& { return tolerance; }, description, location);
    }

template<class Callable>
    void Context::require(Callable&& predicate, std::string_view description,
                 std::source_location location){
        auto const index = begin_check(description);
        auto const evidence = Detail::evidence(std::invoke(std::forward<Callable>(predicate)));
        auto const status = outcome(evidence);
        record(status, description, {}, {}, {}, location, index);
        if (status != Outcome::Passed) throw Detail::AbortCase{};
    }

template<class Exception, class Callable>
    void Context::expect_throws(Callable&& action, std::string_view description,
                       std::source_location location){
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (Exception const&) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, std::string("unexpected exception: ") + e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception type", location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }

template<class Exception, class Callable>
    void Context::expect_throws_message(Callable&& action, std::string_view substring,
                               std::string_view description,
                               std::source_location location){
        static_assert(std::is_base_of_v<std::exception, Exception>,
                      "Exception message checks require a std::exception-derived type");
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (Exception const& e) {
            std::string const actual(e.what());
            bool const matches = actual.find(substring) != std::string::npos;
            record(matches ? Outcome::Passed : Outcome::Failed, description,
                   actual, std::string(substring),
                   matches ? "" : "exception message mismatch", location, index);
            return;
        }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, e.what(), std::string(substring),
                   "unexpected exception type", location, index);
            return;
        }
        catch (...) {
            record(Outcome::Failed, description, {}, std::string(substring),
                   "unexpected non-standard exception", location, index);
            return;
        }
        record(Outcome::Failed, description, {}, std::string(substring),
               "no exception thrown", location, index);
    }

template<class Exception, class Action, class ProjectorFn, class ExpectedFn>
    void Context::expect_throws_payload(Action&& action, ProjectorFn&& projector_fn,
                               ExpectedFn&& expected_fn, std::string_view description,
                               std::source_location location){
        static_assert(std::is_base_of_v<std::exception, Exception>,
                      "Payload checking requires a std::exception-derived type");
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Action>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (Exception const& exception) {
            try {
                decltype(auto) projector =
                    std::invoke(std::forward<ProjectorFn>(projector_fn));
                decltype(auto) actual = std::invoke(projector, exception);
                decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
                auto const verdict = Detail::equal_evidence(actual, expected);
                record(outcome(verdict), description, Detail::render(actual),
                       Detail::render(expected), {}, location, index);
            } catch (Detail::AbortCase const&) {
                throw;
            } catch (std::exception const& error) {
                record(Outcome::Error, description, {}, {},
                       std::string("payload verification threw: ") + error.what(),
                       location, index);
            } catch (...) {
                record(Outcome::Error, description, {}, {},
                       "payload verification threw a non-standard exception",
                       location, index);
            }
            return;
        }
        catch (std::exception const& error) {
            record(Outcome::Failed, description, error.what(), {},
                   "unexpected exception type", location, index);
            return;
        }
        catch (...) {
            record(Outcome::Failed, description, {}, {},
                   "unexpected non-standard exception", location, index);
            return;
        }
        record(Outcome::Failed, description, {}, {},
               "no exception thrown", location, index);
    }


template<class Operation, class Selector, class Status, class Verify>
void Context::expect_status(Operation&& operation, Selector&& selector,
                            std::initializer_list<Status> permitted, Verify&& verify,
                            std::string_view description,
                            std::source_location location) {
    auto const index = begin_check(description);
    decltype(auto) result = std::invoke(std::forward<Operation>(operation));
    decltype(auto) actual = std::invoke(std::forward<Selector>(selector), std::as_const(result));
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(actual)>, Status>,
                  "Selected status and permitted statuses must have the same type");
    static_assert(std::is_convertible_v<
                  decltype(std::declval<Status const&>() == std::declval<Status const&>()), bool>,
                  "Domain status equality must produce bool");
    std::string expected = "one of [";
    bool accepted = false;
    bool first = true;
    for (auto const& candidate : permitted) {
        if (!first) expected += ", ";
        first = false;
        expected += Detail::render(candidate);
        if (static_cast<bool>(actual == candidate)) accepted = true;
    }
    expected += "]";
    if (!accepted) {
        record(Outcome::Failed, description, Detail::render(actual), expected,
               "domain status not permitted", location, index);
        return;
    }
    auto const verdict = Detail::evidence(
        std::invoke(std::forward<Verify>(verify), std::as_const(result)));
    record(outcome(verdict), description, Detail::render(actual), expected,
           verdict == Evidence::False ? "status postcondition failed" :
           verdict == Evidence::Indeterminate ? "status postcondition indeterminate" : "",
           location, index);
}

template<class Operation, class Selector, class Status>
void Context::expect_status(Operation&& operation, Selector&& selector,
                            std::initializer_list<Status> permitted,
                            std::string_view description,
                            std::source_location location) {
    expect_status(std::forward<Operation>(operation), std::forward<Selector>(selector),
                  permitted, [](auto const&) { return true; }, description, location);
}


template<class Operation, class VerifyResult, class VerifyException>
void Context::expect_alternative(Operation&& operation, VerifyResult&& verify_result,
                                 VerifyException&& verify_exception,
                                 std::string_view description,
                                 std::source_location location) {
    using OperationResult = std::invoke_result_t<Operation&&>;
    static_assert(!std::is_reference_v<OperationResult>,
                  "Alternative checks require a value or void operation result");
    using StoredResult = std::conditional_t<std::is_void_v<OperationResult>,
                                            std::monostate, std::remove_cv_t<OperationResult>>;
    auto const index = begin_check(description);
    std::optional<StoredResult> result;
    std::exception_ptr failure;
    try {
        if constexpr (std::is_void_v<OperationResult>) {
            std::invoke(std::forward<Operation>(operation));
            result.emplace();
        } else {
            result.emplace(std::invoke(std::forward<Operation>(operation)));
        }
    } catch (Detail::AbortCase const&) {
        throw;
    } catch (...) {
        failure = std::current_exception();
    }

    if (failure) {
        try {
            std::rethrow_exception(failure);
        } catch (std::exception const& error) {
            try {
                auto const verdict = Detail::evidence(
                    std::invoke(std::forward<VerifyException>(verify_exception), error));
                record(outcome(verdict), description, error.what(), "permitted exception",
                       verdict == Evidence::False ? "exception alternative rejected" :
                       verdict == Evidence::Indeterminate ? "exception postcondition indeterminate" : "",
                       location, index);
            } catch (Detail::AbortCase const&) {
                throw;
            } catch (std::exception const& verification_error) {
                record(Outcome::Error, description, {}, "permitted exception",
                       std::string("exception verifier threw: ") + verification_error.what(),
                       location, index);
            } catch (...) {
                record(Outcome::Error, description, {}, "permitted exception",
                       "exception verifier threw a non-standard exception", location, index);
            }
        } catch (...) {
            record(Outcome::Failed, description, {}, "permitted exception",
                   "non-standard operation exception", location, index);
        }
        return;
    }

    try {
        auto const verdict = Detail::evidence(
            std::invoke(std::forward<VerifyResult>(verify_result), std::as_const(*result)));
        std::string actual;
        if constexpr (std::is_void_v<OperationResult>) actual = "void";
        else actual = Detail::render(*result);
        record(outcome(verdict), description, std::move(actual), "permitted result",
               verdict == Evidence::False ? "result alternative rejected" :
               verdict == Evidence::Indeterminate ? "result postcondition indeterminate" : "",
               location, index);
    } catch (Detail::AbortCase const&) {
        throw;
    } catch (std::exception const& verification_error) {
        record(Outcome::Error, description, {}, "permitted result",
               std::string("result verifier threw: ") + verification_error.what(),
               location, index);
    } catch (...) {
        record(Outcome::Error, description, {}, "permitted result",
               "result verifier threw a non-standard exception", location, index);
    }
}

template<class Callable>
    void Context::expect_exception(Callable&& action, std::string_view description,
                          std::source_location location){
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (...) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }

template<class Callable>
    void Context::expect_no_throw(Callable&& action, std::string_view description,
                         std::source_location location){
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception", location, index); return; }
        record(Outcome::Passed, description, {}, {}, {}, location, index);
    }

namespace Detail {
inline WaitObservation make_wait(WaitStatus status,
        std::chrono::steady_clock::duration elapsed,
        std::chrono::steady_clock::duration budget, bool absolute) {
    return {status, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed),
            std::chrono::duration_cast<std::chrono::nanoseconds>(budget), absolute};
}
inline std::chrono::steady_clock::duration budget_at(
        std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point deadline) {
    return deadline > start ? deadline - start : std::chrono::steady_clock::duration::zero();
}
inline WaitStatus future_wait_status(std::future_status value) {
    switch (value) {
        case std::future_status::ready: return WaitStatus::Ready;
        case std::future_status::timeout: return WaitStatus::Timeout;
        case std::future_status::deferred: return WaitStatus::Deferred;
        default: return WaitStatus::Unexpected;
    }
}
} // namespace Detail

template<class Future, class Rep, class Period>
    void Context::expect_ready(Future& future, std::chrono::duration<Rep, Period> timeout,
                      std::string_view description,
                      std::source_location location){
        auto const index = begin_check(description);
        if (timeout < timeout.zero()) {
            record(Outcome::Error, description, {}, {}, "negative timeout", location, index,
                   Detail::make_wait(WaitStatus::InvalidTimeout,
                       std::chrono::steady_clock::duration::zero(),
                       std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
            return;
        }
        auto const start = std::chrono::steady_clock::now();
        auto const status = future.wait_for(timeout);
        auto const elapsed_time = std::chrono::steady_clock::now() - start;
        std::string observed;
        Outcome result;
        switch (status) {
            case std::future_status::ready:
                observed = "ready";
                result = Outcome::Passed;
                break;
            case std::future_status::timeout:
                observed = "timeout";
                result = Outcome::Failed;
                break;
            case std::future_status::deferred:
                observed = "deferred";
                result = Outcome::Failed;
                break;
            default:
                observed = "unexpected future status";
                result = Outcome::Error;
                break;
        }
        record(result, description, observed, "ready",
               "waited=" + Detail::render(std::chrono::duration_cast<std::chrono::milliseconds>(
                                elapsed_time).count()) + "ms; limit=" +
               Detail::render(std::chrono::duration_cast<std::chrono::milliseconds>(
                                timeout).count()) + "ms",
               location, index,

               Detail::make_wait(Detail::future_wait_status(status), elapsed_time,

                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
    }

template<class Lock, class Predicate, class Rep, class Period>
    void Context::expect_eventually(std::condition_variable_any& condition, Lock& lock,
                           Predicate&& predicate, std::chrono::duration<Rep, Period> timeout,
                           std::string_view description,
                           std::source_location location){
        auto const index = begin_check(description);
        if (timeout < timeout.zero()) {
            record(Outcome::Error, description, {}, {}, "negative timeout", location, index,
                   Detail::make_wait(WaitStatus::InvalidTimeout,
                       std::chrono::steady_clock::duration::zero(),
                       std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
            return;
        }
        auto const start = std::chrono::steady_clock::now();
        bool const completed = condition.wait_for(lock, timeout,
                                                  std::forward<Predicate>(predicate));
        auto const elapsed_time = std::chrono::steady_clock::now() - start;
        record(completed ? Outcome::Passed : Outcome::Failed, description,
               completed ? "condition satisfied" : "timeout", "condition satisfied",
               "waited=" + Detail::render(std::chrono::duration_cast<std::chrono::milliseconds>(
                                elapsed_time).count()) + "ms; limit=" +
               Detail::render(std::chrono::duration_cast<std::chrono::milliseconds>(
                                timeout).count()) + "ms",
               location, index,

               Detail::make_wait(completed ? WaitStatus::Ready : WaitStatus::Timeout, elapsed_time,

                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
    }


template<class T, class Predicate, class Rep, class Period>
void Context::expect_atomic(std::atomic<T> const& state, Predicate&& predicate,
                            std::chrono::duration<Rep, Period> timeout,
                            std::string_view description,
                            std::source_location location) {
    auto const index = begin_check(description);
    if (timeout < timeout.zero()) {
        record(Outcome::Error, description, {}, "predicate satisfied",
               "negative timeout", location, index,
               Detail::make_wait(WaitStatus::InvalidTimeout,
                   std::chrono::steady_clock::duration::zero(),
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
        return;
    }
    auto const start = std::chrono::steady_clock::now();
    auto const deadline = start +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout);
    auto observed = state.load(std::memory_order_acquire);
    bool matched = static_cast<bool>(std::invoke(predicate, observed));
    while (!matched) {
        auto const now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        auto const remaining = deadline - now;
        auto const max_pause = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::min(remaining, max_pause));
        observed = state.load(std::memory_order_acquire);
        matched = static_cast<bool>(std::invoke(predicate, observed));
    }
    auto const finished = std::chrono::steady_clock::now();
    auto const waited = std::chrono::duration_cast<std::chrono::milliseconds>(
        finished - start).count();
    auto const limit = std::chrono::duration_cast<std::chrono::milliseconds>(
        timeout).count();
    record(matched ? Outcome::Passed : Outcome::Failed, description,
           Detail::render(observed), "predicate satisfied",
           std::string(matched ? "waited=" : "timeout; waited=") +
               std::to_string(waited) + "ms; limit=" + std::to_string(limit) + "ms",
           location, index,

           Detail::make_wait(matched ? WaitStatus::Ready : WaitStatus::Timeout, finished - start,

               std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout), false));
}


namespace Detail {

// For absolute waits the original deadline is not restarted at each check.
// The measured overrun is diagnostic, not proof of worker failure or a
// benchmark of the operation's execution time.
inline std::string absolute_wait_detail(std::chrono::steady_clock::time_point started,
                                        std::chrono::steady_clock::time_point deadline,
                                        std::chrono::steady_clock::time_point finished) {
    using Clock = std::chrono::steady_clock;
    auto const remaining = deadline > started ? deadline - started : Clock::duration::zero();
    auto const overrun = finished > deadline ? finished - deadline : Clock::duration::zero();
    auto const millis = [](Clock::duration duration) {
        return std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    };
    return "waited=" + millis(finished - started) +
           "ms; remaining_at_start=" + millis(remaining) +
           "ms; past_deadline_by=" + millis(overrun) + "ms";
}

} // namespace Detail

template<class Future>
void Context::expect_ready_until(Future& future,
                                 std::chrono::steady_clock::time_point deadline,
                                 std::string_view description,
                                 std::source_location location) {
    auto const index = begin_check(description);
    auto const started = std::chrono::steady_clock::now();
    auto const status = future.wait_until(deadline);
    auto const finished = std::chrono::steady_clock::now();
    Outcome result = Outcome::Error;
    std::string observed;
    switch (status) {
        case std::future_status::ready:
            observed = "ready";
            result = Outcome::Passed;
            break;
        case std::future_status::timeout:
            observed = "timeout";
            result = Outcome::Failed;
            break;
        case std::future_status::deferred:
            observed = "deferred";
            result = Outcome::Failed;
            break;
        default:
            observed = "unexpected future status";
            result = Outcome::Error;
            break;
    }
    record(result, description, std::move(observed), "ready",
           Detail::absolute_wait_detail(started, deadline, finished), location, index,
 Detail::make_wait(Detail::future_wait_status(status), finished - started,
     Detail::budget_at(started, deadline), true));
}

template<class Lock, class Predicate>
void Context::expect_eventually_until(std::condition_variable_any& condition, Lock& lock,
                                      Predicate&& predicate,
                                      std::chrono::steady_clock::time_point deadline,
                                      std::string_view description,
                                      std::source_location location) {
    auto const index = begin_check(description);
    auto const started = std::chrono::steady_clock::now();
    bool const matched = condition.wait_until(
        lock, deadline, std::forward<Predicate>(predicate));
    auto const finished = std::chrono::steady_clock::now();
    record(matched ? Outcome::Passed : Outcome::Failed, description,
           matched ? "condition satisfied" : "timeout", "condition satisfied",
           Detail::absolute_wait_detail(started, deadline, finished), location, index,
 Detail::make_wait(matched ? WaitStatus::Ready : WaitStatus::Timeout, finished - started,
     Detail::budget_at(started, deadline), true));
}

template<class T, class Predicate>
void Context::expect_atomic_until(std::atomic<T> const& state, Predicate&& predicate,
                                  std::chrono::steady_clock::time_point deadline,
                                  std::string_view description,
                                  std::source_location location) {
    auto const index = begin_check(description);
    auto const started = std::chrono::steady_clock::now();
    auto observed = state.load(std::memory_order_acquire);
    bool matched = static_cast<bool>(std::invoke(predicate, observed));
    while (!matched) {
        auto const now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        auto const remaining = deadline - now;
        auto const max_pause = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::min(remaining, max_pause));
        observed = state.load(std::memory_order_acquire);
        matched = static_cast<bool>(std::invoke(predicate, observed));
    }
    auto const finished = std::chrono::steady_clock::now();
    record(matched ? Outcome::Passed : Outcome::Failed, description,
           Detail::render(observed), "predicate satisfied",
           (matched ? "" : "timeout; ") +
               Detail::absolute_wait_detail(started, deadline, finished),
           location, index,

           Detail::make_wait(matched ? WaitStatus::Ready : WaitStatus::Timeout, finished - started,

               Detail::budget_at(started, deadline), true));
}

template<class ManifestFn, class ObservedFn>
void Context::check_oracle_rows(ManifestFn&& manifest_fn, ObservedFn&& observed_fn,
                                std::string_view description,
                                std::source_location location) {
    auto const index = begin_check(description);
    auto const& manifest = std::invoke(std::forward<ManifestFn>(manifest_fn));
    auto const& observed = std::invoke(std::forward<ObservedFn>(observed_fn));
    record_oracle_rows(std::span<OracleRow const>{manifest},
                       std::span<OracleRow const>{observed},
                       description, location, index);
}

template<class PlanFn>
void Context::check_oracle_replay(PlanFn&& plan_fn, std::string_view description,
                                  std::source_location location) {
    auto const index = begin_check(description);
    auto const& plan = std::invoke(std::forward<PlanFn>(plan_fn));
    record_oracle_replay(plan, description, location, index);
}

template<class Callable>
    void Context::check(Callable&& predicate, std::string_view description,
               std::source_location location){
        auto const index = begin_check(description);
        auto const value = std::invoke(std::forward<Callable>(predicate));
        record(outcome(Detail::evidence(value)), description, {}, {}, {}, location, index);
    }

} // namespace Ariadne::Testing

#endif // ARIADNE_TESTING_TESTING_TPL_HPP
