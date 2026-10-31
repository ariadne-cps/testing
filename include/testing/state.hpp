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

/** @file testing/state.hpp
 *  @brief Public declarations for this testing module.
 */
#ifndef ARIADNE_TESTING_STATE_HPP
#define ARIADNE_TESTING_STATE_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace Ariadne::Testing {

// A scoped snapshot of caller-managed state. Restoration must not throw:
// destructors run during both ordinary exit and exception unwinding.
// This guard deliberately cannot be copied or moved (no double restore).
template<class State, class Restorer>
/** @brief Non-copyable and non-movable RAII snapshot guard. */
class [[nodiscard]] ScopedState {
    static_assert(std::is_nothrow_invocable_v<Restorer&, State const&>,
                  "State restoration must be noexcept and accept const State&");
  public:
    ScopedState(State original, Restorer restorer);
    ScopedState(ScopedState const&) = delete;
    ScopedState& operator=(ScopedState const&) = delete;
    ScopedState(ScopedState&&) = delete;
    ScopedState& operator=(ScopedState&&) = delete;
    ~ScopedState() noexcept;

  private:
    State _original;
    [[no_unique_address]] Restorer _restorer;
};

// The getter is called once before the guard is constructed. The restorer
// receives the saved value when scope exits, including during unwinding.
// A throwing getter never creates a guard and therefore never restores.
/** @brief Snapshot state and restore it on scope exit, including unwinding.
 *  @pre The restorer must be noexcept.
 */
template<class Getter, class Restorer>
[[nodiscard]] auto preserve_state(Getter&& getter, Restorer&& restorer);

// Shorthand for copyable, nothrow-restorable variables (e.g. precision,
// rounding-mode enums and integer settings). More complex objects can use
// preserve_state with an explicitly noexcept restorer.
/** @brief Save and restore one copyable, noexcept-assignable value. */
template<class State>
[[nodiscard]] auto preserve_value(State& value);

} // namespace Ariadne::Testing

#include "testing/state.tpl.hpp"

#endif // ARIADNE_TESTING_STATE_HPP
