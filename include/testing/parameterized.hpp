/***************************************************************************
 *            parameterized.hpp
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

#ifndef ARIADNE_TESTING_PARAMETERIZED_HPP
#define ARIADNE_TESTING_PARAMETERIZED_HPP

#include "testing/fixture.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Ariadne::Testing {

// A display-name prefix for a variant: implementation, precision, or paradigm.
// There are no nested counters: each (variant, case) is one Suite case.
class SuiteVariant {
  public:
    SuiteVariant(Suite& suite, std::string name)
        : _suite(&suite), _name(std::move(name)) {
        if (_name.empty()) throw std::invalid_argument("Suite variant name must be nonempty");
    }

    template<class Callable>
    void add(std::string name, Callable&& body) const {
        if (name.empty()) throw std::invalid_argument("Variant case name must be nonempty");
        static_assert(std::is_invocable_r_v<void, std::decay_t<Callable>&, Context&>,
                      "Variant case must be callable with Context&");
        auto action = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(body));
        _suite->add(_name + " / " + name, [action](Context& context) {
            std::invoke(*action, context);
        });
    }

    // One generic callable can be registered for multiple compile-time tags.
    template<class Tag, class Callable>
    void add_typed(std::string name, Callable&& body) const {
        static_assert(std::is_invocable_r_v<void, std::decay_t<Callable>&,
                                            Context&, std::type_identity<Tag>>,
                      "Typed case must accept Context& and std::type_identity<Tag>");
        auto action = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(body));
        add(std::move(name), [action](Context& context) {
            std::invoke(*action, context, std::type_identity<Tag>{});
        });
    }

  private:
    Suite* _suite;
    std::string _name;
};

inline SuiteVariant suite_variant(Suite& suite, std::string name) {
    return SuiteVariant(suite, std::move(name));
}

// A factory returns a fresh Fixture by value for *every case execution*.
// It can construct the fixture from Context&, plus arbitrary runtime
// parameters or an owned/cloneable implementation of an interface.
template<class Fixture, class Factory>
class FixtureVariant {
  public:
    FixtureVariant(Suite& suite, std::string name, Factory factory)
        : _variant(suite, std::move(name)),
          _factory(std::make_shared<Factory>(std::move(factory))) {
        static_assert(std::is_invocable_v<Factory&, Context&>,
                      "Factory must accept Context&");
        static_assert(std::is_same_v<std::invoke_result_t<Factory&, Context&>, Fixture>,
                      "Factory must return Fixture by value");
    }

    template<class Method>
    void add(std::string method_name, Method&& method,
             FixtureCheck check = FixtureCheck::Direct) const {
        static_assert(std::is_invocable_v<std::decay_t<Method>&, Fixture&>,
                      "Test method must accept Fixture&");
        static_assert(std::is_void_v<std::invoke_result_t<std::decay_t<Method>&, Fixture&>>,
                      "Test method must return void; do not discard a status");
        if (method_name.starts_with("test_")) method_name.erase(0, 5);
        auto factory = _factory;
        auto operation = std::make_shared<std::decay_t<Method>>(std::forward<Method>(method));
        _variant.add(std::move(method_name), [factory, operation, check](Context& context) {
            // Any failure in the factory is reported as Error by Suite::run.
            // No factory is invoked while cases are being registered.
            Fixture fixture = std::invoke(*factory, context);
            if (check == FixtureCheck::NoThrow) {
                context.expect_no_throw([&] { std::invoke(*operation, fixture); },
                                        "operation completes");
            } else {
                std::invoke(*operation, fixture);
            }
        });
    }

  private:
    SuiteVariant _variant;
    std::shared_ptr<Factory> _factory;
};

template<class Fixture, class Factory>
auto fixture_variant(Suite& suite, std::string name, Factory&& factory) {
    using FactoryType = std::decay_t<Factory>;
    return FixtureVariant<Fixture, FactoryType>(suite, std::move(name),
                                                std::forward<Factory>(factory));
}

// Default Context-only fixture constructor, matching ARIADNE_ADD_TEST.
template<class Fixture>
auto fixture_variant(Suite& suite, std::string name) {
    static_assert(std::is_constructible_v<Fixture, Context&>,
                  "Fixture must be constructible from Context&");
    return fixture_variant<Fixture>(suite, std::move(name),
                                     [](Context& context) { return Fixture(context); });
}

} // namespace Ariadne::Testing

// Only stringify the method name; member access is checked by the compiler.
// The variant group already knows the fixture type, so it is not repeated.
#define ARIADNE_ADD_VARIANT_TEST(group, method) \
    (group).add(#method, [](auto& fixture) { fixture.method(); })

#define ARIADNE_ADD_VARIANT_TEST_NO_THROW(group, method) \
    (group).add(#method, [](auto& fixture) { fixture.method(); }, \
                ::Ariadne::Testing::FixtureCheck::NoThrow)

#endif // ARIADNE_TESTING_PARAMETERIZED_HPP
