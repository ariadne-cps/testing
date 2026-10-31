#include "ariadne-testing.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> cases;
    int checks = 0;
    void check_started(CaseResult const&, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
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
                ARIADNE_WORKER_CHECK(worker, false);
                ARIADNE_WORKER_CHECK(worker, Evidence::Indeterminate);
            });
        });
        std::jthread b([&] {
            batch.run(1, [&](WorkerBatch::Worker& worker) {
                ARIADNE_WORKER_CHECK(worker, true);
                release.set_value();
                throw std::runtime_error("worker failure");
            });
        });
        std::jthread d([&] {
            batch.run(2, [&](WorkerBatch::Worker& worker) {
                ARIADNE_WORKER_CHECK(worker,
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
        try { batch.flush(c); } catch (std::logic_error const&) { early = true; }
        c.prove(early, "premature flush rejected");
        batch.run(0, [](WorkerBatch::Worker& worker) {
            ARIADNE_WORKER_CHECK(worker, true);
        });
        batch.flush(c);
    });
    suite.add("slot lifecycle and one evaluation", [&](Context& c) {
        WorkerBatch batch(1);
        bool wrong = false;
        try { batch.run(2, [](WorkerBatch::Worker&) {}); }
        catch (std::logic_error const&) { wrong = true; }
        batch.run(0, [&](WorkerBatch::Worker& worker) {
            ARIADNE_WORKER_CHECK(worker, ++evaluated == 1);
        });
        bool duplicate = false;
        try { batch.run(0, [](WorkerBatch::Worker&) {}); }
        catch (std::logic_error const&) { duplicate = true; }
        batch.flush(c);
        bool second_flush = false;
        try { batch.flush(c); } catch (std::logic_error const&) { second_flush = true; }
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

    auto const summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool test, std::string const& reason) {
        if (!test) { std::cerr << reason << '\n'; ok = false; }
    };
    verify(summary.passed == 3 && summary.errors == 3 &&
           summary.failed == 0 && summary.inconclusive == 0 &&
           summary.skipped == 0 && capture.cases.size() == 6 &&
           capture.checks == 12 && evaluated == 1, "worker summary or CHECK counts");
    if (capture.cases.size() == 6) {
        auto const& first = capture.cases[0].assertions;
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
    return ok ? 0 : 1;
}
