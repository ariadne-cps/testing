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

#define ARIADNE_PROVE(context, expression) \
    (context).check([&]() -> decltype(auto) { return (expression); }, #expression)

#define ARIADNE_EXPECT_EQUAL(context, actual, expected) \
    (context).check_equal( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " == " #expected)

#define ARIADNE_EXPECT_SAME(context, actual, expected) \
    (context).check_same( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " same " #expected)

#define ARIADNE_EXPECT_WITHIN(context, actual, expected, tolerance) \
    (context).check_within( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        [&]() -> decltype(auto) { return (tolerance); }, \
        #actual " within " #tolerance " of " #expected)

#define ARIADNE_EXPECT_THROWS(context, exception_type, statement) \
    (context).expect_throws<exception_type>([&] { statement; }, \
        #statement " throws " #exception_type)

#define ARIADNE_EXPECT_EXCEPTION(context, statement) \
    (context).expect_exception([&] { statement; }, #statement " throws")

#define ARIADNE_EXPECT_NO_THROW(context, statement) \
    (context).expect_no_throw([&] { statement; }, #statement " does not throw")


// Comparators capture each expression only once, after [CHECK], including
// mixed signed/unsigned integral comparison using std::cmp_*.
#define ARIADNE_EXPECT_NOT_EQUAL(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::NotEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " != " #expected)

#define ARIADNE_EXPECT_LESS(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Less>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " < " #expected)

#define ARIADNE_EXPECT_LESS_EQUAL(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::LessEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " <= " #expected)

#define ARIADNE_EXPECT_GREATER(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::Greater>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " > " #expected)

#define ARIADNE_EXPECT_GREATER_EQUAL(context, actual, expected) \
    (context).check_compare<::Ariadne::Testing::Comparison::GreaterEqual>( \
        [&]() -> decltype(auto) { return (actual); }, \
        [&]() -> decltype(auto) { return (expected); }, \
        #actual " >= " #expected)

// Predicates are resolved at their type-dependent call site, including ADL.
// The not variants negate mathematical evidence, not its bool conversion.
#define ARIADNE_EXPECT_UNARY_PREDICATE(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        #predicate "(" #operand ")")

#define ARIADNE_EXPECT_NOT_UNARY_PREDICATE(context, predicate, operand) \
    (context).check_unary( \
        [](auto const& value) -> decltype(auto) { return predicate(value); }, \
        [&]() -> decltype(auto) { return (operand); }, \
        "not " #predicate "(" #operand ")", true)

#define ARIADNE_EXPECT_BINARY_PREDICATE(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        #predicate "(" #left ", " #right ")")

#define ARIADNE_EXPECT_NOT_BINARY_PREDICATE(context, predicate, left, right) \
    (context).check_binary( \
        [](auto const& lhs, auto const& rhs) -> decltype(auto) { return predicate(lhs, rhs); }, \
        [&]() -> decltype(auto) { return (left); }, \
        [&]() -> decltype(auto) { return (right); }, \
        "not " #predicate "(" #left ", " #right ")", true)

#endif // ARIADNE_TESTING_CHECKS_HPP
