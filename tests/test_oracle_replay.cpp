/***************************************************************************
 *            test_oracle_replay.cpp
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

#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    int announced = 0;
    void check_started(const CaseResult&, std::size_t, std::string_view) override {
        ++announced;
    }
    void case_finished(const CaseResult& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

int main() {
    using K = OracleRowId;
    const std::vector<OracleRow> manifest{
        {K{"a", 0, 0}, true},
        {K{"a", 0, 1}, true},
        {K{"b", 1, 0}, true},
        {K{"c", 2, 0}, true}
    };
    std::vector<K> requested{manifest[3].id, manifest[1].id};
    auto full = plan_oracle_replay(manifest);
    auto partial = plan_oracle_replay(manifest, requested);
    auto duplicate = plan_oracle_replay(manifest, std::vector<K>{manifest[0].id, manifest[0].id});
    auto unknown = plan_oracle_replay(manifest, std::vector<K>{
                                                    K{"absent", 1, 0}
    });
    auto empty_key = plan_oracle_replay(manifest, std::vector<K>{
                                                      K{"", 0, 0}
    });
    auto bad_manifest = manifest;
    bad_manifest.push_back(manifest[0]);
    auto bad = plan_oracle_replay(bad_manifest);
    auto reordered = plan_oracle_replay(manifest,
                                        std::vector<K>{manifest[2].id, manifest[0].id, manifest[3].id, manifest[1].id});
    auto malformed = partial;
    malformed.indices = {2, 1};

    bool ok = true;
    auto verify = [&](bool p, const std::string& msg) {
        if (!p) {
            std::cerr << msg << '\n';
            ok = false;
        }
    };
    verify(full.valid && full.complete && full.structurally_valid() &&
               full.indices == std::vector<std::size_t>({0, 1, 2, 3}),
           "full replay must preserve every index");
    verify(partial.valid && !partial.complete && partial.structurally_valid() &&
               partial.indices == std::vector<std::size_t>({1, 3}) &&
               partial.includes(1) && partial.includes(3) &&
               !partial.includes(0) && !partial.includes(4),
           "partial replay must preserve manifest order and exact identities");
    verify(!empty_key.valid && !empty_key.structurally_valid() &&
               empty_key.reason == "selected row has an empty key",
           "selected row with empty key must be rejected before lookup");
    verify(!duplicate.valid && !unknown.valid && !bad.valid &&
               !duplicate.structurally_valid() && !unknown.structurally_valid(),
           "invalid selections must be rejected");
    verify(reordered.valid && reordered.complete &&
               reordered.indices == full.indices,
           "request ordering must not change complete manifest ordering");
    verify(!malformed.structurally_valid() && !malformed.includes(2),
           "fabricated unsorted plans must not be executable");
    // Even externally constructed replay plans must reject every malformed
    // structural shape before a consumer can use their indices.
    auto zero_rows = full;
    zero_rows.total_rows = 0;
    auto no_indices = full;
    no_indices.indices.clear();
    auto excess_indices = full;
    excess_indices.indices.push_back(0);
    auto outside_manifest = full;
    outside_manifest.indices.back() = full.total_rows;
    verify(!zero_rows.structurally_valid() && !no_indices.structurally_valid() &&
               !excess_indices.structurally_valid() &&
               !outside_manifest.structurally_valid() &&
               !outside_manifest.includes(0),
           "fabricated replay plans must reject empty, excessive and out-of-range indices");
    const auto rng_seed = std::mt19937::default_seed;
    std::mt19937 original(rng_seed), observed(rng_seed);
    // An explicitly selected row still must consume all preceding RNG
    // samples when the consumer uses one shared random stream.
    std::vector<std::uint32_t> draws;
    for (std::size_t i = 0; i < manifest.size(); ++i) {
        const auto draw = observed();
        if (partial.includes(i))
            draws.push_back(draw);
    }
    std::vector<std::uint32_t> full_draws;
    for (std::size_t i = 0; i < manifest.size(); ++i)
        full_draws.push_back(original());
    verify(draws.size() == 2 && draws[0] == full_draws[1] && draws[1] == full_draws[3] &&
               observed() == original(),
           "selection must not reset or skip shared RNG advancement");
    std::mt19937 before(rng_seed), after(rng_seed);
    auto selection = plan_oracle_replay(manifest, requested);
    (void)selection;
    verify(before() == after(), "planning must not access RNG state");

    Suite suite("oracle replay outcome and CHECK order");
    Capture capture;
    int first = 0, second = 0, plan_calls = 0;
    suite.add("full replay", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c, full);
    });
    suite.add("partial replay", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c, partial);
    });
    suite.add("invalid replay", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c, duplicate);
    });
    suite.add("fabricated plan", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c, malformed);
    });
    suite.add("deferred ordered oracle evaluation", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(c,
                                               ([&]() -> const std::vector<OracleRow>& {
                                                   if (capture.announced != 5)
                                                       throw std::logic_error("CHECK not announced");
                                                   ++first;
                                                   return manifest;
                                               })(),
                                               ([&]() -> const std::vector<OracleRow>& {
                                                   if (first != 1 || capture.announced != 5)
                                                       throw std::logic_error("second operand before first/CHECK");
                                                   ++second;
                                                   return manifest;
                                               })());
    });
    suite.add("deferred replay selection", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c,
                                                 ([&]() -> const OracleReplayPlan& {
                                                     if (capture.announced != 6)
                                                         throw std::logic_error("CHECK not announced");
                                                     ++plan_calls;
                                                     return full;
                                                 })());
    });
    const auto summary = suite.run(capture);
    verify(summary.passed == 3 && summary.inconclusive == 1 &&
               summary.errors == 2 && summary.failed == 0 &&
               capture.cases.size() == 6 && capture.announced == 6 &&
               first == 1 && second == 1 && plan_calls == 1,
           "replay outcome classification or once-after-CHECK contract changed");
    if (capture.cases.size() == 6) {
        const std::vector<Outcome> outcomes{
            Outcome::Passed, Outcome::Inconclusive, Outcome::Error,
            Outcome::Error, Outcome::Passed, Outcome::Passed};
        for (std::size_t i = 0; i < outcomes.size(); ++i)
            verify(capture.cases[i].outcome == outcomes[i],
                   "wrong replay result at case " + std::to_string(i));
        verify(capture.cases[1].assertions.size() == 1 &&
                   capture.cases[1].assertions[0].detail.find("partial replay") != std::string::npos,
               "partial coverage is not explicit");
    }
    // Cover the public eager check as well as the deferred macro route.
    Suite direct_replays("direct oracle replay");
    direct_replays.add("complete", [&](Context& c) {
        c.expect_oracle_replay(full, "direct full replay");
    });
    direct_replays.add("selected subset", [&](Context& c) {
        c.expect_oracle_replay(partial, "direct partial replay");
    });
    direct_replays.add("invalid plan", [&](Context& c) {
        c.expect_oracle_replay(malformed, "direct invalid replay");
    });
    Capture direct_capture;
    const auto direct_summary = direct_replays.run(direct_capture);
    verify(direct_summary.passed == 1 && direct_summary.inconclusive == 1 &&
               direct_summary.errors == 1 && direct_capture.announced == 3 &&
               direct_capture.cases.size() == 3,
           "direct replay outcome and CHECK lifecycle");
    if (direct_capture.cases.size() == 3) {
        verify(direct_capture.cases[0].assertions.size() == 1 &&
                   direct_capture.cases[0].assertions[0].outcome == Outcome::Passed &&
                   direct_capture.cases[1].assertions[0].outcome == Outcome::Inconclusive &&
                   direct_capture.cases[2].assertions[0].outcome == Outcome::Error,
               "direct replay must retain exact assertion classification");
    }
    return ok ? 0 : 1;
}
