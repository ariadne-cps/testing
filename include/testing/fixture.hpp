/***************************************************************************
 *            fixture.hpp
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

/** @file testing/fixture.hpp
 *  @brief Public declarations for this testing module.
 */
#ifndef ARIADNE_TESTING_FIXTURE_HPP
#define ARIADNE_TESTING_FIXTURE_HPP

#include "testing/testing.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace Ariadne::Testing {

// Each test case constructs its own fixture with a valid Context reference.
/** @brief Fixture base exposing its case-local Context to checks. */
class ContextFixture {
  public:
    explicit ContextFixture(Context& context) noexcept : _context(context) { }

  protected:
    Context& ctx() noexcept { return _context; }
    Context const& ctx() const noexcept { return _context; }

  private:
    Context& _context;
};

/** @brief How exceptions from a fixture operation are classified. */
enum class FixtureCheck { Direct, NoThrow };

/** @brief Register a fixture method as a separate case.
 *  @tparam Fixture Type constructible from Context&.
 *  @param check Whether an uncaught exception is a case Error or a failing check.
 *  @details A fresh fixture is constructed per case. Optional void set_up()
 *  and tear_down() hooks surround the method; teardown runs even after a
 *  body exception or failed fatal prerequisite, but not after setup failure.
 *  The fixture destructor always runs. If body and teardown both fail, the
 *  resulting Error retains both descriptions. NoThrow affects the body
 *  only, not the hooks.
 *  @note Never reuse a Context-bound fixture across cases. Share separately
 *  owned warm-start data explicitly through a fixture factory instead.
 */
template<class Fixture>
void add_fixture_test(Suite& suite, std::string_view method_name,
                      void (Fixture::*method)(),
                      FixtureCheck check = FixtureCheck::Direct);

inline int run_suite(Suite const& suite, std::ostream& output = std::cout) {
    ConsoleReporter reporter(output);
    return suite.run(reporter).successful() ? 0 : 1;
}

} // namespace Ariadne::Testing

// Only the name/stringification is a macro; semantics are type-checked C++.
#define ARIADNE_TESTING_ADD_TEST_TO(suite, Fixture, method) \
    ::Ariadne::Testing::add_fixture_test<Fixture>((suite), #method, &Fixture::method)

#define ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(suite, Fixture, method) \
    ::Ariadne::Testing::add_fixture_test<Fixture>((suite), #method, &Fixture::method, \
        ::Ariadne::Testing::FixtureCheck::NoThrow)

// Conventional single-suite main; explicit *_TO supports multiple suites.
#define ARIADNE_TESTING_SUITE(name) ::Ariadne::Testing::Suite ariadne_test_suite(name)

#define ARIADNE_TESTING_ADD_TEST(Fixture, method) \
    ARIADNE_TESTING_ADD_TEST_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_TESTING_ADD_TEST_NO_THROW(Fixture, method) \
    ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(ariadne_test_suite, Fixture, method)

#define ARIADNE_TESTING_RUN_TESTS() ::Ariadne::Testing::run_suite(ariadne_test_suite)

#include "testing/fixture.tpl.hpp"


#endif // ARIADNE_TESTING_FIXTURE_HPP
