/***************************************************************************
 *            checks.hpp
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

#ifndef ARIADNE_TESTING_CHECKS_HPP
#define ARIADNE_TESTING_CHECKS_HPP

#include "testing/testing.hpp"

// Optional one-evaluation, source-stringifying wrappers.
// For commas inside template/braced expressions, parenthesize the argument.

#define ARIADNE_PROVE_CTX(context, expression) \
    (context).check([&]() -> decltype(auto) { return (expression); }, #expression)

#define ARIADNE_REQUIRE_CTX(context, expression) \
    (context).require([&]() -> decltype(auto) { return (expression); }, #expression)

#define ARIADNE_EXPECT_EQUAL_CTX(context, actual, expected) \
    (context).check_equal( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " == " #expected)

#define ARIADNE_EXPECT_RANGES_CTX(context, actual, expected) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, #actual " ranges equal " #expected)

#define ARIADNE_EXPECT_RANGES_BY_CTX(context, actual, expected, comparator) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, #actual " ranges by " #expected)

#define ARIADNE_EXPECT_RANGES_AT_CTX(context, actual, expected, comparator, path) \
    (context).check_ranges([&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(comparator); }, \
        [&]() { return ::Ariadne::Testing::Detail::capture_callable(path); }, #actual " ranges at " #expected)

#define ARIADNE_EXPECT_SAME_CTX(context, actual, expected) \
    (context).check_same( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " same " #expected)

#define ARIADNE_EXPECT_WITHIN_CTX(context, actual, expected, tolerance) \
    (context).check_within( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() -> decltype(auto) { return (tolerance); }, \
        #actual " within " #tolerance " of " #expected)

#define ARIADNE_EXPECT_THROWS_CTX(context, exception_type, statement) \
    (context).expect_throws<exception_type>([&] { statement; }, \
        #statement " throws " #exception_type)

#define ARIADNE_EXPECT_THROWS_MESSAGE_CTX(context, exception_type, statement, substring) \
    (context).expect_throws_message<exception_type>([&] { statement; }, (substring), \
        #statement " throws " #exception_type " containing " #substring)

#define ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(context, exception_type, statement, projector, expected) \
    (context).expect_throws_payload<exception_type>([&] { statement; }, \
        [&]() -> decltype(auto) { return (projector); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #statement " throws " #exception_type " with " #projector " == " #expected)

#define ARIADNE_EXPECT_EXCEPTION_CTX(context, statement) \
    (context).expect_exception([&] { statement; }, #statement " throws")

#define ARIADNE_EXPECT_NO_THROW_CTX(context, statement) \
    (context).expect_no_throw([&] { statement; }, #statement " does not throw")


// Comparators capture each expression only once, after [CHECK], including
// mixed signed/unsigned integral comparison using std::cmp_*.
#define ARIADNE_EXPECT_NOT_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::NotEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " != " #expected)

#define ARIADNE_EXPECT_LESS_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Less>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " < " #expected)

#define ARIADNE_EXPECT_LESS_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::LessEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " <= " #expected)

#define ARIADNE_EXPECT_GREATER_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Greater>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " > " #expected)

#define ARIADNE_EXPECT_GREATER_EQUAL_CTX(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::GreaterEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " >= " #expected)

// Predicates are resolved at their type-dependent call site, including ADL.
// The not variants negate mathematical evidence, not its bool conversion.
#define ARIADNE_EXPECT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        #predicate "(" #operand ")")

#define ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        "not " #predicate "(" #operand ")", true)

#define ARIADNE_EXPECT_BINARY_PREDICATE_CTX(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        #predicate "(" #left ", " #right ")")

#define ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        "not " #predicate "(" #left ", " #right ")", true)


// Evidence expectations express distinct test intentions. The proof form
// deliberately retains Inconclusive when the result is indeterminate.
#define ARIADNE_EXPECT_DEFINITELY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::DefinitelyTrue, \
        "definitely(" #expression ")")

#define ARIADNE_EXPECT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::PossiblyTrue, \
        "possibly(" #expression ")")

#define ARIADNE_EXPECT_NOT_POSSIBLY_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::NotPossibly, \
        "not possibly(" #expression ")")

#define ARIADNE_EXPECT_INDETERMINATE_CTX(context, expression) \
    (context).expect_evidence( \
        [&]() -> decltype(auto) { return (expression); }, \
        ::Ariadne::Testing::EvidenceExpectation::Indeterminate, \
        "indeterminate(" #expression ")")


// Fixture shorthand; *_CTX remains valid in free functions and lambdas.
#define ARIADNE_PROVE(expression) \
    ARIADNE_PROVE_CTX(this->ctx(), expression)

#define ARIADNE_REQUIRE(expression) \
    ARIADNE_REQUIRE_CTX(this->ctx(), expression)

#define ARIADNE_EXPECT_EQUAL(actual, expected) \
    ARIADNE_EXPECT_EQUAL_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_RANGES(actual, expected) \
    ARIADNE_EXPECT_RANGES_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_RANGES_BY(actual, expected, comparator) \
    ARIADNE_EXPECT_RANGES_BY_CTX(this->ctx(), actual, expected, comparator)

#define ARIADNE_EXPECT_RANGES_AT(actual, expected, comparator, path) \
    ARIADNE_EXPECT_RANGES_AT_CTX(this->ctx(), actual, expected, comparator, path)

#define ARIADNE_EXPECT_SAME(actual, expected) \
    ARIADNE_EXPECT_SAME_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_WITHIN(actual, expected, tolerance) \
    ARIADNE_EXPECT_WITHIN_CTX(this->ctx(), actual, expected, tolerance)

#define ARIADNE_EXPECT_THROWS(exception_type, statement) \
    ARIADNE_EXPECT_THROWS_CTX(this->ctx(), exception_type, statement)

#define ARIADNE_EXPECT_THROWS_MESSAGE(exception_type, statement, substring) \
    ARIADNE_EXPECT_THROWS_MESSAGE_CTX(this->ctx(), exception_type, statement, substring)

#define ARIADNE_EXPECT_THROWS_PAYLOAD(exception_type, statement, projector, expected) \
    ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(this->ctx(), exception_type, statement, projector, expected)

#define ARIADNE_EXPECT_EXCEPTION(statement) \
    ARIADNE_EXPECT_EXCEPTION_CTX(this->ctx(), statement)

#define ARIADNE_EXPECT_NO_THROW(statement) \
    ARIADNE_EXPECT_NO_THROW_CTX(this->ctx(), statement)

#define ARIADNE_EXPECT_NOT_EQUAL(actual, expected) \
    ARIADNE_EXPECT_NOT_EQUAL_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_LESS(actual, expected) \
    ARIADNE_EXPECT_LESS_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_LESS_EQUAL(actual, expected) \
    ARIADNE_EXPECT_LESS_EQUAL_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_GREATER(actual, expected) \
    ARIADNE_EXPECT_GREATER_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_GREATER_EQUAL(actual, expected) \
    ARIADNE_EXPECT_GREATER_EQUAL_CTX(this->ctx(), actual, expected)

#define ARIADNE_EXPECT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_EXPECT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

#define ARIADNE_EXPECT_NOT_UNARY_PREDICATE(predicate, operand) \
    ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(this->ctx(), predicate, operand)

#define ARIADNE_EXPECT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_EXPECT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

#define ARIADNE_EXPECT_NOT_BINARY_PREDICATE(predicate, left, right) \
    ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(this->ctx(), predicate, left, right)

#define ARIADNE_EXPECT_DEFINITELY(expression) \
    ARIADNE_EXPECT_DEFINITELY_CTX(this->ctx(), expression)

#define ARIADNE_EXPECT_POSSIBLY(expression) \
    ARIADNE_EXPECT_POSSIBLY_CTX(this->ctx(), expression)

#define ARIADNE_EXPECT_NOT_POSSIBLY(expression) \
    ARIADNE_EXPECT_NOT_POSSIBLY_CTX(this->ctx(), expression)

#define ARIADNE_EXPECT_INDETERMINATE(expression) \
    ARIADNE_EXPECT_INDETERMINATE_CTX(this->ctx(), expression)

#endif // ARIADNE_TESTING_CHECKS_HPP
