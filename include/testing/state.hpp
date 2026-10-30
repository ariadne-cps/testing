/***************************************************************************
 *            state.hpp
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

#ifndef ARIADNE_TESTING_STATE_HPP
#define ARIADNE_TESTING_STATE_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace Ariadne {

// A scoped snapshot of caller-managed state. Restoration must not throw:
// destructors run during both ordinary exit and exception unwinding.
// This guard deliberately cannot be copied or moved (no double restore).
template<class State, class Restorer>
class [[nodiscard]] ScopedState {
    static_assert(std::is_nothrow_invocable_v<Restorer&, State const&>,
                  "State restoration must be noexcept and accept const State&");
  public:
    ScopedState(State original, Restorer restorer)
        : _original(std::move(original)), _restorer(std::move(restorer)) { }
    ScopedState(ScopedState const&) = delete;
    ScopedState& operator=(ScopedState const&) = delete;
    ScopedState(ScopedState&&) = delete;
    ScopedState& operator=(ScopedState&&) = delete;

    ~ScopedState() noexcept {
        std::invoke(_restorer, std::as_const(_original));
    }

  private:
    State _original;
    [[no_unique_address]] Restorer _restorer;
};

// The getter is called once before the guard is constructed. The restorer
// receives the saved value when scope exits, including during unwinding.
// A throwing getter never creates a guard and therefore never restores.
template<class Getter, class Restorer>
[[nodiscard]] auto preserve_state(Getter&& getter, Restorer&& restorer) {
    using State = std::remove_cvref_t<std::invoke_result_t<Getter&&>>;
    using Restore = std::decay_t<Restorer>;
    static_assert(!std::is_void_v<State>, "State getter must return a snapshot");
    return ScopedState<State, Restore>(
        std::invoke(std::forward<Getter>(getter)),
        std::forward<Restorer>(restorer));
}

// Shorthand for copyable, nothrow-restorable variables (e.g. precision,
// rounding-mode enums and integer settings). More complex objects can use
// preserve_state with an explicitly noexcept restorer.
template<class State>
[[nodiscard]] auto preserve_value(State& value) {
    static_assert(std::is_copy_constructible_v<State>,
                  "preserve_value needs a copyable snapshot");
    static_assert(std::is_nothrow_copy_assignable_v<State>,
                  "preserve_value needs noexcept copy assignment; use preserve_state");
    return preserve_state([&value] { return State(value); },
                          [&value](State const& original) noexcept { value = original; });
}

} // namespace Ariadne

#endif // ARIADNE_TESTING_STATE_HPP
