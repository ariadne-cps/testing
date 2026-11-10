/***************************************************************************
 *            testing/fixture.hpp
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
#include <memory>
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

/** @brief Register a fixture method selected at compile time.
 *  @details Used by the standard registration macros to avoid GCC
 *  -Wmaybe-uninitialized false positives from runtime member pointers
 *  while retaining the same fixture lifecycle and exception handling.
 *  The original runtime-pointer overload remains available.
 */
template<class Fixture, auto Method>
void add_fixture_test(Suite& suite, std::string_view method_name,
                      FixtureCheck check = FixtureCheck::Direct);

inline int run_suite(Suite const& suite, std::ostream& output = std::cout) {
    ConsoleReporter reporter(output, ConsoleDetail::All);
    return suite.run(reporter).successful() ? 0 : 1;
}

/** @brief Select console verbosity with --full or --concise.
 *  @details Direct invocation without flags remains fully verbose.
 *  The caller (e.g. CTest) may pass --concise without altering results.
 *  Invalid or conflicting flags return 2 before any test executes.
 */
inline int run_suite(Suite const& suite, int argc, char const* const* argv,
                     std::ostream& output = std::cout) {
    ConsoleDetail detail = ConsoleDetail::All;
    bool selected = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view const arg = argv[i] ? std::string_view(argv[i]) : std::string_view();
        if (arg == "--help") {
            output << "Usage: " << (argv[0] ? argv[0] : "test")
                   << " [--full | --concise]" << '\n';
            return 0;
        }
        if (arg != "--full" && arg != "--concise") {
            output << "Unknown test option: " << arg
                   << " (expected --full or --concise)" << '\n';
            return 2;
        }
        auto const requested = arg == "--full" ? ConsoleDetail::All : ConsoleDetail::Concise;
        if (selected && requested != detail) {
            output << "Conflicting test options: --full and --concise" << '\n';
            return 2;
        }
        selected = true;
        detail = requested;
    }
    // A fatal signal bypasses suite_finished(): preserve the latest CHECK
    // only in a crash handler, without printing normal concise progress.
    std::unique_ptr<CrashDiagnostics> crash;
    if (detail == ConsoleDetail::Concise) crash = std::make_unique<CrashDiagnostics>();
    ConsoleReporter reporter(output, detail);
    return suite.run(reporter).successful() ? 0 : 1;
}

} // namespace Ariadne::Testing

#include "testing/fixture.tpl.hpp"


#endif // ARIADNE_TESTING_FIXTURE_HPP
