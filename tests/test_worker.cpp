#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> cases;
    int checks = 0;
    void check_started(const CaseResult&, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(const CaseResult& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};
// Exercise the different mathematical and exceptional outcomes with one
// concrete predicate type, so the same Worker::check specialization sees all
// paths rather than compiling one lambda specialization per observation.
struct EvidenceScenario {
    int path;
    Evidence operator()() const {
        switch (path) {
            case 0:
                return Evidence::True;
            case 1:
                return Evidence::False;
            case 2:
                return Evidence::Indeterminate;
            case 3:
                return static_cast<Evidence>(99);
            case 4:
                throw std::runtime_error("scenario standard exception");
            default:
                throw 17;
        }
    }
};

// One callable specialization exercises the runner's normal, empty and
// exception paths, rather than generating separate lambdas per outcome.
struct RunScenario {
    int mode;
    int* invocations = nullptr;
    void operator()(WorkerBatch::Worker& worker) const {
        if (invocations)
            ++*invocations;
        if (mode == 1)
            return;
        if (mode == 2)
            throw std::runtime_error("standard runner failure");
        if (mode == 3)
            throw 17;
        worker.check([] { return true; }, "check before completion");
        if (mode == 4)
            throw std::runtime_error("failure after a worker check");
        if (mode == 5)
            throw 18;
    }
};

int main() {
    Suite suite("worker result aggregation");
    Capture capture;
    int evaluated = 0;
    suite.add("out-of-order workers merge in slot order", [&](Context& c) {
        WorkerBatch batch(3);
        std::promise<void> release;
        auto ready = release.get_future().share();
        std::jthread a([&] {
            batch.run(0, [&](WorkerBatch::Worker& worker) {
                ready.wait();
                ARIADNE_TESTING_WORKER_CHECK(worker, false);
                ARIADNE_TESTING_WORKER_CHECK(worker, Evidence::Indeterminate);
            });
        });
        std::jthread b([&] {
            batch.run(1, [&](WorkerBatch::Worker& worker) {
                ARIADNE_TESTING_WORKER_CHECK(worker, true);
                release.set_value();
                throw std::runtime_error("worker failure");
            });
        });
        std::jthread d([&] {
            batch.run(2, [&](WorkerBatch::Worker& worker) {
                ARIADNE_TESTING_WORKER_CHECK(worker,
                                             ([]() -> bool { throw std::runtime_error("predicate failure"); })());
            });
        });
        a.join();
        b.join();
        d.join();
        if (capture.checks != 0)
            throw std::logic_error("worker emitted a CHECK before flush");
        batch.flush(c);
    });
    suite.add("incomplete batch is not silently consumed", [](Context& c) {
        WorkerBatch batch(1);
        bool early = false;
        try {
            batch.flush(c);
        } catch (const std::logic_error&) {
            early = true;
        }
        c.prove(early, "premature flush rejected");
        batch.run(0, [](WorkerBatch::Worker& worker) {
            ARIADNE_TESTING_WORKER_CHECK(worker, true);
        });
        batch.flush(c);
    });
    suite.add("slot lifecycle and one evaluation", [&](Context& c) {
        WorkerBatch batch(1);
        bool wrong = false;
        try {
            batch.run(2, [](WorkerBatch::Worker&) {});
        } catch (const std::logic_error&) {
            wrong = true;
        }
        batch.run(0, [&](WorkerBatch::Worker& worker) {
            ARIADNE_TESTING_WORKER_CHECK(worker, ++evaluated == 1);
        });
        bool duplicate = false;
        try {
            batch.run(0, [](WorkerBatch::Worker&) {});
        } catch (const std::logic_error&) {
            duplicate = true;
        }
        batch.flush(c);
        bool second_flush = false;
        try {
            batch.flush(c);
        } catch (const std::logic_error&) {
            second_flush = true;
        }
        c.prove(wrong && duplicate && second_flush, "slot lifecycle enforced");
    });
    suite.add("empty worker produces an Error", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker&) {});
        batch.flush(c);
    });
    suite.add("nonstandard worker exception is captured", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker&) { throw 3; });
        batch.flush(c);
    });
    suite.add("subsequent case still works", [](Context& c) {
        c.prove(true, "case thread remains operational");
    });

    const auto summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool test, const std::string& reason) {
        if (!test) {
            std::cerr << reason << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 3 && summary.errors == 3 &&
               summary.failed == 0 && summary.inconclusive == 0 &&
               summary.skipped == 0 && capture.cases.size() == 6 &&
               capture.checks == 12 && evaluated == 1,
           "worker summary or CHECK counts");
    if (capture.cases.size() == 6) {
        const auto& first = capture.cases[0].assertions;
        verify(first.size() == 5, "worker records missing");
        if (first.size() == 5) {
            verify(first[0].outcome == Outcome::Failed &&
                       first[1].outcome == Outcome::Inconclusive &&
                       first[2].outcome == Outcome::Passed &&
                       first[3].outcome == Outcome::Error &&
                       first[4].outcome == Outcome::Error,
                   "worker outcomes changed");
            verify(first[0].description.find("worker[0]:") == 0 &&
                       first[1].description.find("worker[0]:") == 0 &&
                       first[2].description.find("worker[1]:") == 0 &&
                       first[3].description.find("worker[1]:") == 0 &&
                       first[4].description.find("worker[2]:") == 0,
                   "worker report order changed");
            verify(first[0].line != 0 && !first[0].file.empty() &&
                       first[3].detail == "worker failure" &&
                       first[4].detail == "predicate failure",
                   "worker source or diagnostic lost");
        }
        verify(capture.cases[1].outcome == Outcome::Passed &&
                   capture.cases[1].assertions.size() == 2 &&
                   capture.cases[2].outcome == Outcome::Passed &&
                   capture.cases[2].assertions.size() == 2 &&
                   capture.cases[3].outcome == Outcome::Error &&
                   capture.cases[3].assertions[0].detail ==
                       "worker completed without recording checks" &&
                   capture.cases[4].outcome == Outcome::Error &&
                   capture.cases[4].assertions[0].detail == "non-standard worker exception" &&
                   capture.cases[5].outcome == Outcome::Passed,
               "worker lifecycle cases changed");
    }
    // These cases exercise valid public error handling that cannot arise in
    // an already-running worker batch's normal success path.
    Suite contracts("worker boundary contracts");
    contracts.add("zero workers rejected", [](Context& c) {
        bool rejected = false;
        try {
            WorkerBatch empty(0);
        } catch (const std::invalid_argument& error) {
            rejected = std::string_view(error.what()) ==
                       "WorkerBatch needs a nonzero slot count";
        }
        c.prove(rejected, "zero-slot worker batch must be rejected");
    });
    contracts.add("workers cannot start after flush", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker& worker) {
            worker.check([] { return true; }, "first worker check");
        });
        batch.flush(c);
        bool rejected = false;
        try {
            batch.run(0, [](WorkerBatch::Worker&) {});
        } catch (const std::logic_error& error) {
            rejected = std::string_view(error.what()) ==
                       "invalid or repeated worker slot";
        }
        c.prove(rejected, "flushed batch must not accept new workers");
    });
    contracts.add("invalid evidence records Error", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker& worker) {
            worker.check([] { return static_cast<Evidence>(99); },
                         "malformed evidence");
        });
        batch.flush(c);
    });
    contracts.add("nonstandard predicate exception records Error", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker& worker) {
            worker.check([]() -> bool { throw 17; },
                         "nonstandard predicate");
        });
        batch.flush(c);
    });
    Capture contracts_capture;
    const auto contracts_summary = contracts.run(contracts_capture);
    verify(contracts_summary.passed == 2 && contracts_summary.errors == 2 &&
               contracts_summary.failed == 0 && contracts_summary.inconclusive == 0 &&
               contracts_capture.cases.size() == 4 && contracts_capture.checks == 5,
           "worker boundary outcome or CHECK count changed");
    if (contracts_capture.cases.size() == 4) {
        verify(contracts_capture.cases[0].outcome == Outcome::Passed &&
                   contracts_capture.cases[1].outcome == Outcome::Passed &&
                   contracts_capture.cases[2].outcome == Outcome::Error &&
                   contracts_capture.cases[3].outcome == Outcome::Error,
               "worker boundary classifications changed");
        verify(contracts_capture.cases[2].assertions.size() == 1 &&
                   contracts_capture.cases[2].assertions[0].detail == "invalid evidence" &&
                   contracts_capture.cases[3].assertions.size() == 1 &&
                   contracts_capture.cases[3].assertions[0].detail ==
                       "non-standard worker predicate exception",
               "worker error diagnostics lost their semantic causes");
    }
    Suite scenario_suite("worker predicate outcome matrix");
    scenario_suite.add("same callable supports all outcomes", [](Context& c) {
        WorkerBatch batch(1);
        batch.run(0, [](WorkerBatch::Worker& worker) {
            for (int scenario = 0; scenario != 6; ++scenario) {
                worker.check(EvidenceScenario{scenario},
                             "scenario " + std::to_string(scenario));
            }
        });
        batch.flush(c);
    });
    Capture scenario_capture;
    const auto scenario_summary = scenario_suite.run(scenario_capture);
    verify(scenario_summary.errors == 1 && scenario_summary.passed == 0 &&
               scenario_capture.cases.size() == 1 && scenario_capture.checks == 6,
           "single predicate specialization must preserve all six observations");
    if (scenario_capture.cases.size() == 1) {
        const auto& records = scenario_capture.cases[0].assertions;
        verify(records.size() == 6,
               "outcome matrix must record each worker observation");
        if (records.size() == 6) {
            const Outcome expected[]{Outcome::Passed, Outcome::Failed,
                                     Outcome::Inconclusive, Outcome::Error, Outcome::Error, Outcome::Error};
            for (std::size_t i = 0; i < 6; ++i) {
                verify(records[i].outcome == expected[i] &&
                           records[i].description ==
                               "worker[0]: scenario " + std::to_string(i),
                       "worker predicate outcome or order changed at " + std::to_string(i));
            }
            verify(records[3].detail == "invalid evidence" &&
                       records[4].detail == "scenario standard exception" &&
                       records[5].detail == "non-standard worker predicate exception",
                   "worker predicate exception diagnostics changed");
        }
    }

    // A recorded check must survive a later exception, whereas a callback
    // returning without any checks must be classified as Error.
    Suite runner_matrix("worker callback lifecycle matrix");
    for (int mode = 0; mode < 6; ++mode) {
        runner_matrix.add("runner mode " + std::to_string(mode), [mode](Context& c) {
            WorkerBatch batch(1);
            batch.run(0, RunScenario{mode});
            batch.flush(c);
        });
    }
    Capture runner_capture;
    const auto runner_summary = runner_matrix.run(runner_capture);
    verify(runner_summary.passed == 1 && runner_summary.errors == 5 &&
               runner_summary.failed == 0 && runner_summary.inconclusive == 0 &&
               runner_capture.checks == 8 && runner_capture.cases.size() == 6,
           "worker callback outcome matrix summary changed");
    if (runner_capture.cases.size() == 6) {
        for (std::size_t i = 0; i < 6; ++i) {
            const auto expected = i == 0 ? Outcome::Passed : Outcome::Error;
            const auto expected_checks = i >= 4 ? 2u : 1u;
            verify(runner_capture.cases[i].outcome == expected &&
                       runner_capture.cases[i].assertions.size() == expected_checks,
                   "worker callback outcome or check count changed at mode " +
                       std::to_string(i));
        }
        verify(runner_capture.cases[1].assertions[0].detail ==
                       "worker completed without recording checks" &&
                   runner_capture.cases[2].assertions[0].detail ==
                       "standard runner failure" &&
                   runner_capture.cases[3].assertions[0].detail ==
                       "non-standard worker exception" &&
                   runner_capture.cases[4].assertions[1].detail ==
                       "failure after a worker check" &&
                   runner_capture.cases[5].assertions[1].detail ==
                       "non-standard worker exception",
               "worker callback diagnostics changed");
    }

    // Invalid slots, repeated slots and a flushed batch must reject the
    // callback before invoking it. Reuse the same RunScenario type.
    Suite runner_rejections("worker runner precondition contracts");
    runner_rejections.add("invalid and repeated callbacks are not evaluated", [](Context& c) {
        WorkerBatch batch(1);
        int invocations = 0;
        bool outside = false, repeated = false, flushed = false;
        try {
            batch.run(1, RunScenario{0, &invocations});
        } catch (const std::logic_error&) {
            outside = true;
        }
        batch.run(0, RunScenario{0, &invocations});
        try {
            batch.run(0, RunScenario{0, &invocations});
        } catch (const std::logic_error&) {
            repeated = true;
        }
        batch.flush(c);
        try {
            batch.run(0, RunScenario{0, &invocations});
        } catch (const std::logic_error&) {
            flushed = true;
        }
        c.prove(outside && repeated && flushed && invocations == 1,
                "rejected runner callbacks must never be invoked");
    });
    Capture rejection_capture;
    const auto rejection_summary = runner_rejections.run(rejection_capture);
    verify(rejection_summary.passed == 1 && rejection_summary.failed == 0 &&
               rejection_summary.errors == 0 && rejection_capture.checks == 2 &&
               rejection_capture.cases.size() == 1,
           "worker runner preconditions or check replay changed");
    return ok ? 0 : 1;
}
