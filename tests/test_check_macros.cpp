#include "ariadne-testing.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace Ariadne::Testing;
using namespace std::chrono_literals;

enum class DomainStatus { Sat, Unsat, Unknown };

struct DomainResult {
    DomainStatus status;
    int reason;
};

struct Incomplete : std::runtime_error {
    int progress;
    explicit Incomplete(int n) : std::runtime_error("incomplete"), progress(n) {}
};

DomainResult solve(DomainStatus status) { return {status, 42}; }
DomainResult incomplete() { throw Incomplete(7); }

struct Capture final : Reporter {
    int checks = 0;
    std::vector<CaseResult> cases;
    void check_started(CaseResult const&, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

struct MacroFixture : ContextFixture {
    using ContextFixture::ContextFixture;

    void test_status() {
        ARIADNE_TESTING_EXPECT_STATUS(solve(DomainStatus::Unknown),
            [](DomainResult const& r) { return r.status; },
            (std::initializer_list<DomainStatus>{DomainStatus::Sat, DomainStatus::Unknown}),
            [](DomainResult const& r) { return r.reason == 42; });
    }
    void test_status_only() {
        ARIADNE_TESTING_EXPECT_STATUS_ONLY(solve(DomainStatus::Unsat),
            [](DomainResult const& r) { return r.status; },
            (std::initializer_list<DomainStatus>{DomainStatus::Unsat}));
    }
    void test_alternative() {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE(incomplete(),
            [](DomainResult const&) { return false; },
            [](std::exception const& e) {
                auto const* partial = dynamic_cast<Incomplete const*>(&e);
                return partial && partial->progress == 7;
            });
    }
    void test_alternative_void() {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE((void)0,
            [](std::monostate const&) { return true; },
            [](std::exception const&) { return false; });
    }
    void test_atomic() {
        std::atomic<int> state{8};
        ARIADNE_TESTING_EXPECT_ATOMIC(state, [](int observed) { return observed == 8; }, 0ms);
    }
    void test_ready() {
        std::promise<int> promise;
        auto future = promise.get_future();
        promise.set_value(3);
        ARIADNE_TESTING_EXPECT_READY(future, 0ms);
    }
    void test_eventually() {
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        ARIADNE_TESTING_EXPECT_EVENTUALLY(cv, lock, [] { return true; }, 0ms);
    }
};

int main() {
    Suite suite("public checks macros");
    Capture report;
    int operations = 0, selectors = 0, verifiers = 0;
    int rejected_verifiers = 0;
    suite.add("context typed status", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_STATUS_CTX(c,
            (++operations, solve(DomainStatus::Unknown)),
            [&](DomainResult const& r) { ++selectors; return r.status; },
            (std::initializer_list<DomainStatus>{DomainStatus::Sat, DomainStatus::Unknown}),
            [&](DomainResult const& r) {
                ++verifiers;
                return r.reason == 42 && report.checks == 1;
            });
    });
    suite.add("context status only", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_STATUS_ONLY_CTX(c,
            (++operations, solve(DomainStatus::Unsat)),
            [&](DomainResult const& r) { ++selectors; return r.status; },
            (std::initializer_list<DomainStatus>{DomainStatus::Unsat}));
    });
    suite.add("context unpermitted status", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_STATUS_CTX(c, (++operations, solve(DomainStatus::Sat)),
            [&](DomainResult const& r) { ++selectors; return r.status; },
            (std::initializer_list<DomainStatus>{DomainStatus::Unsat}),
            [&](DomainResult const&) { ++rejected_verifiers; return true; });
    });
    suite.add("context returned alternative", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(c,
            (++operations, solve(DomainStatus::Sat)),
            [&](DomainResult const& r) {
                ++verifiers;
                return r.status == DomainStatus::Sat && report.checks == 4;
            },
            [&](std::exception const&) {
                ++rejected_verifiers;
                return false;
            });
    });
    suite.add("context exception alternative", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(c, incomplete(),
            [&](DomainResult const&) {
                ++rejected_verifiers;
                return false;
            },
            [&](std::exception const& e) {
                ++verifiers;
                auto const* partial = dynamic_cast<Incomplete const*>(&e);
                return partial && partial->progress == 7 && report.checks == 5;
            });
    });
    suite.add("context atomic progress", [&](Context& c) {
        std::atomic<int> value{4};
        ARIADNE_TESTING_EXPECT_ATOMIC_CTX(c, value,
            [&](int observed) { ++verifiers; return observed >= 4 && report.checks == 6; },
            0ms);
    });
    suite.add("context future readiness", [](Context& c) {
        std::promise<void> promise;
        auto future = promise.get_future();
        promise.set_value();
        ARIADNE_TESTING_EXPECT_READY_CTX(c, future, 0ms);
    });
    suite.add("context condition eventually", [&](Context& c) {
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX(c, cv, lock,
            [&] { ++verifiers; return report.checks == 8; }, 0ms);
    });
    suite.add("context atomic timeout", [](Context& c) {
        std::atomic<int> value{0};
        ARIADNE_TESTING_EXPECT_ATOMIC_CTX(c, value, [](int observed) { return observed == 1; }, 0ms);
    });
    suite.add("context condition timeout", [](Context& c) {
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX(c, cv, lock, [] { return false; }, 0ms);
    });
    suite.add("context deferred future", [](Context& c) {
        auto future = std::async(std::launch::deferred, [] { return 42; });
        ARIADNE_TESTING_EXPECT_READY_CTX(c, future, 0ms);
    });
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_status);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_status_only);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_alternative);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_alternative_void);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_atomic);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_ready);
    ARIADNE_TESTING_ADD_TEST_TO(suite, MacroFixture, test_eventually);

    Summary const summary = suite.run(report);
    bool ok = true;
    auto verify = [&](bool condition, std::string const& message) {
        if (!condition) {
            std::cerr << message << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 14 && summary.failed == 4 &&
           summary.errors == 0 && summary.inconclusive == 0 &&
           summary.skipped == 0 && report.cases.size() == 18 &&
           report.checks == 18,
           "public macro summary/check count changed");
    verify(operations == 4 && selectors == 3 && verifiers == 5 &&
           rejected_verifiers == 0,
           "deferred operations or postconditions were evaluated incorrectly");
    if (report.cases.size() == 18) {
        for (std::size_t i = 0; i < report.cases.size(); ++i) {
            Outcome const expected = i == 2 || i == 8 || i == 9 || i == 10
                ? Outcome::Failed : Outcome::Passed;
            verify(report.cases[i].outcome == expected &&
                   report.cases[i].assertions.size() == 1,
                   "incorrect assertion outcome for macro " + std::to_string(i));
        }
        verify(report.cases[0].assertions[0].description.find("status in") != std::string::npos,
               "status macro must preserve source-spelled description");
        verify(report.cases[3].assertions[0].description.find("result or exception") !=
                   std::string::npos, "alternative macro must label its branch");
        verify(report.cases[8].assertions[0].actual == "0",
               "atomic timeout must preserve last observed value");
        verify(report.cases[10].assertions[0].actual == "deferred",
               "future macro must preserve deferred status");
    }
    return ok ? 0 : 1;
}
