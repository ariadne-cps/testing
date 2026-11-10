/***************************************************************************
 *            testing/fixture.tpl.hpp
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

/** @file testing/fixture.tpl.hpp
 *  @brief Template implementation details for fixture.hpp.
 */
#ifndef ARIADNE_TESTING_FIXTURE_TPL_HPP
#define ARIADNE_TESTING_FIXTURE_TPL_HPP

#include "testing/fixture.hpp"

namespace Ariadne::Testing {

namespace Detail {

// Do not silently ignore status-returning setup or teardown hooks.
template<class Fixture>
void fixture_set_up(Fixture& fixture) {
    if constexpr (requires(Fixture& value) { value.set_up(); }) {
        static_assert(std::is_void_v<decltype(fixture.set_up())>,
                      "Fixture set_up() must return void");
        fixture.set_up();
    }
}
template<class Fixture>
void fixture_tear_down(Fixture& fixture) {
    if constexpr (requires(Fixture& value) { value.tear_down(); }) {
        static_assert(std::is_void_v<decltype(fixture.tear_down())>,
                      "Fixture tear_down() must return void");
        fixture.tear_down();
    }
}
inline std::string fixture_failure_description(std::exception_ptr failure) {
    try {
        std::rethrow_exception(failure);
    } catch (const AbortCase&) {
        return "failed prerequisite";
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "non-standard exception";
    }
}
template<class Fixture, class Operation>
void run_fixture_lifecycle(Fixture& fixture, Operation&& operation) {
    fixture_set_up(fixture); // No teardown if setup itself fails.
    std::exception_ptr body_failure;
    try {
        std::invoke(std::forward<Operation>(operation), fixture);
    } catch (...) {
        body_failure = std::current_exception();
    }
    std::exception_ptr teardown_failure;
    try {
        fixture_tear_down(fixture);
    } catch (...) {
        teardown_failure = std::current_exception();
    }
    if (teardown_failure) {
        if (body_failure) {
            throw std::runtime_error(
                "fixture body failed: " + fixture_failure_description(body_failure) +
                "; tear_down failed: " + fixture_failure_description(teardown_failure));
        }
        std::rethrow_exception(teardown_failure);
    }
    if (body_failure)
        std::rethrow_exception(body_failure);
}
} // namespace Detail

template<class Fixture, auto Method>
void add_fixture_test(Suite& suite, std::string_view method_name,
                      FixtureCheck check) {
    static_assert(std::is_constructible_v<Fixture, Context&>,
                  "Fixture must be constructible from Ariadne::Testing::Context&");
    static_assert(std::is_invocable_r_v<void, decltype(Method), Fixture&>,
                  "Method must be a callable fixture member");
    if (method_name.starts_with("test_"))
        method_name.remove_prefix(5);
    suite.add(std::string(method_name), [check](Context& context) {
        Fixture fixture(context);
        Detail::run_fixture_lifecycle(fixture, [&](Fixture& instance) {
            if (check == FixtureCheck::NoThrow) {
                context.expect_no_throw([&] { std::invoke(Method, instance); },
                                        "operation completes");
            } else {
                std::invoke(Method, instance);
            }
        });
    });
}

template<class Fixture>
void add_fixture_test(Suite& suite, std::string_view method_name,
                      void (Fixture::*method)(),
                      FixtureCheck check) {
    static_assert(std::is_constructible_v<Fixture, Context&>,
                  "Fixture must be constructible from Ariadne::Testing::Context&");
    // Display "map_get" for a method named "test_map_get".
    if (method_name.starts_with("test_"))
        method_name.remove_prefix(5);
    suite.add(std::string(method_name), [method, check](Context& context) {
        Fixture fixture(context);
        Detail::run_fixture_lifecycle(fixture, [&](Fixture& instance) {
            if (check == FixtureCheck::NoThrow) {
                context.expect_no_throw([&] { (instance.*method)(); },
                                        "operation completes");
            } else {
                // Suite::run reports uncaught method exceptions as Error.
                (instance.*method)();
            }
        });
    });
}

} // namespace Ariadne::Testing

#endif
