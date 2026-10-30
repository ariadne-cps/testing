/***************************************************************************
 *            fixture.hpp
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

#ifndef ARIADNE_TESTING_FIXTURE_HPP
#define ARIADNE_TESTING_FIXTURE_HPP

#include "testing/testing.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace Ariadne {

// Each test case constructs its own fixture with a valid Context reference.
class ContextFixture {
  public:
    explicit ContextFixture(Context& context) noexcept : _context(context) { }

  protected:
    Context& ctx() noexcept { return _context; }
    Context const& ctx() const noexcept { return _context; }

  private:
    Context& _context;
};

enum class FixtureCheck { Direct, NoThrow };

template<class Fixture>
void add_fixture_test(Suite& suite, std::string_view method_name,
                      void (Fixture::*method)(),
                      FixtureCheck check = FixtureCheck::Direct) {
    static_assert(std::is_constructible_v<Fixture, Context&>,
                  "Fixture must be constructible from Ariadne::Context&");
    // Display "map_get" for a method named "test_map_get".
    if (method_name.starts_with("test_")) method_name.remove_prefix(5);
    suite.add(std::string(method_name), [method, check](Context& context) {
        Fixture fixture(context);
        if (check == FixtureCheck::NoThrow) {
            context.expect_no_throw([&] { (fixture.*method)(); }, "operation completes");
        } else {
            // Suite::run records uncaught exceptions as an Error.
            (fixture.*method)();
        }
    });
}

inline int run_suite(Suite const& suite, std::ostream& output = std::cout) {
    ConsoleReporter reporter(output);
    return suite.run(reporter).successful() ? 0 : 1;
}

} // namespace Ariadne

// Only the name/stringification is a macro; semantics are type-checked C++.
#define ARIADNE_ADD_TEST_TO(suite, Fixture, method) \
    ::Ariadne::add_fixture_test<Fixture>((suite), #method, &Fixture::method)

#define ARIADNE_ADD_TEST_NO_THROW_TO(suite, Fixture, method) \
    ::Ariadne::add_fixture_test<Fixture>((suite), #method, &Fixture::method, \
        ::Ariadne::FixtureCheck::NoThrow)

// Conventional single-suite main; explicit *_TO supports multiple suites.
#define ARIADNE_TEST_SUITE(name) ::Ariadne::Suite ariadne_test_suite(name)

#define ARIADNE_ADD_TEST(Fixture, method) \
    ARIADNE_ADD_TEST_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_ADD_TEST_NO_THROW(Fixture, method) \
    ARIADNE_ADD_TEST_NO_THROW_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_RUN_TESTS() ::Ariadne::run_suite(ariadne_test_suite)

#endif // ARIADNE_TESTING_FIXTURE_HPP
