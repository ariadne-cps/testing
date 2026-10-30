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

#include <string>
#include <string_view>
#include <type_traits>

namespace Ariadne::Testing {

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
                  "Fixture must be constructible from Ariadne::Testing::Context&");
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

} // namespace Ariadne::Testing

// Only the name/stringification is a macro; semantics are type-checked C++.
#define ARIADNE_ADD_TEST(suite, Fixture, method) \
    ::Ariadne::Testing::add_fixture_test<Fixture>((suite), #method, &Fixture::method)

#define ARIADNE_ADD_TEST_NO_THROW(suite, Fixture, method) \
    ::Ariadne::Testing::add_fixture_test<Fixture>((suite), #method, &Fixture::method, \
        ::Ariadne::Testing::FixtureCheck::NoThrow)

#endif // ARIADNE_TESTING_FIXTURE_HPP
