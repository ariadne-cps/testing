#include "ariadne-testing.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Ariadne::Testing;
using namespace std::chrono_literals;

struct Capture final : Reporter {
    std::vector<CaseResult> cases;
    int checks = 0;
    void check_started(CaseResult const&, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

// Route atomic progress checks through one type-erased instantiation so a
// single template group observes immediate, polling and timeout outcomes.
inline void atomic_erased(Context& c, std::atomic<int> const& state,
                          std::function<bool(int)> pred,
                          std::chrono::milliseconds timeout, std::string_view desc) {
    c.expect_atomic(state, std::move(pred), timeout, desc);
}

int main() {
    Capture capture;
    Suite suite("atomic progress and deadlines");
    int predicate_calls = 0;
    suite.add("initial progress meets goal at zero timeout", [&](Context& c) {
        std::atomic<int> state{4};
        atomic_erased(c, state, [&](int observed) {
            ++predicate_calls;
            return capture.checks == 1 && observed >= 3;
        }, 0ms, "already ready");
    });
    suite.add("cross-thread release/acquire progress", [&](Context& c) {
        std::atomic<int> state{0};
        std::promise<void> ready;
        std::promise<void> allow_update;
        auto release = allow_update.get_future();
        std::jthread worker([&] {
            ready.set_value();
            release.wait();
            state.store(5, std::memory_order_release);
        });
        ready.get_future().wait();
        allow_update.set_value();
        atomic_erased(c, state, [&](int observed) {
            ++predicate_calls;
            return capture.checks == 2 && observed >= 5;
        }, 5000ms, "worker progress");
    });
    suite.add("zero timeout reports last state", [](Context& c) {
        std::atomic<int> state{2};
        atomic_erased(c, state, [](int observed) { return observed >= 3; },
                        0ms, "not yet ready");
    });
    suite.add("positive timeout reports last state", [](Context& c) {
        std::atomic<int> state{7};
        atomic_erased(c, state, [](int observed) { return observed == 9; },
                        3ms, "bounded timeout");
    });
    suite.add("negative timeout never invokes predicate", [&](Context& c) {
        std::atomic<int> state{1};
        atomic_erased(c, state, [&](int) {
            ++predicate_calls;
            return true;
        }, -1ms, "invalid deadline");
    });
    suite.add("predicate exception is Error", [](Context& c) {
        std::atomic<int> state{1};
        atomic_erased(c, state, [](int) -> bool {
            throw std::logic_error("invalid progress predicate");
        }, 0ms, "predicate throws");
    });
    suite.add("later case remains unaffected", [](Context& c) {
        c.prove(true, "case thread can still issue checks");
    });

    auto const summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool condition, std::string const& description) {
        if (!condition) {
            std::cerr << description << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 3 && summary.failed == 2 &&
           summary.errors == 2 && summary.skipped == 0 &&
           summary.inconclusive == 0 && capture.cases.size() == 7 &&
           capture.checks == 7, "atomic progress summary or CHECK count");
    verify(predicate_calls >= 2, "atomic predicates not exercised");
    if (capture.cases.size() == 7) {
        std::vector<Outcome> const outcomes{
            Outcome::Passed, Outcome::Passed, Outcome::Failed, Outcome::Failed,
            Outcome::Error, Outcome::Error, Outcome::Passed
        };
        for (std::size_t i = 0; i < outcomes.size(); ++i) {
            verify(capture.cases[i].outcome == outcomes[i],
                   "wrong atomic progress outcome " + std::to_string(i));
            verify(capture.cases[i].assertions.size() == 1,
                   "wrong assertion count " + std::to_string(i));
        }
        if (capture.cases[2].assertions.size() == 1)
            verify(capture.cases[2].assertions[0].actual == "2" &&
                   capture.cases[2].assertions[0].detail.find("timeout; waited=") == 0,
                   "zero timeout must report observed atomic value");
        if (capture.cases[3].assertions.size() == 1)
            verify(capture.cases[3].assertions[0].actual == "7" &&
                   capture.cases[3].assertions[0].detail.find("limit=3ms") != std::string::npos,
                   "positive timeout must preserve limit and state");
        if (capture.cases[4].assertions.size() == 1)
            verify(capture.cases[4].assertions[0].detail == "negative timeout",
                   "negative timeout must be rejected");
        if (capture.cases[5].assertions.size() == 1)
            verify(capture.cases[5].assertions[0].detail == "invalid progress predicate",
                   "predicate exception must retain diagnostic");
    }
    return ok ? 0 : 1;
}
