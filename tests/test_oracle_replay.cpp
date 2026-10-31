#include "ariadne-testing.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Ariadne::Testing;

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    int announced = 0;
    void check_started(CaseResult const&, std::size_t, std::string_view) override {
        ++announced;
    }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

int main() {
    using K = OracleRowId;
    std::vector<OracleRow> const manifest{
        {K{"a",0,0},true},{K{"a",0,1},true},{K{"b",1,0},true},{K{"c",2,0},true}};
    std::vector<K> requested{manifest[3].id, manifest[1].id};
    auto full = plan_oracle_replay(manifest);
    auto partial = plan_oracle_replay(manifest, requested);
    auto duplicate = plan_oracle_replay(manifest, std::vector<K>{manifest[0].id, manifest[0].id});
    auto unknown = plan_oracle_replay(manifest, std::vector<K>{K{"absent",1,0}});
    auto bad_manifest = manifest; bad_manifest.push_back(manifest[0]);
    auto bad = plan_oracle_replay(bad_manifest);
    auto reordered = plan_oracle_replay(manifest,
        std::vector<K>{manifest[2].id,manifest[0].id,manifest[3].id,manifest[1].id});
    auto malformed = partial;
    malformed.indices = {2,1};

    bool ok = true;
    auto verify = [&](bool p, std::string const& msg) {
        if (!p) { std::cerr << msg << '\n'; ok = false; }
    };
    verify(full.valid && full.complete && full.structurally_valid() &&
           full.indices == std::vector<std::size_t>({0,1,2,3}),
           "full replay must preserve every index");
    verify(partial.valid && !partial.complete && partial.structurally_valid() &&
           partial.indices == std::vector<std::size_t>({1,3}) &&
           partial.includes(1) && partial.includes(3) &&
           !partial.includes(0) && !partial.includes(4),
           "partial replay must preserve manifest order and exact identities");
    verify(!duplicate.valid && !unknown.valid && !bad.valid &&
           !duplicate.structurally_valid() && !unknown.structurally_valid(),
           "invalid selections must be rejected");
    verify(reordered.valid && reordered.complete &&
           reordered.indices == full.indices,
           "request ordering must not change complete manifest ordering");
    verify(!malformed.structurally_valid() && !malformed.includes(2),
           "fabricated unsorted plans must not be executable");
    auto const rng_seed = std::mt19937::default_seed;
    std::mt19937 original(rng_seed), observed(rng_seed);
    // An explicitly selected row still must consume all preceding RNG
    // samples when the consumer uses one shared random stream.
    std::vector<std::uint32_t> draws;
    for (std::size_t i=0; i<manifest.size(); ++i) {
        auto const draw=observed();
        if (partial.includes(i)) draws.push_back(draw);
    }
    std::vector<std::uint32_t> full_draws;
    for (std::size_t i=0; i<manifest.size(); ++i) full_draws.push_back(original());
    verify(draws.size()==2 && draws[0]==full_draws[1] && draws[1]==full_draws[3] &&
           observed()==original(),"selection must not reset or skip shared RNG advancement");
    std::mt19937 before(rng_seed), after(rng_seed);
    auto selection = plan_oracle_replay(manifest, requested);
    (void) selection;
    verify(before()==after(),"planning must not access RNG state");

    Suite suite("oracle replay outcome and CHECK order");
    Capture capture;
    int first=0,second=0,plan_calls=0;
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
          ([&]() -> std::vector<OracleRow> const& {
              if (capture.announced != 5) throw std::logic_error("CHECK not announced");
              ++first; return manifest;
          })(),
          ([&]() -> std::vector<OracleRow> const& {
              if (first!=1 || capture.announced!=5)
                  throw std::logic_error("second operand before first/CHECK");
              ++second;return manifest;
          })());
    });
    suite.add("deferred replay selection", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c,
          ([&]() -> OracleReplayPlan const& {
              if (capture.announced!=6) throw std::logic_error("CHECK not announced");
              ++plan_calls; return full;
          })());
    });
    auto const summary=suite.run(capture);
    verify(summary.passed==3 && summary.inconclusive==1 &&
           summary.errors==2 && summary.failed==0 &&
           capture.cases.size()==6 && capture.announced==6 &&
           first==1 && second==1 && plan_calls==1,
           "replay outcome classification or once-after-CHECK contract changed");
    if (capture.cases.size()==6) {
        std::vector<Outcome> const outcomes{
            Outcome::Passed,Outcome::Inconclusive,Outcome::Error,
            Outcome::Error,Outcome::Passed,Outcome::Passed};
        for (std::size_t i=0;i<outcomes.size();++i)
            verify(capture.cases[i].outcome==outcomes[i],
                   "wrong replay result at case "+std::to_string(i));
        verify(capture.cases[1].assertions.size()==1 &&
               capture.cases[1].assertions[0].detail.find("partial replay")!=std::string::npos,
               "partial coverage is not explicit");
    }
    return ok?0:1;
}
