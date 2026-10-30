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

#endif // ARIADNE_TESTING_CHECKS_HPP
