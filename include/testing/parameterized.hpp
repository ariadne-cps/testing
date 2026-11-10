/***************************************************************************
 *            testing/parameterized.hpp
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

/** @file testing/parameterized.hpp
 *  @brief Public declarations for this testing module.
 */
#ifndef ARIADNE_TESTING_PARAMETERIZED_HPP
#define ARIADNE_TESTING_PARAMETERIZED_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "testing/fixture.hpp"

namespace Ariadne::Testing {

// A display-name prefix for a variant: implementation, precision, or paradigm.
// There are no nested counters: each (variant, case) is one Suite case.
/** @brief Named collection of cases sharing a display-name prefix. */
class SuiteVariant {
  public:
    SuiteVariant(Suite& suite, std::string name)
        : _suite(&suite), _name(std::move(name)) {
        if (_name.empty())
            throw std::invalid_argument("Suite variant name must be nonempty");
    }

    template<class Callable>
    void add(std::string name, Callable&& body,
             std::vector<std::string> tags = {}) const;

    // One generic callable can be registered for multiple compile-time tags.
    template<class Tag, class Callable>
    void add_typed(std::string name, Callable&& body,
                   std::vector<std::string> tags = {}) const;

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
/** @brief Registers operations against independently constructed fixtures. */
class FixtureVariant {
  public:
    /** @brief Create a named variant whose factory supplies fresh fixtures per case. */
    FixtureVariant(Suite& suite, std::string name, Factory factory);

    template<class Method>
    void add(std::string method_name, Method&& method,
             FixtureCheck check = FixtureCheck::Direct,
             std::vector<std::string> tags = {}) const;

  private:
    SuiteVariant _variant;
    std::shared_ptr<Factory> _factory;
};

/** @brief Register a variant with a per-case fixture factory. */
template<class Fixture, class Factory>
auto fixture_variant(Suite& suite, std::string name, Factory&& factory);

// Default Context-only fixture constructor, matching ARIADNE_ADD_TEST.
/** @brief Register a fixture variant constructed from Context&. */
template<class Fixture>
auto fixture_variant(Suite& suite, std::string name);

} // namespace Ariadne::Testing

#include "testing/parameterized.tpl.hpp"

#endif // ARIADNE_TESTING_PARAMETERIZED_HPP
