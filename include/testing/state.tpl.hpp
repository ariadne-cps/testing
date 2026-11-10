/***************************************************************************
 *            testing/state.tpl.hpp
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

/** @file testing/state.tpl.hpp
 *  @brief Template implementation details for state.hpp.
 */
#ifndef ARIADNE_TESTING_STATE_TPL_HPP
#define ARIADNE_TESTING_STATE_TPL_HPP

#include "testing/state.hpp"

namespace Ariadne::Testing {

template<class State, class Restorer>
ScopedState<State, Restorer>::ScopedState(State original, Restorer restorer)
    : _original(std::move(original)), _restorer(std::move(restorer)) {}

template<class State, class Restorer>
ScopedState<State, Restorer>::~ScopedState() noexcept {
    std::invoke(_restorer, std::as_const(_original));
}

template<class Getter, class Restorer>
[[nodiscard]] auto preserve_state(Getter&& getter, Restorer&& restorer) {
    using State = std::remove_cvref_t<std::invoke_result_t<Getter&&>>;
    using Restore = std::decay_t<Restorer>;
    static_assert(!std::is_void_v<State>, "State getter must return a snapshot");
    return ScopedState<State, Restore>(
        std::invoke(std::forward<Getter>(getter)),
        std::forward<Restorer>(restorer));
}

template<class State>
[[nodiscard]] auto preserve_value(State& value) {
    static_assert(std::is_copy_constructible_v<State>,
                  "preserve_value needs a copyable snapshot");
    static_assert(std::is_nothrow_copy_assignable_v<State>,
                  "preserve_value needs noexcept copy assignment; use preserve_state");
    return preserve_state([&value] { return State(value); },
                          [&value](const State& original) noexcept { value = original; });
}

} // namespace Ariadne::Testing

#endif
