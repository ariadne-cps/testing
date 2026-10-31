/***************************************************************************
 *            parameterized.tpl.hpp
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

/** @file testing/parameterized.tpl.hpp
 *  @brief Template implementation details for parameterized.hpp.
 */
#ifndef ARIADNE_TESTING_PARAMETERIZED_TPL_HPP
#define ARIADNE_TESTING_PARAMETERIZED_TPL_HPP

#include "testing/parameterized.hpp"

namespace Ariadne::Testing {

    template<class Callable>
    void SuiteVariant::add(std::string name, Callable&& body,
                           std::vector<std::string> tags) const {
        if (name.empty()) throw std::invalid_argument("Variant case name must be nonempty");
        static_assert(std::is_invocable_r_v<void, std::decay_t<Callable>&, Context&>,
                      "Variant case must be callable with Context&");
        auto action = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(body));
        _suite->add(_name + " / " + name, [action](Context& context) {
            std::invoke(*action, context);
        }, CaseMetadata{std::move(tags), _name});
    }

    template<class Tag, class Callable>
    void SuiteVariant::add_typed(std::string name, Callable&& body,
                                 std::vector<std::string> tags) const {
        static_assert(std::is_invocable_r_v<void, std::decay_t<Callable>&,
                                            Context&, std::type_identity<Tag>>,
                      "Typed case must accept Context& and std::type_identity<Tag>");
        auto action = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(body));
        add(std::move(name), [action](Context& context) {
            std::invoke(*action, context, std::type_identity<Tag>{});
        }, std::move(tags));
    }

template<class Fixture, class Factory>
FixtureVariant<Fixture, Factory>::FixtureVariant(Suite& suite, std::string name, Factory factory)
        : _variant(suite, std::move(name)),
          _factory(std::make_shared<Factory>(std::move(factory))) {
        static_assert(std::is_invocable_v<Factory&, Context&>,
                      "Factory must accept Context&");
        static_assert(std::is_same_v<std::invoke_result_t<Factory&, Context&>, Fixture>,
                      "Factory must return Fixture by value");
    }

template<class Fixture, class Factory>
template<class Method>
    void FixtureVariant<Fixture, Factory>::add(std::string method_name, Method&& method,
             FixtureCheck check, std::vector<std::string> tags) const {
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
            Detail::run_fixture_lifecycle(fixture, [&](Fixture& instance) {
                if (check == FixtureCheck::NoThrow) {
                    context.expect_no_throw([&] { std::invoke(*operation, instance); },
                                            "operation completes");
                } else {
                    std::invoke(*operation, instance);
                }
            });
        }, std::move(tags));
    }

template<class Fixture, class Factory>
auto fixture_variant(Suite& suite, std::string name, Factory&& factory) {
    using FactoryType = std::decay_t<Factory>;
    return FixtureVariant<Fixture, FactoryType>(suite, std::move(name),
                                                std::forward<Factory>(factory));
}

template<class Fixture>
auto fixture_variant(Suite& suite, std::string name) {
    static_assert(std::is_constructible_v<Fixture, Context&>,
                  "Fixture must be constructible from Context&");
    return fixture_variant<Fixture>(suite, std::move(name),
                                     [](Context& context) { return Fixture(context); });
}

} // namespace Ariadne::Testing

#endif
