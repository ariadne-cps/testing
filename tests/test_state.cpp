/***************************************************************************
 *            test_state.cpp
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

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

struct Capture : Reporter {
    std::vector<CaseResult> results;
    void case_finished(const CaseResult& result) override { results.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

int main() {
    int global_setting = 11;
    int saves = 0, restores = 0;
    Suite suite("state isolation");
    suite.add("value restored", [&](Context& ctx) {
        {
            auto guard = preserve_value(global_setting);
            global_setting = 37;
            ctx.expect_equal(global_setting, 37, "modified in scope");
        }
        ctx.expect_equal(global_setting, 11, "restored on normal exit");
    });
    suite.add("nested restoration", [&](Context& ctx) {
        auto outer = preserve_value(global_setting);
        global_setting = 22;
        {
            auto inner = preserve_value(global_setting);
            global_setting = 33;
        }
        ctx.expect_equal(global_setting, 22, "inner snapshot restored");
    });
    suite.add("after nested case", [&](Context& ctx) {
        ctx.expect_equal(global_setting, 11, "no leakage between cases");
    });
    suite.add("unwinding restores", [&](Context& ctx) {
        try {
            auto guard = preserve_value(global_setting);
            global_setting = 44;
            throw std::runtime_error("intentional");
        } catch (const std::runtime_error&) {
            ctx.expect_equal(global_setting, 11, "restored during unwinding");
        }
    });
    suite.add("custom getter/restorer", [&](Context& ctx) {
        auto guard = preserve_state([&] { ++saves; return global_setting; },
                                    [&](const int& original) noexcept { ++restores; global_setting=original; });
        global_setting = 55;
        ctx.expect_equal(global_setting, 55, "custom state changed");
    });
    suite.add("after custom case", [&](Context& ctx) {
        ctx.expect_equal(global_setting, 11, "custom restore across cases");
        ctx.expect_equal(saves, 1, "getter called once");
        ctx.expect_equal(restores, 1, "restorer called once");
    });
    suite.add("failed requirement unwinds", [&](Context& ctx) {
        auto guard = preserve_value(global_setting);
        global_setting = 66;
        ctx.require([] { return false; }, "fatal prerequisite");
    });
    suite.add("after aborted case", [&](Context& ctx) {
        ctx.expect_equal(global_setting, 11, "aborted case restored state");
    });
    Capture reporter;
    auto result = suite.run(reporter);
    const bool ok = result.passed == 7 && result.failed == 1 &&
                    result.errors == 0 && result.inconclusive == 0 &&
                    reporter.results.size() == 8 && global_setting == 11;
    if (!ok) {
        std::cerr << "test_state: passed=" << result.passed << ", failed=" << result.failed
                  << ", errors=" << result.errors << ", inconclusive=" << result.inconclusive
                  << ", cases=" << reporter.results.size() << ", value=" << global_setting << "\n";
        for (const auto& test: reporter.results) {
            std::cerr << "  " << test.name << ": " << static_cast<int>(test.outcome) << "\n";
        }
        return 1;
    }
    return 0;
}
