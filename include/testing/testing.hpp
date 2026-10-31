/***************************************************************************
 *            testing.hpp
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

/** @file testing/testing.hpp
 *  @brief Public test outcomes, reporters, case contexts, assertions, and suites.
 *
 *  All checks classify a result as Passed, Failed, Inconclusive, Skipped, or
 *  Error. Deferred checks evaluate arguments once, after check_started().
 *  Template implementations are provided by testing.tpl.hpp.
 */
#ifndef ARIADNE_TESTING_TESTING_HPP
#define ARIADNE_TESTING_TESTING_HPP

#include <chrono>
#include <atomic>
#include <thread>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <cmath>
#include <cstdint>
#include <functional>
#include <future>
#include <exception>
#include <variant>
#include <initializer_list>
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

/** @brief Three-valued mathematical evidence; Indeterminate is not a boolean pass. */
enum class Evidence { False, True, Indeterminate };
/** @brief Operational classification of a case or assertion. */
enum class Outcome { Passed, Failed, Inconclusive, Skipped, Error };

// The relation is selected at compile time; predicate evidence stays three-valued.
/** @brief Compile-time selection of a binary comparison relation. */
enum class Comparison { NotEqual, Less, LessEqual, Greater, GreaterEqual };

// Desired evidence, not an algorithm's return code or an operational outcome.
/** @brief Expectation for a three-valued logical result. */
enum class EvidenceExpectation { DefinitelyTrue, PossiblyTrue, NotPossibly, Indeterminate };

// Diagnostic events are not assertions and cannot change the test outcome.
/** @brief Kind of non-asserting diagnostic notification. */
enum class DiagnosticKind { Scope, Note, Warning, Value, Elapsed };
/** @brief Verbose or concise console diagnostic rendering. */
enum class ConsoleDetail { All, Concise };

/** @brief Diagnostic event emitted during case execution; it does not change the outcome. */
struct DiagnosticEvent {
    DiagnosticKind kind;
    std::string description;
    std::string value;
    std::vector<std::string> context;
    std::chrono::nanoseconds duration{};
    std::string file;
    std::uint_least32_t line = 0;
};

/** @brief Observed end state of a bounded wait, without diagnosing its cause. */
enum class WaitStatus { Ready, Timeout, Deferred, InvalidTimeout, Unexpected };

/** @brief Structured observation for a bounded wait assertion.
 *  @details `elapsed` is measured waiting time, not a benchmark.
 *  `available_budget` is the relative timeout or time remaining when
 *  an absolute-deadline check began. It is negative for invalid durations.
 *  A timeout does not establish deadlock, starvation or scheduler slowdown.
 */
struct WaitObservation {
    WaitStatus status;
    std::chrono::nanoseconds elapsed{};
    std::chrono::nanoseconds available_budget{};
    bool absolute_deadline = false;
};

/** @brief Captured assertion, including source location and nested diagnostic context. */
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
    // Empty for ordinary assertions and nonasserting elapsed diagnostics.
    std::optional<WaitObservation> wait;
};

// The framework only records these references; adapters decide file formats.
/** @brief Role of a registered artifact, independent of its retention policy. */
enum class ArtifactKind { Output, Reference, Difference };
/** @brief Ownership and retention policy for an artifact's path. */
enum class ArtifactLifetime { External, Temporary, KeepOnFailure, KeepAlways };

/** @brief Metadata reference for an output, reference or difference artifact. */
struct ArtifactRecord {
    std::filesystem::path path;
    std::string label;
    std::string media_type;
    ArtifactKind kind = ArtifactKind::Output;
    ArtifactLifetime lifetime = ArtifactLifetime::External;
    std::vector<std::pair<std::string, std::string>> metadata;
};

/** @brief Per-case execution result, assertions, diagnostics and artifacts. */
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
/** @brief Whether an inconclusive case blocks overall acceptance. */
enum class InconclusivePolicy { Blocking, NonBlocking };
/** @brief Whether a skipped case blocks overall acceptance. */
enum class SkippedPolicy { NonBlocking, Blocking };

/** @brief Exit-gate policy; does not rewrite assertion classifications. */
struct AcceptancePolicy {
    InconclusivePolicy inconclusive = InconclusivePolicy::Blocking;
    SkippedPolicy skipped = SkippedPolicy::NonBlocking;
};

/** @brief Aggregated counts and the exit-gate acceptance decision. */
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

/** @brief Callback interface for live case and diagnostic reporting. */
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

/** @brief Human-readable reporter with configurable diagnostic verbosity. */
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

/** @cond INTERNAL */
namespace Detail {

template<class T>
Evidence evidence(T const& value);

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
std::string render(T const& value);

// Use the same evidence-preserving equality and integral signedness rules
// for ordinary checks and exception payload comparisons.
template<class Actual, class Expected>
Evidence equal_evidence(Actual const& actual, Expected const& expected);

// Retain non-copyable lvalue callables by reference and own temporaries.
// Wrapping named functions also avoids MSVC's inability to deduce a
 // decltype(auto) return directly from a function designator.
template<class Callable>
auto capture_callable(Callable&& callable);

template<class T>
auto magnitude(T const& value);

// Internal case-abort marker, not a user-observable test exception.
struct AbortCase {};

} // namespace Detail
/** @endcond */

/** @brief Per-case context for assertions, diagnostics, files and fatal prerequisites. */
class WorkerBatch;

class Context {
  public:
    // Move-only guard: a nested diagnostic scope always restores its parent.
/** @brief Move-only RAII guard for a nested diagnostic scope. */
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
    /** @brief Return this case's lazily allocated private work directory.
     *  @return Owned path, unique across concurrent test processes.
     */
    std::filesystem::path const& work_directory();

    // Attach metadata only. Never writes or regenerates a reference file.
    /** @brief Register artifact metadata without modifying the referenced file.
     *  @throws std::invalid_argument for empty identifiers or managed paths
     *          outside the current case directory (including symlink escapes).
     */
    void artifact(std::filesystem::path path, std::string label,
                  std::string media_type = "application/octet-stream",
                  ArtifactKind kind = ArtifactKind::Output,
                  ArtifactLifetime lifetime = ArtifactLifetime::External,
                  std::vector<std::pair<std::string,std::string>> metadata = {});

    // File expectations issue CHECK before inspecting the filesystem.
    /** @brief Check whether a path is an accessible regular file. */
    void expect_file_exists(std::filesystem::path const& path,
                            std::string_view description,
                            std::source_location location = std::source_location::current());
    /** @brief Compare binary files byte for byte and record the first difference. */
    void expect_file_equal(std::filesystem::path const& actual,
                           std::filesystem::path const& reference,
                           std::string_view description,
                           std::source_location location = std::source_location::current());
    /** @brief Compare a file's initial bytes against a binary signature. */
    void expect_file_signature(std::filesystem::path const& actual,
                               std::string_view signature,
                               std::string_view description,
                               std::source_location location = std::source_location::current());


    Scope scope(std::string label,
                std::source_location location = std::source_location::current());

    /** @brief Begin a named diagnostic scope. The returned guard restores its parent on destruction.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class T>
    Scope scope(std::string name, T const& value,
                std::source_location location = std::source_location::current());

    void note(std::string description,
              std::source_location location = std::source_location::current());
    void warning(std::string description,
                 std::source_location location = std::source_location::current());

    /** @brief Record a named value without changing the case outcome.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class T>
    void inspect(std::string name, T const& value,
                 std::source_location location = std::source_location::current());

    void elapsed(std::string description, std::chrono::nanoseconds duration,
                 std::source_location location = std::source_location::current());

    // For an arbitrary operation, emit its duration even if it throws.
    // A hanging operation is diagnosed by the preceding scope event.
    /** @brief Measure a void operation and emit elapsed time even if it throws; exceptions propagate.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Callable>
    void measure(std::string description, Callable&& operation,
                 std::source_location location = std::source_location::current());

    /** @brief Record a direct mathematical proposition, retaining indeterminate evidence.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class T>
    void prove(T const& predicate, std::string_view description,
               std::source_location location = std::source_location::current());

    // Test a specified mathematical evidence classification. A deliberately
    // expected indeterminate result is a passing test, not a proven statement.
    /** @brief Compare a deferred proposition against the specified three-valued expectation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class PredicateFn>
    void expect_evidence(PredicateFn&& predicate_fn, EvidenceExpectation expected,
                         std::string_view description,
                         std::source_location location = std::source_location::current());

    // Deferred operands: emit CHECK before evaluating each expression once.
    /** @brief Evaluate deferred operands once and compare their equality.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn>
    void check_equal(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                     std::string_view description,
                     std::source_location location = std::source_location::current());

    // Compare two input ranges without copying their elements. Both ranges
    // and the comparator/path selectors are evaluated once after CHECK.
    // A non-true predicate is retained as evidence, not coerced to bool.
    /** @brief Compare input ranges, retaining first mismatch, uncertainty and element path.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn, class ComparatorFn, class PathFn>
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, PathFn&& path_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current());

    /** @brief Compare input ranges, retaining first mismatch, uncertainty and element path.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn, class ComparatorFn>
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      ComparatorFn&& comparator_fn, std::string_view description,
                      std::source_location location = std::source_location::current());

    /** @brief Compare input ranges, retaining first mismatch, uncertainty and element path.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn>
    void check_ranges(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current());

    // Each relation is selected at compile time and evaluates its operands once,
    // left-to-right, after the live CHECK notification.
    /** @brief Compare deferred operands with the selected compile-time relation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<Comparison Relation, class ActualFn, class ExpectedFn>
    void check_compare(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                       std::string_view description,
                       std::source_location location = std::source_location::current());

    /** @brief Apply a deferred unary predicate with optional evidence negation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Predicate, class OperandFn>
    void check_unary(Predicate&& predicate, OperandFn&& operand_fn,
                     std::string_view description, bool negate = false,
                     std::source_location location = std::source_location::current());

    /** @brief Apply a deferred binary predicate with optional evidence negation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Predicate, class LeftFn, class RightFn>
    void check_binary(Predicate&& predicate, LeftFn&& left_fn, RightFn&& right_fn,
                      std::string_view description, bool negate = false,
                      std::source_location location = std::source_location::current());

    /** @brief Convenience equality check for already-evaluated operands.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class A, class B>
    void expect_equal(A const& actual, B const& expected,
                      std::string_view description = "equality",
                      std::source_location location = std::source_location::current());

    /** @brief Compare semantic identity through the ADL-discovered same() operation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn>
    void check_same(ActualFn&& actual_fn, ExpectedFn&& expected_fn,
                    std::string_view description,
                    std::source_location location = std::source_location::current());

    /** @brief Convenience semantic identity check for values.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class A, class B>
    void expect_same(A const& actual, B const& expected,
                     std::string_view description = "identity",
                     std::source_location location = std::source_location::current());

    /** @brief Compare a computed error against a tolerance.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class ActualFn, class ExpectedFn, class ToleranceFn>
    void check_within(ActualFn&& actual_fn, ExpectedFn&& expected_fn, ToleranceFn&& tolerance_fn,
                      std::string_view description,
                      std::source_location location = std::source_location::current());

    /** @brief Convenience bounded-error check for already-evaluated values.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class A, class B, class Tolerance>
    void expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description = "bounded error",
                       std::source_location location = std::source_location::current());

    // Stop only this case if a prerequisite is false or indeterminate.
    // The check is recorded before aborting, retaining its original outcome.
    /** @brief Record a prerequisite and abort only the current case if not Passed.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Callable>
    void require(Callable&& predicate, std::string_view description,
                 std::source_location location = std::source_location::current());

    /** @brief Require a particular exception type from an operation.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Exception, class Callable>
    void expect_throws(Callable&& action, std::string_view description = "expected exception",
                       std::source_location location = std::source_location::current());
    // Both the exception type and the message substring must match.
    /** @brief Require an exception type and a substring in its message.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Exception, class Callable>
    void expect_throws_message(Callable&& action, std::string_view substring,
                               std::string_view description = "expected exception message",
                               std::source_location location = std::source_location::current());

    // Extract one caller-selected field from a typed exception and compare
    // it with a separately evaluated expectation. Both are deferred until
    // after CHECK and the tested operation; no domain dependency is needed.
    /** @brief Project an expected exception into a value and compare its payload.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Exception, class Action, class ProjectorFn, class ExpectedFn>
    void expect_throws_payload(Action&& action, ProjectorFn&& projector_fn,
                               ExpectedFn&& expected_fn, std::string_view description,
                               std::source_location location = std::source_location::current());

    /** @brief Check an allowed typed domain status and conditional postcondition.
     *  CHECK is announced before evaluating the operation. Only accepted exact
     *  statuses are validated; an expected UNKNOWN is not Inconclusive.
     */
    template<class Operation, class Selector, class Status, class Verify>
    void expect_status(Operation&& operation, Selector&& selector,
                       std::initializer_list<Status> permitted, Verify&& verify,
                       std::string_view description,
                       std::source_location location = std::source_location::current());

    /** @brief Check only a typed domain status, without a postcondition. */
    template<class Operation, class Selector, class Status>
    void expect_status(Operation&& operation, Selector&& selector,
                       std::initializer_list<Status> permitted,
                       std::string_view description,
                       std::source_location location = std::source_location::current());

    // An alternative is accepted only if its validator returns true/proven.
    // Only the operation's exceptions are candidates for the exception path.
    /** @brief Check a returned result or an allowed std::exception alternative.
     *  @details The operation runs once after CHECK. The result validator
     *  receives const Result& (std::monostate for void operations); the
     *  exception validator receives const std::exception& and may inspect
     *  derived types and their partial-result payloads using dynamic_cast.
     *  Each verifier returns boolean or three-valued evidence. Rejection is
     *  Failed, uncertainty Inconclusive, and verifier exceptions Error.
     *  Non-standard operation exceptions are rejected. Reference-returning
     *  operations are intentionally unsupported.
     */
    template<class Operation, class VerifyResult, class VerifyException>
    void expect_alternative(Operation&& operation, VerifyResult&& verify_result,
                            VerifyException&& verify_exception,
                            std::string_view description,
                            std::source_location location = std::source_location::current());

    /** @brief Verify that an operation throws any exception.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Callable>
    void expect_exception(Callable&& action, std::string_view description = "exception expected",
                          std::source_location location = std::source_location::current());
    /** @brief Verify that an operation completes without throwing.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Callable>
    void expect_no_throw(Callable&& action, std::string_view description = "no exception expected",
                         std::source_location location = std::source_location::current());

    // Bounded waits observe externally synchronized state; they do not
    // cancel worker threads. The caller owns the future and its lifetime.
    /** @brief Check a future's readiness within a nonnegative timeout.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Future, class Rep, class Period>
    void expect_ready(Future& future, std::chrono::duration<Rep, Period> timeout,
                      std::string_view description,
                      std::source_location location = std::source_location::current());

    // Use wait_for's predicate overload to handle spurious wakeups. The
    // predicate executes while the caller's unique_lock is held.
    /** @brief Wait on a condition variable until a predicate succeeds or timeout expires.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Lock, class Predicate, class Rep, class Period>
    void expect_eventually(std::condition_variable_any& condition, Lock& lock,
                           Predicate&& predicate, std::chrono::duration<Rep, Period> timeout,
                           std::string_view description,
                           std::source_location location = std::source_location::current());

    // std::atomic::wait has no timeout in C++20. Poll a load with a
    // steady-clock deadline and a bounded backoff; do not block indefinitely.
    // Only the owning case thread may use this Context and its Reporter.
    /** @brief Check that externally updated atomic state satisfies a predicate.
     *  @details The predicate must be a nonblocking boolean condition on the
     *  observed value. Atomic loads use acquire ordering, suitable for a
     *  producer that publishes progress with release. No worker is cancelled
     *  or joined by this API. It reports the last observed value and timeout.
     */
    template<class T, class Predicate, class Rep, class Period>
    void expect_atomic(std::atomic<T> const& state, Predicate&& predicate,
                       std::chrono::duration<Rep, Period> timeout,
                       std::string_view description,
                       std::source_location location = std::source_location::current());

    // Absolute deadlines can be reused across sequential waits without
    // granting a fresh full timeout to every operation. An expired deadline
    // still permits an immediate observation of an already-satisfied state.
    /** @brief Check future readiness against a shared steady-clock deadline.
     *  @note A deferred future fails; the future is never consumed.
     */
    template<class Future>
    void expect_ready_until(Future& future,
                            std::chrono::steady_clock::time_point deadline,
                            std::string_view description,
                            std::source_location location = std::source_location::current());

    /** @brief Wait for a condition predicate until an absolute deadline.
     *  @note The caller owns and locks the synchronization state.
     */
    template<class Lock, class Predicate>
    void expect_eventually_until(std::condition_variable_any& condition, Lock& lock,
                                 Predicate&& predicate,
                                 std::chrono::steady_clock::time_point deadline,
                                 std::string_view description,
                                 std::source_location location = std::source_location::current());

    /** @brief Observe atomic progress using one shared absolute deadline.
     *  @note Loads use acquire ordering; predicate executes on the case thread.
     *  A timeout means the required observation was not made before the
     *  deadline; it cannot establish a deadlock or identify scheduler delays.
     */
    template<class T, class Predicate>
    void expect_atomic_until(std::atomic<T> const& state, Predicate&& predicate,
                             std::chrono::steady_clock::time_point deadline,
                             std::string_view description,
                             std::source_location location = std::source_location::current());

    // Deferred checker: emit progress before evaluating its potentially costly predicate.
    /** @brief Record the evidence of a deferred proposition.
 *  @note Arguments supplied as callables are evaluated at check time.
 */
template<class Callable>
    void check(Callable&& predicate, std::string_view description,
               std::source_location location = std::source_location::current());

    void skip(std::string_view reason,
              std::source_location location = std::source_location::current());

  private:
    friend class WorkerBatch;
    friend /** @brief Ordered collection of cases executed with a reporter. */
class Suite;
    static Outcome outcome(Evidence value);
    std::size_t enter_scope(std::string label, std::source_location location);
    void leave_scope(std::size_t depth, int uncaught_at_entry) noexcept;
    void emit(DiagnosticKind kind, std::string description, std::string value,
              std::chrono::nanoseconds duration, std::source_location location);
    std::size_t begin_check(std::string_view description);
    void record(Outcome outcome, std::string_view description, std::string actual,
                std::string expected, std::string detail, std::source_location location,
                std::size_t index,
                std::optional<WaitObservation> wait = std::nullopt);
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

#include "testing/testing.tpl.hpp"

#endif // ARIADNE_TESTING_TESTING_HPP
