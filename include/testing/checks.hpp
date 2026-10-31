/***************************************************************************
 *            checks.hpp
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

/** @file testing/checks.hpp
 *  @brief Single-evaluation assertion macros for Context and ContextFixture.
 *  @details Expression operands are evaluated lazily after a live CHECK event;
 *  the *_CTX macros take an explicit Context, while their counterparts use
 *  the current fixture context. Parenthesize arguments containing commas.
 */
#ifndef ARIADNE_TESTING_CHECKS_HPP
#define ARIADNE_TESTING_CHECKS_HPP

#include "testing/testing.hpp"

/** @name Explicit-context assertion macros
 *  @brief Usable in lambdas, free functions and fixture methods.
 *  @details Operands are deferred and stringified for diagnostics. Parenthesize
 *  expressions containing top-level commas (e.g. braced values).
 *  @{ */

/** @def ARIADNE_PROVE_CTX
 *  @brief Record mathematical evidence from a deferred proposition, retaining uncertainty.
 */
#define ARIADNE_PROVE_CTX(context, expression) \
    (context).check([&]() -> decltype(auto) { return (expression); }, #expression)

/** @def ARIADNE_REQUIRE_CTX
 *  @brief Require a proven prerequisite; a failure or inconclusive result aborts the case.
 */
#define ARIADNE_REQUIRE_CTX(context, expression) \
    (context).require([&]() -> decltype(auto) { return (expression); }, #expression)

/** @def ARIADNE_EXPECT_EQUAL_CTX
 *  @brief Compare two expressions once using evidence-preserving equality.
 */
#define ARIADNE_EXPECT_EQUAL_CTX(context, actual, expected) \
    (context).check_equal( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " == " #expected)

/** @def ARIADNE_EXPECT_RANGES_CTX
 *  @brief Compare two ranges, preserving the first mismatch and unknown element.
 */
#define ARIADNE_EXPECT_RANGES_CTX(context, actual, expected) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, #actual " ranges equal " #expected)

/** @def ARIADNE_EXPECT_RANGES_BY_CTX
 *  @brief Compare ranges with a caller-provided element comparator.
 */
#define ARIADNE_EXPECT_RANGES_BY_CTX(context, actual, expected, comparator) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, #actual " ranges by " #expected)

/** @def ARIADNE_EXPECT_RANGES_AT_CTX
 *  @brief Compare ranges using a comparator and a mismatch-path formatter.
 */
#define ARIADNE_EXPECT_RANGES_AT_CTX(context, actual, expected, comparator, path) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(path); }, #actual " ranges at " #expected)

/** @def ARIADNE_EXPECT_SAME_CTX
 *  @brief Check semantic identity using the available same() relation.
 */
#define ARIADNE_EXPECT_SAME_CTX(context, actual, expected) \
    (context).check_same( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " same " #expected)

/** @def ARIADNE_EXPECT_WITHIN_CTX
 *  @brief Check that the absolute or domain-specific difference is within a tolerance.
 */
#define ARIADNE_EXPECT_WITHIN_CTX(context, actual, expected, tolerance) \
    (context).check_within( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() -> decltype(auto) { return (tolerance); }, \
        #actual " within " #tolerance " of " #expected)

/** @def ARIADNE_EXPECT_THROWS_CTX
 *  @brief Require the specified exception type from the statement.
 */
#define ARIADNE_EXPECT_THROWS_CTX(context, exception_type, statement) \
    (context).expect_throws<exception_type>([&] { statement; }, \
        #statement " throws " #exception_type)

/** @def ARIADNE_EXPECT_THROWS_MESSAGE_CTX
 *  @brief Require the specified exception type and a message substring.
 */
#define ARIADNE_EXPECT_THROWS_MESSAGE_CTX(context, exception_type, statement, substring) \
    (context).expect_throws_message<exception_type>([&] { statement; }, (substring), \
        #statement " throws " #exception_type " containing " #substring)

/** @def ARIADNE_EXPECT_THROWS_PAYLOAD_CTX
 *  @brief Require an exception type and check a projected payload against an expected value.
 */
#define ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(context, exception_type, statement, projector, expected) \
    (context).expect_throws_payload<exception_type>([&] { statement; }, \
        [&]() -> decltype(auto) { return (projector); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #statement " throws " #exception_type " with " #projector " == " #expected)

/** @def ARIADNE_EXPECT_EXCEPTION_CTX
 *  @brief Verify that the statement throws an exception.
 */
#define ARIADNE_EXPECT_EXCEPTION_CTX(context, statement) \
    (context).expect_exception([&] { statement; }, #statement " throws")

/** @def ARIADNE_EXPECT_NO_THROW_CTX
 *  @brief Verify that the statement completes without throwing.
 */
#define ARIADNE_EXPECT_NO_THROW_CTX(context, statement) \
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
 
/** @def ARIADNE_EXPECT_STATUS_CTX
 *  @brief Check a deferred result's permitted typed status and postcondition.
 *  @param operation Result-producing expression evaluated once after CHECK.
 *  @param selector Callable extracting the typed status from the result.
 *  @param permitted std::initializer_list of allowed statuses, parenthesized if needed.
 *  @param verifier Callable checking the result only when status is permitted.
 */
#define ARIADNE_EXPECT_STATUS_CTX(context, operation, selector, permitted, verifier) \
    (context).expect_status( \
        [&]() -> decltype(auto) { return (operation); }, \
        [&](auto const& ariadne_result) -> decltype(auto) { \
            return std::invoke((selector), ariadne_result); \
        }, (permitted), \
        [&](auto const& ariadne_result) -> decltype(auto) { \
            return std::invoke((verifier), ariadne_result); \
        }, #operation " status in " #permitted)

/** @def ARIADNE_EXPECT_STATUS_ONLY_CTX
 *  @brief Check a deferred result's typed status without a postcondition.
 */
#define ARIADNE_EXPECT_STATUS_ONLY_CTX(context, operation, selector, permitted) \
    (context).expect_status( \
        [&]() -> decltype(auto) { return (operation); }, \
        [&](auto const& ariadne_result) -> decltype(auto) { \
            return std::invoke((selector), ariadne_result); \
        }, (permitted), #operation " status in " #permitted)

/** @def ARIADNE_EXPECT_ALTERNATIVE_CTX
 *  @brief Verify either a completed result or a permitted exception alternative.
 *  @details Exactly one validator runs; a throwing validator is Error, not
 *  an accepted exception. Both callbacks return bool or mathematical evidence.
 *  For void operations the result validator receives std::monostate.
 */
#define ARIADNE_EXPECT_ALTERNATIVE_CTX(context, operation, verify_result, verify_exception) \
    (context).expect_alternative( \
        [&]() -> decltype(auto) { return (operation); }, \
        [&](auto const& ariadne_result) -> decltype(auto) { \
            return std::invoke((verify_result), ariadne_result); \
        }, \
        [&](std::exception const& ariadne_error) -> decltype(auto) { \
            return std::invoke((verify_exception), ariadne_error); \
        }, #operation " result or exception")

/** @def ARIADNE_EXPECT_ATOMIC_CTX
 *  @brief Wait for a predicate on atomic state, retaining last observation.
 *  @note The atomic object must remain alive during the bounded wait;
 *  predicate is called by the case thread, never by the producer.
 */
#define ARIADNE_EXPECT_ATOMIC_CTX(context, state, predicate, timeout) \
    (context).expect_atomic((state), \
        [&](auto const& ariadne_observed) -> decltype(auto) { \
            return std::invoke((predicate), ariadne_observed); \
        }, (timeout), #state " satisfies " #predicate)

/** @def ARIADNE_EXPECT_READY_CTX
 *  @brief Check a future's readiness within a duration (without consuming it).
 */
#define ARIADNE_EXPECT_READY_CTX(context, future, timeout) \
    (context).expect_ready((future), (timeout), #future " becomes ready")

/** @def ARIADNE_EXPECT_EVENTUALLY_CTX
 *  @brief Check a condition-variable predicate with a caller-owned lock.
 *  @note The condition variable and unique_lock must outlive the check.
 */
#define ARIADNE_EXPECT_EVENTUALLY_CTX(context, condition, lock, predicate, timeout) \
    (context).expect_eventually((condition), (lock), \
        [&]() -> decltype(auto) { return std::invoke((predicate)); }, \
        (timeout), #predicate " eventually holds")

/** @} */

/** @name Relational checks
 *  @brief Evaluate left/right once after the live CHECK event.
 *  @{ */
/** @def ARIADNE_EXPECT_NOT_EQUAL_CTX
 *  @brief Check inequality, handling signed and unsigned integral operands correctly.
 */
#define ARIADNE_EXPECT_NOT_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::NotEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " != " #expected)

/** @def ARIADNE_EXPECT_LESS_CTX
 *  @brief Check strict less-than ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_LESS_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Less>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " < " #expected)

/** @def ARIADNE_EXPECT_LESS_EQUAL_CTX
 *  @brief Check less-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_LESS_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::LessEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " <= " #expected)

/** @def ARIADNE_EXPECT_GREATER_CTX
 *  @brief Check strict greater-than ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_GREATER_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Greater>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " > " #expected)

/** @def ARIADNE_EXPECT_GREATER_EQUAL_CTX
 *  @brief Check greater-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_GREATER_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::GreaterEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " >= " #expected)

/** @} */

/** @name Predicate checks
 *  @brief ADL-aware unary and binary predicates with evidence-preserving negation.
 *  @{ */
/** @def ARIADNE_EXPECT_UNARY_PREDICATE_CTX
 *  @brief Evaluate a named unary predicate; preserve three-valued evidence.
 */
#define ARIADNE_EXPECT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        #predicate "(" #operand ")")

/** @def ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX
 *  @brief Negate the evidence of a unary predicate without turning unknown into true.
 */
#define ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        "not " #predicate "(" #operand ")", true)

/** @def ARIADNE_EXPECT_BINARY_PREDICATE_CTX
 *  @brief Evaluate a named binary predicate with deferred left/right operands.
 */
#define ARIADNE_EXPECT_BINARY_PREDICATE_CTX(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        #predicate "(" #left ", " #right ")")

/** @def ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX
 *  @brief Negate binary predicate evidence while preserving indeterminate results.
 */
#define ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        "not " #predicate "(" #left ", " #right ")", true)


/** @} */

/** @name Three-valued evidence checks
 *  @brief Express definite, possible, impossible or indeterminate expectations.
 *  @{ */
/** @def ARIADNE_EXPECT_DEFINITELY_CTX
 *  @brief Require definite proof of a proposition; unknown is Inconclusive.
 */
#define ARIADNE_EXPECT_DEFINITELY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::DefinitelyTrue, \
        "definitely(" #expression ")")

/** @def ARIADNE_EXPECT_POSSIBLY_CTX
 *  @brief Accept true or indeterminate evidence; reject definite falsehood.
 */
#define ARIADNE_EXPECT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::PossiblyTrue, \
        "possibly(" #expression ")")

/** @def ARIADNE_EXPECT_NOT_POSSIBLY_CTX
 *  @brief Require impossibility; unknown is Inconclusive, not a pass.
 */
#define ARIADNE_EXPECT_NOT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::NotPossibly, \
        "not possibly(" #expression ")")

/** @def ARIADNE_EXPECT_INDETERMINATE_CTX
 *  @brief Pass only when mathematical evidence is genuinely indeterminate.
 */
#define ARIADNE_EXPECT_INDETERMINATE_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::Indeterminate, \
        "indeterminate(" #expression ")")


/** @} */

/** @name Fixture shorthand macros
 *  @brief Forward to the equivalent explicit-context macro using this->ctx().
 *  @note These require a ContextFixture-derived object.
 *  @{ */
/** @def ARIADNE_PROVE
 *  @brief Fixture shorthand: Record mathematical evidence from a deferred proposition, retaining uncertainty.
 */
#define ARIADNE_PROVE(expression) \
    ARIADNE_PROVE_CTX(this->ctx(), expression)

/** @def ARIADNE_REQUIRE
 *  @brief Fixture shorthand: Require a proven prerequisite; a failure or inconclusive result aborts the case.
 */
#define ARIADNE_REQUIRE(expression) \
    ARIADNE_REQUIRE_CTX(this->ctx(), expression)

/** @def ARIADNE_EXPECT_EQUAL
 *  @brief Fixture shorthand: Compare two expressions once using evidence-preserving equality.
 */
#define ARIADNE_EXPECT_EQUAL(actual, expected) \
    ARIADNE_EXPECT_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_RANGES
 *  @brief Fixture shorthand: Compare two ranges, preserving the first mismatch and unknown element.
 */
#define ARIADNE_EXPECT_RANGES(actual, expected) \
    ARIADNE_EXPECT_RANGES_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_RANGES_BY
 *  @brief Fixture shorthand: Compare ranges with a caller-provided element comparator.
 */
#define ARIADNE_EXPECT_RANGES_BY(actual, expected, comparator) \
    ARIADNE_EXPECT_RANGES_BY_CTX(this->ctx(), actual, expected, comparator)

/** @def ARIADNE_EXPECT_RANGES_AT
 *  @brief Fixture shorthand: Compare ranges using a comparator and a mismatch-path formatter.
 */
#define ARIADNE_EXPECT_RANGES_AT(actual, expected, comparator, path) \
    ARIADNE_EXPECT_RANGES_AT_CTX(this->ctx(), actual, expected, comparator, path)

/** @def ARIADNE_EXPECT_SAME
 *  @brief Fixture shorthand: Check semantic identity using the available same() relation.
 */
#define ARIADNE_EXPECT_SAME(actual, expected) \
    ARIADNE_EXPECT_SAME_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_WITHIN
 *  @brief Fixture shorthand: Check that the absolute or domain-specific difference is within a tolerance.
 */
#define ARIADNE_EXPECT_WITHIN(actual, expected, tolerance) \
    ARIADNE_EXPECT_WITHIN_CTX(this->ctx(), actual, expected, tolerance)

/** @def ARIADNE_EXPECT_THROWS
 *  @brief Fixture shorthand: Require the specified exception type from the statement.
 */
#define ARIADNE_EXPECT_THROWS(exception_type, statement) \
    ARIADNE_EXPECT_THROWS_CTX(this->ctx(), exception_type, statement)

/** @def ARIADNE_EXPECT_THROWS_MESSAGE
 *  @brief Fixture shorthand: Require the specified exception type and a message substring.
 */
#define ARIADNE_EXPECT_THROWS_MESSAGE(exception_type, statement, substring) \
    ARIADNE_EXPECT_THROWS_MESSAGE_CTX(this->ctx(), exception_type, statement, substring)

/** @def ARIADNE_EXPECT_THROWS_PAYLOAD
 *  @brief Fixture shorthand: Require an exception type and check a projected payload against an expected value.
 */
#define ARIADNE_EXPECT_THROWS_PAYLOAD(exception_type, statement, projector, expected) \
    ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(this->ctx(), exception_type, statement, projector, expected)

/** @def ARIADNE_EXPECT_EXCEPTION
 *  @brief Fixture shorthand: Verify that the statement throws an exception.
 */
#define ARIADNE_EXPECT_EXCEPTION(statement) \
    ARIADNE_EXPECT_EXCEPTION_CTX(this->ctx(), statement)

/** @def ARIADNE_EXPECT_NO_THROW
 *  @brief Fixture shorthand: Verify that the statement completes without throwing.
 */
#define ARIADNE_EXPECT_NO_THROW(statement) \
    ARIADNE_EXPECT_NO_THROW_CTX(this->ctx(), statement)

/** @def ARIADNE_EXPECT_NOT_EQUAL
 *  @brief Fixture shorthand: Check inequality, handling signed and unsigned integral operands correctly.
 */
#define ARIADNE_EXPECT_NOT_EQUAL(actual, expected) \
    ARIADNE_EXPECT_NOT_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_LESS
 *  @brief Fixture shorthand: Check strict less-than ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_LESS(actual, expected) \
    ARIADNE_EXPECT_LESS_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_LESS_EQUAL
 *  @brief Fixture shorthand: Check less-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_LESS_EQUAL(actual, expected) \
    ARIADNE_EXPECT_LESS_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_GREATER
 *  @brief Fixture shorthand: Check strict greater-than ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_GREATER(actual, expected) \
    ARIADNE_EXPECT_GREATER_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_GREATER_EQUAL
 *  @brief Fixture shorthand: Check greater-than-or-equal ordering of two deferred operands.
 */
#define ARIADNE_EXPECT_GREATER_EQUAL(actual, expected) \
    ARIADNE_EXPECT_GREATER_EQUAL_CTX(this->ctx(), actual, expected)

/** @def ARIADNE_EXPECT_UNARY_PREDICATE
 *  @brief Fixture shorthand: Evaluate a named unary predicate; preserve three-valued evidence.
 */
#define ARIADNE_EXPECT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_EXPECT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

/** @def ARIADNE_EXPECT_NOT_UNARY_PREDICATE
 *  @brief Fixture shorthand: Negate the evidence of a unary predicate without turning unknown into true.
 */
#define ARIADNE_EXPECT_NOT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

/** @def ARIADNE_EXPECT_BINARY_PREDICATE
 *  @brief Fixture shorthand: Evaluate a named binary predicate with deferred left/right operands.
 */
#define ARIADNE_EXPECT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_EXPECT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

/** @def ARIADNE_EXPECT_NOT_BINARY_PREDICATE
 *  @brief Fixture shorthand: Negate binary predicate evidence while preserving indeterminate results.
 */
#define ARIADNE_EXPECT_NOT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

/** @def ARIADNE_EXPECT_DEFINITELY
 *  @brief Fixture shorthand: Require definite proof of a proposition; unknown is Inconclusive.
 */
#define ARIADNE_EXPECT_DEFINITELY(expression) \
    ARIADNE_EXPECT_DEFINITELY_CTX(this->ctx(), expression)

/** @def ARIADNE_EXPECT_POSSIBLY
 *  @brief Fixture shorthand: Accept true or indeterminate evidence; reject definite falsehood.
 */
#define ARIADNE_EXPECT_POSSIBLY(expression) \
    ARIADNE_EXPECT_POSSIBLY_CTX(this->ctx(), expression)

/** @def ARIADNE_EXPECT_NOT_POSSIBLY
 *  @brief Fixture shorthand: Require impossibility; unknown is Inconclusive, not a pass.
 */
#define ARIADNE_EXPECT_NOT_POSSIBLY(expression) \
    ARIADNE_EXPECT_NOT_POSSIBLY_CTX(this->ctx(), expression)

/** @def ARIADNE_EXPECT_INDETERMINATE
 *  @brief Fixture shorthand: Pass only when mathematical evidence is genuinely indeterminate.
 */
#define ARIADNE_EXPECT_INDETERMINATE(expression) \
    ARIADNE_EXPECT_INDETERMINATE_CTX(this->ctx(), expression)

/** @name Fixture shorthands for domain and concurrency checks
 *  @brief Forward to the corresponding explicit-context macros using ctx().
 *  @note Available within ContextFixture methods; all names have _CTX peers.
 *  @{ */
/** @def ARIADNE_EXPECT_STATUS
 *  @brief Fixture shorthand for allowed status with conditional postcondition.
 */
#define ARIADNE_EXPECT_STATUS(operation, selector, permitted, verifier) \
    ARIADNE_EXPECT_STATUS_CTX(this->ctx(), operation, selector, permitted, verifier)

/** @def ARIADNE_EXPECT_STATUS_ONLY
 *  @brief Fixture shorthand for status-only checks.
 */
#define ARIADNE_EXPECT_STATUS_ONLY(operation, selector, permitted) \
    ARIADNE_EXPECT_STATUS_ONLY_CTX(this->ctx(), operation, selector, permitted)

/** @def ARIADNE_EXPECT_ALTERNATIVE
 *  @brief Fixture shorthand for a result or exception alternative.
 */
#define ARIADNE_EXPECT_ALTERNATIVE(operation, verify_result, verify_exception) \
    ARIADNE_EXPECT_ALTERNATIVE_CTX(this->ctx(), operation, verify_result, verify_exception)

/** @def ARIADNE_EXPECT_ATOMIC
 *  @brief Fixture shorthand for bounded atomic progress.
 */
#define ARIADNE_EXPECT_ATOMIC(state, predicate, timeout) \
    ARIADNE_EXPECT_ATOMIC_CTX(this->ctx(), state, predicate, timeout)

/** @def ARIADNE_EXPECT_READY
 *  @brief Fixture shorthand for future readiness.
 */
#define ARIADNE_EXPECT_READY(future, timeout) \
    ARIADNE_EXPECT_READY_CTX(this->ctx(), future, timeout)

/** @def ARIADNE_EXPECT_EVENTUALLY
 *  @brief Fixture shorthand for condition-variable bounded progress.
 */
#define ARIADNE_EXPECT_EVENTUALLY(condition, lock, predicate, timeout) \
    ARIADNE_EXPECT_EVENTUALLY_CTX(this->ctx(), condition, lock, predicate, timeout)

/** @} */

/** @} */

#endif // ARIADNE_TESTING_CHECKS_HPP
