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
#include <filesystem>
#include <fstream>
#include <system_error>
#include <cmath>
#include <cstdint>
#include <functional>
#include <future>
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
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

// Diagnostic events are not assertions and cannot change the test outcome.
enum class DiagnosticKind { Scope, Note, Warning, Value, Elapsed };
enum class ConsoleDetail { All, Concise };

struct DiagnosticEvent {
    DiagnosticKind kind;
    std::string description;
    std::string value;
    std::vector<std::string> context;
    std::chrono::nanoseconds duration{};
    std::string file;
    std::uint_least32_t line = 0;
};

struct AssertionRecord {
    Outcome outcome;
    std::string description;
    std::string actual;
    std::string expected;
    std::string detail;
    std::string file;
    std::uint_least32_t line;
    std::size_t check_index = 0;
    std::vector<std::string> context;
};

// The framework only records these references; adapters decide file formats.
enum class ArtifactKind { Output, Reference, Difference };
enum class ArtifactLifetime { External, Temporary, KeepOnFailure, KeepAlways };

struct ArtifactRecord {
    std::filesystem::path path;
    std::string label;
    std::string media_type;
    ArtifactKind kind = ArtifactKind::Output;
    ArtifactLifetime lifetime = ArtifactLifetime::External;
    std::vector<std::pair<std::string, std::string>> metadata;
};

struct CaseResult {
    std::string name;
    Outcome outcome = Outcome::Passed;
    std::vector<AssertionRecord> assertions;
    std::chrono::nanoseconds duration{};
    std::string suite_name;
    std::size_t case_index = 0;
    std::size_t case_total = 0;
    std::vector<DiagnosticEvent> diagnostics;
    std::vector<ArtifactRecord> artifacts;
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
    virtual void diagnostic_emitted(CaseResult const&, DiagnosticEvent const&) { }
    virtual void case_finished(CaseResult const& result) = 0;
    virtual void suite_finished(std::string_view name, Summary const& summary) = 0;
};

class ConsoleReporter final : public Reporter {
  public:
    explicit ConsoleReporter(std::ostream& output, ConsoleDetail detail = ConsoleDetail::All);
    void diagnostic_emitted(CaseResult const& result, DiagnosticEvent const& event) override;
    void case_started(CaseResult const& result) override;
    void check_started(CaseResult const& result, std::size_t index,
                       std::string_view description) override;
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::ostream& _output;
    ConsoleDetail _detail;
};

namespace Detail {

template<class T>
Evidence evidence(T const& value) {
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

// Use the same evidence-preserving equality and integral signedness rules
// for ordinary checks and exception payload comparisons.
template<class Actual, class Expected>
Evidence equal_evidence(Actual const& actual, Expected const& expected) {
    using A = std::remove_cvref_t<Actual>;
    using B = std::remove_cvref_t<Expected>;
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>
                  && !std::is_same_v<A, bool> && !std::is_same_v<B, bool>) {
        return evidence(std::cmp_equal(actual, expected));
    } else {
        return evidence(actual == expected);
    }
}

// Retain non-copyable lvalue callables by reference and own temporaries.
// Wrapping named functions also avoids MSVC's inability to deduce a
 // decltype(auto) return directly from a function designator.
template<class Callable>
auto capture_callable(Callable&& callable) {
    if constexpr (std::is_lvalue_reference_v<Callable>)
        return std::ref(callable);
    else
        return std::forward<Callable>(callable);
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

// Internal case-abort marker, not a user-observable test exception.
struct AbortCase {};

} // namespace Detail

class Context {
  public:
    // Move-only guard: a nested diagnostic scope always restores its parent.
    class Scope {
      public:
        Scope(Scope const&) = delete;
        Scope& operator=(Scope const&) = delete;
        Scope(Scope&& other) noexcept;
        Scope& operator=(Scope&&) = delete;
        ~Scope() noexcept;
      private:
        friend class Context;
        Scope(Context& context, std::string label, std::source_location location);
        Context* _context;
        std::size_t _depth;
        int _uncaught;
    };

    explicit Context(CaseResult& result, Reporter* reporter = nullptr)
        : _result(result), _reporter(reporter) { }

    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;
    ~Context() noexcept;

    // Lazily allocated, uniquely created directory for this case's outputs.
    std::filesystem::path const& work_directory();

    // Attach metadata only. Never writes or regenerates a reference file.
    void artifact(std::filesystem::path path, std::string label,
                  std::string media_type = "application/octet-stream",
                  ArtifactKind kind = ArtifactKind::Output,
                  ArtifactLifetime lifetime = ArtifactLifetime::External,
                  std::vector<std::pair<std::string,std::string>> metadata = {});

    // File expectations issue CHECK before inspecting the filesystem.
    void expect_file_exists(std::filesystem::path const& path,
                            std::string_view description,
                            std::source_location location = std::source_location::current());
    void expect_file_equal(std::filesystem::path const& actual,
                           std::filesystem::path const& reference,
                           std::string_view description,
                           std::source_location location = std::source_location::current());
    void expect_file_signature(std::filesystem::path const& actual,
                               std::string_view signature,
                               std::string_view description,
                               std::source_location location = std::source_location::current());


    Scope scope(std::string label,
                std::source_location location = std::source_location::current());

    template<class T>
    Scope scope(std::string name, T const& value,
                std::source_location location = std::source_location::current()) {
        return scope(std::move(name) + "=" + Detail::render(value), location);
    }

    void note(std::string description,
              std::source_location location = std::source_location::current());
    void warning(std::string description,
                 std::source_location location = std::source_location::current());

    template<class T>
    void inspect(std::string name, T const& value,
                 std::source_location location = std::source_location::current()) {
        emit(DiagnosticKind::Value, std::move(name), Detail::render(value), {}, location);
    }

    void elapsed(std::string description, std::chrono::nanoseconds duration,
                 std::source_location location = std::source_location::current());

    // For an arbitrary operation, emit its duration even if it throws.
    // A hanging operation is diagnosed by the preceding scope event.
    template<class Callable>
    void measure(std::string description, Callable&& operation,
                 std::source_location location = std::source_location::current()) {
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
    void prove(T const& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        record(outcome(Detail::evidence(predicate)), description, {}, {}, {}, location, index);
    }

    // Test a specified mathematical evidence classification. A deliberately
    // expected indeterminate result is a passing test, not a proven statement.
    template<class PredicateFn>
    void expect_evidence(PredicateFn&& predicate_fn, EvidenceExpectation expected,
                         std::string_view description,
                         std::source_location location = std::source_location::current()) {
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

    // Deferred operands: emit CHECK before evaluating each expression once.
    template<class ActualFn, class ExpectedFn>
    void check_equal(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                     std::string_view description,
                     std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) actual = std::invoke(std::forward<ActualFn>(actual_fn));
        decltype(auto) expected = std::invoke(std::forward<ExpectedFn>(expected_fn));
        auto const verdict = Detail::equal_evidence(actual, expected);
        record(outcome(verdict), description,
               Detail::render(actual), Detail::render(expected), {}, location, index);
    }

    // Compare two input ranges without copying their elements. Both ranges
    // and the comparator/path selectors are evaluated once after CHECK.
    // A non-true predicate is retained as evidence, not coerced to bool.
    template<class ActualFn, class ExpectedFn, class ComparatorFn, class PathFn>
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, PathFn&& path_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current()) {
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

        // Count mismatches always fail, even when earlier elements are
        // mathematically indeterminate.
        auto const status = first_failure || actual_count != expected_count
            ? Outcome::Failed : first_unknown ? Outcome::Inconclusive
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
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, std::string_view description,
                      std::source_location location = std::source_location::current()) {
        check_ranges(std::forward<ActualFn>(actual_fn),
                     std::forward<ExpectedFn>(expected_fn),
                     std::forward<ComparatorFn>(comparator_fn),
                     [] { return [](std::size_t index) { return std::to_string(index); }; },
                     description, location);
    }

    template<class ActualFn, class ExpectedFn>
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current()) {
        check_ranges(std::forward<ActualFn>(actual_fn),
                     std::forward<ExpectedFn>(expected_fn),
                     [] { return [](auto const& a, auto const& b) {
                         return Detail::equal_evidence(a, b);
                     }; },
                     description, location);
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
    void check_unary(Predicate&& predicate, OperandFn&& operand_fn,
                     std::string_view description, bool negate = false,
                     std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) operand = std::invoke(std::forward<OperandFn>(operand_fn));
        auto verdict = Detail::evidence(std::invoke(std::forward<Predicate>(predicate), operand));
        if (negate) verdict = Detail::negate(verdict);
        record(outcome(verdict), description, Detail::render(operand), {}, {}, location, index);
    }

    template<class Predicate, class LeftFn, class RightFn>
    void check_binary(Predicate&& predicate, LeftFn&& left_fn, RightFn&& right_fn,
                      std::string_view description, bool negate = false,
                      std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        decltype(auto) left = std::invoke(std::forward<LeftFn>(left_fn));
        decltype(auto) right = std::invoke(std::forward<RightFn>(right_fn));
        auto verdict = Detail::evidence(std::invoke(std::forward<Predicate>(predicate), left, right));
        if (negate) verdict = Detail::negate(verdict);
        record(outcome(verdict), description,
               Detail::render(left), Detail::render(right), {}, location, index);
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
        record(outcome(Detail::evidence(relation)), description,
               Detail::render(actual), Detail::render(expected), {}, location, index);
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
        auto const error = Detail::magnitude(actual - expected);
        auto const relation = (error <= tolerance);
        record(outcome(Detail::evidence(relation)), description,
               Detail::render(actual), Detail::render(expected),
               "error=" + Detail::render(error) + ", tolerance=" + Detail::render(tolerance), location, index);
    }

    template<class A, class B, class Tolerance>
    void expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description = "bounded error",
                       std::source_location location = std::source_location::current()) {
        check_within([&]() -> A const& { return actual; },
                     [&]() -> B const& { return expected; },
                     [&]() -> Tolerance const& { return tolerance; }, description, location);
    }

    // Stop only this case if a prerequisite is false or indeterminate.
    // The check is recorded before aborting, retaining its original outcome.
    template<class Callable>
    void require(Callable&& predicate, std::string_view description,
                 std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const evidence = Detail::evidence(std::invoke(std::forward<Callable>(predicate)));
        auto const status = outcome(evidence);
        record(status, description, {}, {}, {}, location, index);
        if (status != Outcome::Passed) throw Detail::AbortCase{};
    }

    template<class Exception, class Callable>
    void expect_throws(Callable&& action, std::string_view description = "expected exception",
                       std::source_location location = std::source_location::current()) {
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
    // Both the exception type and the message substring must match.
    template<class Exception, class Callable>
    void expect_throws_message(Callable&& action, std::string_view substring,
                               std::string_view description = "expected exception message",
                               std::source_location location = std::source_location::current()) {
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

    // Extract one caller-selected field from a typed exception and compare
    // it with a separately evaluated expectation. Both are deferred until
    // after CHECK and the tested operation; no domain dependency is needed.
    template<class Exception, class Action, class ProjectorFn, class ExpectedFn>
    void expect_throws_payload(Action&& action, ProjectorFn&& projector_fn,
                               ExpectedFn&& expected_fn, std::string_view description,
                               std::source_location location = std::source_location::current()) {
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

    template<class Callable>
    void expect_exception(Callable&& action, std::string_view description = "exception expected",
                          std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (...) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }
    template<class Callable>
    void expect_no_throw(Callable&& action, std::string_view description = "no exception expected",
                         std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Detail::AbortCase const&) { throw; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception", location, index); return; }
        record(Outcome::Passed, description, {}, {}, {}, location, index);
    }

    // Bounded waits observe externally synchronized state; they do not
    // cancel worker threads. The caller owns the future and its lifetime.
    template<class Future, class Rep, class Period>
    void expect_ready(Future& future, std::chrono::duration<Rep, Period> timeout,
                      std::string_view description,
                      std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        if (timeout < timeout.zero()) {
            record(Outcome::Error, description, {}, {}, "negative timeout", location, index);
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
               location, index);
    }

    // Use wait_for's predicate overload to handle spurious wakeups. The
    // predicate executes while the caller's unique_lock is held.
    template<class Lock, class Predicate, class Rep, class Period>
    void expect_eventually(std::condition_variable_any& condition, Lock& lock,
                           Predicate&& predicate, std::chrono::duration<Rep, Period> timeout,
                           std::string_view description,
                           std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        if (timeout < timeout.zero()) {
            record(Outcome::Error, description, {}, {}, "negative timeout", location, index);
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
               location, index);
    }

    // Deferred checker: emit progress before evaluating its potentially costly predicate.
    template<class Callable>
    void check(Callable&& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const value = std::invoke(std::forward<Callable>(predicate));
        record(outcome(Detail::evidence(value)), description, {}, {}, {}, location, index);
    }

    void skip(std::string_view reason,
              std::source_location location = std::source_location::current());

  private:
    friend class Suite;
    static Outcome outcome(Evidence value);
    std::size_t enter_scope(std::string label, std::source_location location);
    void leave_scope(std::size_t depth, int uncaught_at_entry) noexcept;
    void emit(DiagnosticKind kind, std::string description, std::string value,
              std::chrono::nanoseconds duration, std::source_location location);
    std::size_t begin_check(std::string_view description);
    void record(Outcome outcome, std::string_view description, std::string actual,
                std::string expected, std::string detail, std::source_location location,
                std::size_t index);
    CaseResult& _result;
    Reporter* _reporter;
    std::size_t _check_count = 0;
    std::vector<std::string> _scopes;
    std::vector<std::string> _unwinding_scopes;
    std::optional<std::filesystem::path> _work_directory;
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
