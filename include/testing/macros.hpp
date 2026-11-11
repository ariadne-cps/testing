/***************************************************************************
 *            testing/macros.hpp
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

/** @file testing/macros.hpp
 *  @brief The single home of all public ARIADNE_TESTING_* macros.
 *  @details Use C++ APIs where possible; macros preserve expression text,
 *  source locations and compile-time diagnostics. No compatibility aliases.
 */
#ifndef ARIADNE_TESTING_MACROS_HPP
#define ARIADNE_TESTING_MACROS_HPP

#include <sstream>
#include <type_traits>

#include "testing/fixture.hpp"
#include "testing/parameterized.hpp"
#include "testing/testing.hpp"

// Compile-time constraints; these produce static_assert diagnostics.
/** @def ARIADNE_TESTING_COMPILE_CONCEPT
 *  @brief Require a C++20 concept expression at compile time.
 */
#define ARIADNE_TESTING_COMPILE_CONCEPT(condition) \
    static_assert((condition), "ARIADNE compile-time concept requirement failed: " #condition)

/** @def ARIADNE_TESTING_COMPILE_TRAIT
 *  @brief Require a compile-time boolean trait.
 */
#define ARIADNE_TESTING_COMPILE_TRAIT(condition) \
    static_assert((condition), "ARIADNE compile-time trait requirement failed: " #condition)

/** @def ARIADNE_TESTING_COMPILE_SAME_TYPE
 *  @brief Require exact type identity rather than convertibility.
 */
#define ARIADNE_TESTING_COMPILE_SAME_TYPE(first, second) \
    static_assert(std::is_same_v<first, second>,         \
                  "ARIADNE compile-time type identity failed: " #first " vs " #second)

/** @brief Capture an informative stream expression in the active test context.
 *  @details Information is available to all reporters and does not count as a check.
 *  Concise console output replays it only for problematic cases.
 */
#define ARIADNE_TESTING_NOTE_CTX(context, stream_expression) \
    do {                                                     \
        std::ostringstream ariadne_testing_note_stream;      \
        ariadne_testing_note_stream << stream_expression;    \
        (context).note(ariadne_testing_note_stream.str());   \
    } while (false)

/** @brief Fixture shorthand for an informative stream expression. */
#define ARIADNE_TESTING_NOTE(stream_expression) \
    ARIADNE_TESTING_NOTE_CTX(this->ctx(), stream_expression)

/** @brief Capture a named typed diagnostic value without counting an assertion. */
#define ARIADNE_TESTING_INSPECT_CTX(context, name, value) \
    (context).inspect((name), (value))

/** @brief Fixture shorthand for a named typed diagnostic value. */
#define ARIADNE_TESTING_INSPECT(name, value) \
    ARIADNE_TESTING_INSPECT_CTX(this->ctx(), name, value)

// Runtime assertions; explicit Context forms and fixture shorthands.
/** @name Explicit-context assertion macros
 *  @brief Usable in lambdas, free functions and fixture methods.
 *  @details Operands are deferred and stringified for diagnostics. Parenthesize
 *  expressions containing top-level commas (e.g. braced values).
 *  @{ */

/** @def ARIADNE_TESTING_PROVE_CTX
 *  @brief Record mathematical evidence from a deferred proposition, retaining uncertainty.
 */
#define ARIADNE_TESTING_PROVE_CTX(context, expression) \
    (context).check([&]() -> decltype(auto) { return (expression); }, #expression)

/** @def ARIADNE_TESTING_REQUIRE_CTX
 *  @brief Require a proven prerequisite; a failure or inconclusive result aborts the case.
 */
#define ARIADNE_TESTING_REQUIRE_CTX(context, expression) \
    (context).require([&]() -> decltype(auto) { return (expression); }, #expression)

/** @def ARIADNE_TESTING_EXPECT_EQUAL_CTX
 *  @brief Compare two expressions once using evidence-preserving equality.
 */
#define ARIADNE_TESTING_EXPECT_EQUAL_CTX(context, actual, expected)                                                    \
    (context).check_equal_snapshot(                                                                                    \
        [&](bool capture_passing_values) {                                                                             \
            return ([&](auto const& observed_actual) {                                                                 \
                return ::Ariadne::Testing::Detail::observe_equal(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                              \
        },                                                                                                             \
        #actual " == " #expected)

/** @def ARIADNE_TESTING_EXPECT_RANGES_CTX
 *  @brief Compare two ranges, preserving the first mismatch and unknown element.
 */
#define ARIADNE_TESTING_EXPECT_RANGES_CTX(context, actual, expected)     \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
                           [&]() -> decltype(auto) { return (expected); }, #actual " ranges equal " #expected)

/** @def ARIADNE_TESTING_EXPECT_RANGES_BY_CTX
 *  @brief Compare ranges with a caller-provided element comparator.
 */
#define ARIADNE_TESTING_EXPECT_RANGES_BY_CTX(context, actual, expected, comparator) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); },            \
                           [&]() -> decltype(auto) { return (expected); },          \
                           [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, #actual " ranges by " #expected)

/** @def ARIADNE_TESTING_EXPECT_RANGES_AT_CTX
 *  @brief Compare ranges using a comparator and a mismatch-path formatter.
 */
#define ARIADNE_TESTING_EXPECT_RANGES_AT_CTX(context, actual, expected, comparator, path)              \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); },                               \
                           [&]() -> decltype(auto) { return (expected); },                             \
                           [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, \
                           [&]() { return ::Ariadne::Testing::Detail::capture_callable(path); }, #actual " ranges at " #expected)

/** @def ARIADNE_TESTING_EXPECT_SAME_CTX
 *  @brief Check semantic identity using the available same() relation.
 */
#define ARIADNE_TESTING_EXPECT_SAME_CTX(context, actual, expected) \
    (context).check_same(                                          \
        [&]() -> decltype(auto) { return (actual); },              \
        [&]() -> decltype(auto) { return (expected); },            \
        #actual " same " #expected)

/** @def ARIADNE_TESTING_EXPECT_WITHIN_CTX
 *  @brief Check that the absolute or domain-specific difference is within a tolerance.
 */
#define ARIADNE_TESTING_EXPECT_WITHIN_CTX(context, actual, expected, tolerance) \
    (context).check_within(                                                     \
        [&]() -> decltype(auto) { return (actual); },                           \
        [&]() -> decltype(auto) { return (expected); },                         \
        [&]() -> decltype(auto) { return (tolerance); },                        \
        #actual " within " #tolerance " of " #expected)

/** @def ARIADNE_TESTING_EXPECT_THROWS_CTX
 *  @brief Require the specified exception type from the statement.
 */
#define ARIADNE_TESTING_EXPECT_THROWS_CTX(context, exception_type, statement) \
    (context).template expect_throws<exception_type>([&] { statement; },      \
                                                     #statement " throws " #exception_type)

/** @def ARIADNE_TESTING_EXPECT_THROWS_MESSAGE_CTX
 *  @brief Require the specified exception type and a message substring.
 */
#define ARIADNE_TESTING_EXPECT_THROWS_MESSAGE_CTX(context, exception_type, statement, substring) \
    (context).template expect_throws_message<exception_type>([&] { statement; }, (substring),    \
                                                             #statement " throws " #exception_type " containing " #substring)

/** @def ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD_CTX
 *  @brief Require an exception type and check a projected payload against an expected value.
 */
#define ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD_CTX(context, exception_type, statement, projector, expected)    \
    (context).template expect_throws_payload<exception_type>([&] { statement; },                              \
                                                             [&]() -> decltype(auto) { return (projector); }, \
                                                             [&]() -> decltype(auto) { return (expected); },  \
                                                             #statement " throws " #exception_type " with " #projector " == " #expected)

/** @def ARIADNE_TESTING_EXPECT_EXCEPTION_CTX
 *  @brief Verify that the statement throws an exception.
 */
#define ARIADNE_TESTING_EXPECT_EXCEPTION_CTX(context, statement) \
    (context).expect_exception([&] { statement; }, #statement " throws")

/** @def ARIADNE_TESTING_EXPECT_NO_THROW_CTX
 *  @brief Verify that the statement completes without throwing.
 */
#define ARIADNE_TESTING_EXPECT_NO_THROW_CTX(context, statement) \
    (context).expect_no_throw([&] { statement; }, #statement " does not throw")

/** @} */

/** @name Status, alternative and concurrency expectations
 *  @brief User-facing wrappers for typed statuses, alternative completions,
 *         future/condition waits and atomic progress.
 *  @details The operation/predicate expression executes only after CHECK.
 *  Named callback expressions are invoked once in the selected branch;
 *  allowed status lists and duration arguments are C++ configuration
 *  arguments, evaluated when calling the checker.
 *  Parenthesize initializer lists containing commas, e.g.
 *  (std::initializer_list<Status>{Status::Sat, Status::Unknown}).
 *  @{ */

/** @def ARIADNE_TESTING_EXPECT_STATUS_CTX
 *  @brief Check a deferred result's permitted typed status and postcondition.
 *  @param operation Result-producing expression evaluated once after CHECK.
 *  @param selector Callable extracting the typed status from the result.
 *  @param permitted std::initializer_list of allowed statuses, parenthesized if needed.
 *  @param verifier Callable checking the result only when status is permitted.
 */
#define ARIADNE_TESTING_EXPECT_STATUS_CTX(context, operation, selector, permitted, verifier) \
    (context).expect_status(                                                                 \
        [&]() -> decltype(auto) { return (operation); },                                     \
        [&](auto const& ariadne_result) -> decltype(auto) {                                  \
            return std::invoke((selector), ariadne_result);                                  \
        },                                                                                   \
        (permitted),                                                                         \
        [&](auto const& ariadne_result) -> decltype(auto) {                                  \
            return std::invoke((verifier), ariadne_result);                                  \
        },                                                                                   \
        #operation " status in " #permitted)

/** @def ARIADNE_TESTING_EXPECT_STATUS_ONLY_CTX
 *  @brief Check a deferred result's typed status without a postcondition.
 */
#define ARIADNE_TESTING_EXPECT_STATUS_ONLY_CTX(context, operation, selector, permitted) \
    (context).expect_status(                                                            \
        [&]() -> decltype(auto) { return (operation); },                                \
        [&](auto const& ariadne_result) -> decltype(auto) {                             \
            return std::invoke((selector), ariadne_result);                             \
        },                                                                              \
        (permitted), #operation " status in " #permitted)

/** @def ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX
 *  @brief Verify either a completed result or a permitted exception alternative.
 *  @details Exactly one validator runs; a throwing validator is Error, not
 *  an accepted exception. Both callbacks return bool or mathematical evidence.
 *  For void operations the result validator receives std::monostate.
 */
#define ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(context, operation, verify_result, verify_exception) \
    (context).expect_alternative(                                                                   \
        [&]() -> decltype(auto) { return (operation); },                                            \
        [&](auto const& ariadne_result) -> decltype(auto) {                                         \
            return std::invoke((verify_result), ariadne_result);                                    \
        },                                                                                          \
        [&](std::exception const& ariadne_error) -> decltype(auto) {                                \
            return std::invoke((verify_exception), ariadne_error);                                  \
        },                                                                                          \
        #operation " result or exception")

/** @def ARIADNE_TESTING_EXPECT_ATOMIC_CTX
 *  @brief Wait for a predicate on atomic state, retaining last observation.
 *  @note The atomic object must remain alive during the bounded wait;
 *  predicate is called by the case thread, never by the producer.
 */
#define ARIADNE_TESTING_EXPECT_ATOMIC_CTX(context, state, predicate, timeout)      \
    (context).expect_atomic((state),                                               \
                            [&](auto const& ariadne_observed) -> decltype(auto) {  \
                                return std::invoke((predicate), ariadne_observed); \
                            },                                                     \
                            (timeout), #state " satisfies " #predicate)

/** @def ARIADNE_TESTING_EXPECT_READY_CTX
 *  @brief Check a future's readiness within a duration (without consuming it).
 */
#define ARIADNE_TESTING_EXPECT_READY_CTX(context, future, timeout) \
    (context).expect_ready((future), (timeout), #future " becomes ready")

/** @def ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX
 *  @brief Check a condition-variable predicate with a caller-owned lock.
 *  @note The condition variable and unique_lock must outlive the check.
 */
#define ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX(context, condition, lock, predicate, timeout)   \
    (context).expect_eventually((condition), (lock),                                          \
                                [&]() -> decltype(auto) { return std::invoke((predicate)); }, \
                                (timeout), #predicate " eventually holds")

/** @} */

/** @name Shared absolute-deadline expectations
 *  @brief Reuse one steady-clock deadline across multiple checks.
 *  @details Pass a std::chrono::steady_clock::time_point, e.g.
 *  `auto deadline = std::chrono::steady_clock::now() + 5s;`.
 *  Only the condition/atomic predicate is evaluated by the check; objects
 *  and deadline arguments are regular C++ function arguments.
 *  @{ */
/** @def ARIADNE_TESTING_EXPECT_READY_UNTIL_CTX
 *  @brief Check future readiness before a shared absolute deadline.
 */
#define ARIADNE_TESTING_EXPECT_READY_UNTIL_CTX(context, future, deadline) \
    (context).expect_ready_until((future), (deadline), #future " ready by " #deadline)

/** @def ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL_CTX
 *  @brief Check a condition variable with a shared deadline.
 *  @note The predicate is deferred until after CHECK.
 */
#define ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL_CTX(context, condition, lock, predicate, deadline)  \
    (context).expect_eventually_until((condition), (lock),                                          \
                                      [&]() -> decltype(auto) { return std::invoke((predicate)); }, \
                                      (deadline), #predicate " holds by " #deadline)

/** @def ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL_CTX
 *  @brief Check atomic progress against a reusable absolute deadline.
 *  @note The predicate executes on the owning case thread after CHECK.
 */
#define ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL_CTX(context, state, predicate, deadline)     \
    (context).expect_atomic_until((state),                                               \
                                  [&](auto const& ariadne_observed) -> decltype(auto) {  \
                                      return std::invoke((predicate), ariadne_observed); \
                                  },                                                     \
                                  (deadline), #state " satisfies " #predicate " by " #deadline)

/** @} */

/** @def ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX
 *  @brief Require all parsed oracle row identities to match a frozen manifest.
 *  @details Both arguments are containers of OracleRow. The reader/parser
 *  must already have produced them and must not silently filter rows.
 *  This is a complete integrity check, not an expected-value generator.
 */
#define ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(context, manifest, observed) \
    (context).check_oracle_rows(                                            \
        [&]() -> decltype(auto) { return (manifest); },                     \
        [&]() -> decltype(auto) { return (observed); },                     \
        #observed " matches " #manifest)

/** @def ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX
 *  @brief Report complete vs explicitly selected frozen-oracle replay.
 *  @details A partial selection remains Inconclusive, not a vacuous pass.
 */
#define ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(context, plan) \
    (context).check_oracle_replay(                              \
        [&]() -> decltype(auto) { return (plan); },             \
        #plan " covers the complete oracle")

/** @name Relational checks
 *  @brief Evaluate left/right once after the live CHECK event.
 *  @{ */
/** @def ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX
 *  @brief Check inequality, handling signed and unsigned integral operands correctly.
 */
#define ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(context, actual, expected)                                             \
    (context).template check_compare_snapshot<::Ariadne::Testing::Comparison::NotEqual>(                            \
        [&](bool capture_passing_values) {                                                                          \
            return ([&](auto const& observed_actual) {                                                              \
                return ::Ariadne::Testing::Detail::observe_compare<                                                 \
                    ::Ariadne::Testing::Comparison::NotEqual>(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                           \
        },                                                                                                          \
        #actual " != " #expected)

/** @def ARIADNE_TESTING_EXPECT_LESS_CTX
 *  @brief Check strict less-than ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_LESS_CTX(context, actual, expected)                                              \
    (context).template check_compare_snapshot<::Ariadne::Testing::Comparison::Less>(                            \
        [&](bool capture_passing_values) {                                                                      \
            return ([&](auto const& observed_actual) {                                                          \
                return ::Ariadne::Testing::Detail::observe_compare<                                             \
                    ::Ariadne::Testing::Comparison::Less>(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                       \
        },                                                                                                      \
        #actual " < " #expected)

/** @def ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX
 *  @brief Check less-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(context, actual, expected)                                             \
    (context).template check_compare_snapshot<::Ariadne::Testing::Comparison::LessEqual>(                            \
        [&](bool capture_passing_values) {                                                                           \
            return ([&](auto const& observed_actual) {                                                               \
                return ::Ariadne::Testing::Detail::observe_compare<                                                  \
                    ::Ariadne::Testing::Comparison::LessEqual>(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                            \
        },                                                                                                           \
        #actual " <= " #expected)

/** @def ARIADNE_TESTING_EXPECT_GREATER_CTX
 *  @brief Check strict greater-than ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_GREATER_CTX(context, actual, expected)                                              \
    (context).template check_compare_snapshot<::Ariadne::Testing::Comparison::Greater>(                            \
        [&](bool capture_passing_values) {                                                                         \
            return ([&](auto const& observed_actual) {                                                             \
                return ::Ariadne::Testing::Detail::observe_compare<                                                \
                    ::Ariadne::Testing::Comparison::Greater>(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                          \
        },                                                                                                         \
        #actual " > " #expected)

/** @def ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX
 *  @brief Check greater-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(context, actual, expected)                                             \
    (context).template check_compare_snapshot<::Ariadne::Testing::Comparison::GreaterEqual>(                            \
        [&](bool capture_passing_values) {                                                                              \
            return ([&](auto const& observed_actual) {                                                                  \
                return ::Ariadne::Testing::Detail::observe_compare<                                                     \
                    ::Ariadne::Testing::Comparison::GreaterEqual>(observed_actual, (expected), capture_passing_values); \
            }((actual)));                                                                                               \
        },                                                                                                              \
        #actual " >= " #expected)

/** @} */

/** @name Predicate checks
 *  @brief ADL-aware unary and binary predicates with evidence-preserving negation.
 *  @{ */
/** @def ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX
 *  @brief Evaluate a named unary predicate; preserve three-valued evidence.
 */
#define ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary(                                                      \
        [](auto const& value) -> decltype(auto) { return predicate(value); },   \
        [&]() -> decltype(auto) { return (operand); },                          \
        #predicate "(" #operand ")")

/** @def ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX
 *  @brief Negate the evidence of a unary predicate without turning unknown into true.
 */
#define ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary(                                                          \
        [](auto const& value) -> decltype(auto) { return predicate(value); },       \
        [&]() -> decltype(auto) { return (operand); },                              \
        "not " #predicate "(" #operand ")", true)

/** @def ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX
 *  @brief Evaluate a named binary predicate with deferred left/right operands.
 */
#define ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(context, predicate, left, right)            \
    (context).check_binary(                                                                     \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); },                                             \
        [&]() -> decltype(auto) { return (right); },                                            \
        #predicate "(" #left ", " #right ")")

/** @def ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX
 *  @brief Negate binary predicate evidence while preserving indeterminate results.
 */
#define ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(context, predicate, left, right)        \
    (context).check_binary(                                                                     \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); },                                             \
        [&]() -> decltype(auto) { return (right); },                                            \
        "not " #predicate "(" #left ", " #right ")", true)

/** @} */

/** @name Three-valued evidence checks
 *  @brief Express definite, possible, impossible or indeterminate expectations.
 *  @{ */
/** @def ARIADNE_TESTING_EXPECT_DEFINITELY_CTX
 *  @brief Require definite proof of a proposition; unknown is Inconclusive.
 */
#define ARIADNE_TESTING_EXPECT_DEFINITELY_CTX(context, expression) \
    (context).expect_evidence(                                     \
        [&]() -> decltype(auto) { return (expression); },          \
        ::Ariadne::Testing::EvidenceExpectation::DefinitelyTrue,   \
        "definitely(" #expression ")")

/** @def ARIADNE_TESTING_EXPECT_POSSIBLY_CTX
 *  @brief Accept true or indeterminate evidence; reject definite falsehood.
 */
#define ARIADNE_TESTING_EXPECT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence(                                   \
        [&]() -> decltype(auto) { return (expression); },        \
        ::Ariadne::Testing::EvidenceExpectation::PossiblyTrue,   \
        "possibly(" #expression ")")

/** @def ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX
 *  @brief Require impossibility; unknown is Inconclusive, not a pass.
 */
#define ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence(                                       \
        [&]() -> decltype(auto) { return (expression); },            \
        ::Ariadne::Testing::EvidenceExpectation::NotPossibly,        \
        "not possibly(" #expression ")")

/** @def ARIADNE_TESTING_EXPECT_INDETERMINATE_CTX
 *  @brief Pass only when mathematical evidence is genuinely indeterminate.
 */
#define ARIADNE_TESTING_EXPECT_INDETERMINATE_CTX(context, expression) \
    (context).expect_evidence(                                        \
        [&]() -> decltype(auto) { return (expression); },             \
        ::Ariadne::Testing::EvidenceExpectation::Indeterminate,       \
        "indeterminate(" #expression ")")

/** @} */

/** @def ARIADNE_TESTING_WORKER_CHECK
 *  @brief Evaluate one worker-local expression and stage its evidence once.
 *  @note Does not touch Context/Reporter. CHECK is emitted by WorkerBatch::flush
 *  after all workers have completed and been joined by the caller.
 */
#define ARIADNE_TESTING_WORKER_CHECK(worker, expression) \
    (worker).check([&]() -> decltype(auto) { return (expression); }, #expression)

/** @name Fixture shorthand macros
 *  @brief Forward to the equivalent explicit-context macro using this->ctx().
 *  @note These require a ContextFixture-derived object.
 *  @{ */
/** @def ARIADNE_TESTING_PROVE
 *  @brief Fixture shorthand: Record mathematical evidence from a deferred proposition, retaining uncertainty.
 */
#define ARIADNE_TESTING_PROVE(expression) \
    ARIADNE_TESTING_PROVE_CTX(this->ctx(), expression)

/** @def ARIADNE_TESTING_REQUIRE
 *  @brief Fixture shorthand: Require a proven prerequisite; a failure or inconclusive result aborts the case.
 */
#define ARIADNE_TESTING_REQUIRE(expression) \
    ARIADNE_TESTING_REQUIRE_CTX(this->ctx(), expression)

/** @def ARIADNE_TESTING_EXPECT_EQUAL
 *  @brief Fixture shorthand: Compare two expressions once using evidence-preserving equality.
 */
#define ARIADNE_TESTING_EXPECT_EQUAL(actual, expected) \
    ARIADNE_TESTING_EXPECT_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_RANGES
 *  @brief Fixture shorthand: Compare two ranges, preserving the first mismatch and unknown element.
 */
#define ARIADNE_TESTING_EXPECT_RANGES(actual, expected) \
    ARIADNE_TESTING_EXPECT_RANGES_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_RANGES_BY
 *  @brief Fixture shorthand: Compare ranges with a caller-provided element comparator.
 */
#define ARIADNE_TESTING_EXPECT_RANGES_BY(actual, expected, comparator) \
    ARIADNE_TESTING_EXPECT_RANGES_BY_CTX(this->ctx(), actual, expected, comparator)

/** @def ARIADNE_TESTING_EXPECT_RANGES_AT
 *  @brief Fixture shorthand: Compare ranges using a comparator and a mismatch-path formatter.
 */
#define ARIADNE_TESTING_EXPECT_RANGES_AT(actual, expected, comparator, path) \
    ARIADNE_TESTING_EXPECT_RANGES_AT_CTX(this->ctx(), actual, expected, comparator, path)

/** @def ARIADNE_TESTING_EXPECT_SAME
 *  @brief Fixture shorthand: Check semantic identity using the available same() relation.
 */
#define ARIADNE_TESTING_EXPECT_SAME(actual, expected) \
    ARIADNE_TESTING_EXPECT_SAME_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_WITHIN
 *  @brief Fixture shorthand: Check that the absolute or domain-specific difference is within a tolerance.
 */
#define ARIADNE_TESTING_EXPECT_WITHIN(actual, expected, tolerance) \
    ARIADNE_TESTING_EXPECT_WITHIN_CTX(this->ctx(), actual, expected, tolerance)

/** @def ARIADNE_TESTING_EXPECT_THROWS
 *  @brief Fixture shorthand: Require the specified exception type from the statement.
 */
#define ARIADNE_TESTING_EXPECT_THROWS(exception_type, statement) \
    ARIADNE_TESTING_EXPECT_THROWS_CTX(this->ctx(), exception_type, statement)

/** @def ARIADNE_TESTING_EXPECT_THROWS_MESSAGE
 *  @brief Fixture shorthand: Require the specified exception type and a message substring.
 */
#define ARIADNE_TESTING_EXPECT_THROWS_MESSAGE(exception_type, statement, substring) \
    ARIADNE_TESTING_EXPECT_THROWS_MESSAGE_CTX(this->ctx(), exception_type, statement, substring)

/** @def ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD
 *  @brief Fixture shorthand: Require an exception type and check a projected payload against an expected value.
 */
#define ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD(exception_type, statement, projector, expected) \
    ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD_CTX(this->ctx(), exception_type, statement, projector, expected)

/** @def ARIADNE_TESTING_EXPECT_EXCEPTION
 *  @brief Fixture shorthand: Verify that the statement throws an exception.
 */
#define ARIADNE_TESTING_EXPECT_EXCEPTION(statement) \
    ARIADNE_TESTING_EXPECT_EXCEPTION_CTX(this->ctx(), statement)

/** @def ARIADNE_TESTING_EXPECT_NO_THROW
 *  @brief Fixture shorthand: Verify that the statement completes without throwing.
 */
#define ARIADNE_TESTING_EXPECT_NO_THROW(statement) \
    ARIADNE_TESTING_EXPECT_NO_THROW_CTX(this->ctx(), statement)

/** @def ARIADNE_TESTING_EXPECT_NOT_EQUAL
 *  @brief Fixture shorthand: Check inequality, handling signed and unsigned integral operands correctly.
 */
#define ARIADNE_TESTING_EXPECT_NOT_EQUAL(actual, expected) \
    ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_LESS
 *  @brief Fixture shorthand: Check strict less-than ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_LESS(actual, expected) \
    ARIADNE_TESTING_EXPECT_LESS_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_LESS_EQUAL
 *  @brief Fixture shorthand: Check less-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_LESS_EQUAL(actual, expected) \
    ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_GREATER
 *  @brief Fixture shorthand: Check strict greater-than ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_GREATER(actual, expected) \
    ARIADNE_TESTING_EXPECT_GREATER_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_GREATER_EQUAL
 *  @brief Fixture shorthand: Check greater-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_TESTING_EXPECT_GREATER_EQUAL(actual, expected) \
    ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_TESTING_EXPECT_UNARY_PREDICATE
 *  @brief Fixture shorthand: Evaluate a named unary predicate; preserve three-valued evidence.
 */
#define ARIADNE_TESTING_EXPECT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

/** @def ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE
 *  @brief Fixture shorthand: Negate the evidence of a unary predicate without turning unknown into true.
 */
#define ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

/** @def ARIADNE_TESTING_EXPECT_BINARY_PREDICATE
 *  @brief Fixture shorthand: Evaluate a named binary predicate with deferred left/right operands.
 */
#define ARIADNE_TESTING_EXPECT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

/** @def ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE
 *  @brief Fixture shorthand: Negate binary predicate evidence while preserving indeterminate results.
 */
#define ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

/** @def ARIADNE_TESTING_EXPECT_DEFINITELY
 *  @brief Fixture shorthand: Require definite proof of a proposition; unknown is Inconclusive.
 */
#define ARIADNE_TESTING_EXPECT_DEFINITELY(expression) \
    ARIADNE_TESTING_EXPECT_DEFINITELY_CTX(this->ctx(), expression)

/** @def ARIADNE_TESTING_EXPECT_POSSIBLY
 *  @brief Fixture shorthand: Accept true or indeterminate evidence; reject definite falsehood.
 */
#define ARIADNE_TESTING_EXPECT_POSSIBLY(expression) \
    ARIADNE_TESTING_EXPECT_POSSIBLY_CTX(this->ctx(), expression)

/** @def ARIADNE_TESTING_EXPECT_NOT_POSSIBLY
 *  @brief Fixture shorthand: Require impossibility; unknown is Inconclusive, not a pass.
 */
#define ARIADNE_TESTING_EXPECT_NOT_POSSIBLY(expression) \
    ARIADNE_TESTING_EXPECT_NOT_POSSIBLY_CTX(this->ctx(), expression)

/** @def ARIADNE_TESTING_EXPECT_INDETERMINATE
 *  @brief Fixture shorthand: Pass only when mathematical evidence is genuinely indeterminate.
 */
#define ARIADNE_TESTING_EXPECT_INDETERMINATE(expression) \
    ARIADNE_TESTING_EXPECT_INDETERMINATE_CTX(this->ctx(), expression)

/** @name Fixture shorthands for domain and concurrency checks
 *  @brief Forward to the corresponding explicit-context macros using ctx().
 *  @note Available within ContextFixture methods; all names have _CTX peers.
 *  @{ */
/** @def ARIADNE_TESTING_EXPECT_STATUS
 *  @brief Fixture shorthand for allowed status with conditional postcondition.
 */
#define ARIADNE_TESTING_EXPECT_STATUS(operation, selector, permitted, verifier) \
    ARIADNE_TESTING_EXPECT_STATUS_CTX(this->ctx(), operation, selector, permitted, verifier)

/** @def ARIADNE_TESTING_EXPECT_STATUS_ONLY
 *  @brief Fixture shorthand for status-only checks.
 */
#define ARIADNE_TESTING_EXPECT_STATUS_ONLY(operation, selector, permitted) \
    ARIADNE_TESTING_EXPECT_STATUS_ONLY_CTX(this->ctx(), operation, selector, permitted)

/** @def ARIADNE_TESTING_EXPECT_ALTERNATIVE
 *  @brief Fixture shorthand for a result or exception alternative.
 */
#define ARIADNE_TESTING_EXPECT_ALTERNATIVE(operation, verify_result, verify_exception) \
    ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(this->ctx(), operation, verify_result, verify_exception)

/** @def ARIADNE_TESTING_EXPECT_ATOMIC
 *  @brief Fixture shorthand for bounded atomic progress.
 */
#define ARIADNE_TESTING_EXPECT_ATOMIC(state, predicate, timeout) \
    ARIADNE_TESTING_EXPECT_ATOMIC_CTX(this->ctx(), state, predicate, timeout)

/** @def ARIADNE_TESTING_EXPECT_READY
 *  @brief Fixture shorthand for future readiness.
 */
#define ARIADNE_TESTING_EXPECT_READY(future, timeout) \
    ARIADNE_TESTING_EXPECT_READY_CTX(this->ctx(), future, timeout)

/** @def ARIADNE_TESTING_EXPECT_EVENTUALLY
 *  @brief Fixture shorthand for condition-variable bounded progress.
 */
#define ARIADNE_TESTING_EXPECT_EVENTUALLY(condition, lock, predicate, timeout) \
    ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX(this->ctx(), condition, lock, predicate, timeout)

/** @} */

/** @def ARIADNE_TESTING_EXPECT_READY_UNTIL
 *  @brief Fixture shorthand for a future and absolute deadline.
 */
#define ARIADNE_TESTING_EXPECT_READY_UNTIL(future, deadline) \
    ARIADNE_TESTING_EXPECT_READY_UNTIL_CTX(this->ctx(), future, deadline)

/** @def ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL
 *  @brief Fixture shorthand for a condition-variable absolute wait.
 */
#define ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL(condition, lock, predicate, deadline) \
    ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL_CTX(this->ctx(), condition, lock, predicate, deadline)

/** @def ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL
 *  @brief Fixture shorthand for atomic progress with an absolute deadline.
 */
#define ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL(state, predicate, deadline) \
    ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL_CTX(this->ctx(), state, predicate, deadline)

/** @} */

/** @def ARIADNE_TESTING_EXPECT_ORACLE_ROWS
 *  @brief Fixture shorthand for full frozen-oracle row integrity.
 */
#define ARIADNE_TESTING_EXPECT_ORACLE_ROWS(manifest, observed) \
    ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(this->ctx(), manifest, observed)

/** @def ARIADNE_TESTING_EXPECT_ORACLE_REPLAY
 *  @brief Fixture shorthand for explicit full/partial replay coverage.
 */
#define ARIADNE_TESTING_EXPECT_ORACLE_REPLAY(plan) \
    ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(this->ctx(), plan)

// Fixture and suite registration helpers.
// Only the name/stringification is a macro; semantics are type-checked C++.
#define ARIADNE_TESTING_ADD_TEST_TO(suite, Fixture, method) \
    ::Ariadne::Testing::add_fixture_test<Fixture, &Fixture::method>((suite), #method)

#define ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(suite, Fixture, method)                  \
    ::Ariadne::Testing::add_fixture_test<Fixture, &Fixture::method>((suite), #method, \
                                                                    ::Ariadne::Testing::FixtureCheck::NoThrow)

// Conventional single-suite main; explicit *_TO supports multiple suites.
#define ARIADNE_TESTING_SUITE(name) ::Ariadne::Testing::Suite ariadne_test_suite(name)

#define ARIADNE_TESTING_ADD_TEST(Fixture, method) \
    ARIADNE_TESTING_ADD_TEST_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_TESTING_ADD_TEST_NO_THROW(Fixture, method) \
    ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_TESTING_RUN_TESTS() ::Ariadne::Testing::run_suite(ariadne_test_suite)
#define ARIADNE_TESTING_RUN_TESTS_WITH_ARGS(argc, argv) \
    ::Ariadne::Testing::run_suite(ariadne_test_suite, argc, argv)

// Parameterized/variant registration helpers.
// Only stringify the method name; member access is checked by the compiler.
// The variant group already knows the fixture type, so it is not repeated.
#define ARIADNE_TESTING_ADD_VARIANT_TEST(group, method) \
    (group).add(#method, [](auto& fixture) { fixture.method(); })

#define ARIADNE_TESTING_ADD_VARIANT_TEST_NO_THROW(group, method) \
    (group).add(#method, [](auto& fixture) { fixture.method(); }, ::Ariadne::Testing::FixtureCheck::NoThrow)

#endif // ARIADNE_TESTING_MACROS_HPP
