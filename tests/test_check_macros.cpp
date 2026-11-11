#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;
using namespace std::chrono_literals;

enum class DomainStatus { Sat,
                          Unsat,
                          Unknown };

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
    void check_started(const CaseResult&, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(const CaseResult& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

bool throws_from_binary_predicate(int, int) {
    throw std::runtime_error("binary predicate failure");
}

struct MacroFixture : ContextFixture {
    using ContextFixture::ContextFixture;

    void test_status() {
        ARIADNE_TESTING_EXPECT_STATUS(solve(DomainStatus::Unknown), [](const DomainResult& r) { return r.status; }, (std::initializer_list<DomainStatus>{DomainStatus::Sat, DomainStatus::Unknown}), [](const DomainResult& r) { return r.reason == 42; });
    }
    void test_status_only() {
        ARIADNE_TESTING_EXPECT_STATUS_ONLY(solve(DomainStatus::Unsat), [](const DomainResult& r) { return r.status; }, (std::initializer_list<DomainStatus>{DomainStatus::Unsat}));
    }
    void test_alternative() {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE(incomplete(), [](const DomainResult&) { return false; }, [](const std::exception& e) {
                 const auto * partial = dynamic_cast< const Incomplete *>(&e);
                return partial && partial->progress == 7; });
    }
    void test_alternative_void() {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE((void)0,
                                           [](const std::monostate&) { return true; },
                                           [](const std::exception&) { return false; });
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
        ARIADNE_TESTING_EXPECT_STATUS_CTX(c, (++operations, solve(DomainStatus::Unknown)), [&](const DomainResult& r) { ++selectors; return r.status; }, (std::initializer_list<DomainStatus>{DomainStatus::Sat, DomainStatus::Unknown}), [&](const DomainResult& r) {
                ++verifiers;
                return r.reason == 42 && report.checks == 1; });
    });
    suite.add("context status only", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_STATUS_ONLY_CTX(c, (++operations, solve(DomainStatus::Unsat)), [&](const DomainResult& r) { ++selectors; return r.status; }, (std::initializer_list<DomainStatus>{DomainStatus::Unsat}));
    });
    suite.add("context unpermitted status", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_STATUS_CTX(c, (++operations, solve(DomainStatus::Sat)), [&](const DomainResult& r) { ++selectors; return r.status; }, (std::initializer_list<DomainStatus>{DomainStatus::Unsat}), [&](const DomainResult&) { ++rejected_verifiers; return true; });
    });
    suite.add("context returned alternative", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(c, (++operations, solve(DomainStatus::Sat)), [&](const DomainResult& r) {
                ++verifiers;
                return r.status == DomainStatus::Sat && report.checks == 4; }, [&](const std::exception&) {
                ++rejected_verifiers;
                return false; });
    });
    suite.add("context exception alternative", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(c, incomplete(), [&](const DomainResult&) {
                ++rejected_verifiers;
                return false; }, [&](const std::exception& e) {
                ++verifiers;
                 const auto * partial = dynamic_cast< const Incomplete *>(&e);
                return partial && partial->progress == 7 && report.checks == 5; });
    });
    suite.add("context atomic progress", [&](Context& c) {
        std::atomic<int> value{4};
        ARIADNE_TESTING_EXPECT_ATOMIC_CTX(c, value, [&](int observed) { ++verifiers; return observed >= 4 && report.checks == 6; }, 0ms);
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
        ARIADNE_TESTING_EXPECT_EVENTUALLY_CTX(c, cv, lock, [&] { ++verifiers; return report.checks == 8; }, 0ms);
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

    const Summary summary = suite.run(report);
    bool ok = true;
    auto verify = [&](bool condition, const std::string& message) {
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
            const Outcome expected = i == 2 || i == 8 || i == 9 || i == 10
                                       ? Outcome::Failed
                                       : Outcome::Passed;
            verify(report.cases[i].outcome == expected &&
                       report.cases[i].assertions.size() == 1,
                   "incorrect assertion outcome for macro " + std::to_string(i));
        }
        verify(report.cases[0].assertions[0].description.find("status in") != std::string::npos,
               "status macro must preserve source-spelled description");
        verify(report.cases[3].assertions[0].description.find("result or exception") !=
                   std::string::npos,
               "alternative macro must label its branch");
        verify(report.cases[8].assertions[0].actual == "0",
               "atomic timeout must preserve last observed value");
        verify(report.cases[10].assertions[0].actual == "deferred",
               "future macro must preserve deferred status");
    }

    // Keep the same typed macro expansion across accepted/unpermitted status,
    // every postcondition outcome, and both result/exception alternatives.
    // Deferred operations and verifiers are evaluated only on the active path.
    Suite macro_matrix("shared macro control-flow specialization");
    const std::array<Outcome, 12> macro_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Failed, Outcome::Error,
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Passed, Outcome::Failed, Outcome::Error, Outcome::Error};
    for (int mode = 0; mode < static_cast<int>(macro_outcomes.size()); ++mode) {
        macro_matrix.add("macro mode " + std::to_string(mode), [mode](Context& c) {
            std::function<DomainResult()> operation = [mode] {
                if (mode >= 8 && mode <= 10)
                    throw Incomplete(7);
                return DomainResult{mode == 3 ? DomainStatus::Unsat : DomainStatus::Sat, 42};
            };
            std::function<DomainStatus(const DomainResult&)> selector =
                [](const DomainResult& r) { return r.status; };
            std::function<Evidence(const DomainResult&)> verifier =
                [mode](const DomainResult&) {
                    if (mode == 4 || mode == 11)
                        throw std::runtime_error("macro result verification failed");
                    if (mode == 1 || mode == 6)
                        return Evidence::False;
                    if (mode == 2 || mode == 7)
                        return Evidence::Indeterminate;
                    return Evidence::True;
                };
            std::function<Evidence(const std::exception&)> exception_verifier =
                [mode](const std::exception&) {
                    if (mode == 10)
                        throw std::runtime_error("macro exception verification failed");
                    return mode == 9 ? Evidence::False : Evidence::True;
                };
            if (mode <= 4) {
                ARIADNE_TESTING_EXPECT_STATUS_CTX(c, operation(), selector,
                                                  (std::initializer_list<DomainStatus>{DomainStatus::Sat}), verifier);
            } else {
                ARIADNE_TESTING_EXPECT_ALTERNATIVE_CTX(c, operation(),
                                                       verifier, exception_verifier);
            }
        });
    }
    Capture macro_capture;
    const auto matrix_summary = macro_matrix.run(macro_capture);
    verify(matrix_summary.passed == 3 && matrix_summary.failed == 4 &&
               matrix_summary.inconclusive == 2 && matrix_summary.errors == 3 &&
               macro_capture.cases.size() == macro_outcomes.size() &&
               macro_capture.checks == 12,
           "shared macro specialization changed its outcome summary");
    if (macro_capture.cases.size() == macro_outcomes.size()) {
        for (std::size_t i = 0; i < macro_outcomes.size(); ++i) {
            const auto& result = macro_capture.cases[i];
            verify(result.outcome == macro_outcomes[i] &&
                       result.assertions.size() == 1 &&
                       result.assertions[0].outcome == macro_outcomes[i],
                   "incorrect shared macro outcome " + std::to_string(i));
        }
    }

    // Deferred macro operands are observed only after CHECK publication.
    // Exceptions must abort that case, preserve their original diagnostics,
    // and never evaluate the operands or predicates that come later.
    Suite macro_exceptions("macro deferred evaluation exceptions");
    Capture thrown_report;
    int manifest_reads = 0, observed_reads = 0;
    int replay_reads = 0, lhs_reads = 0, rhs_reads = 0;
    const std::vector<OracleRow> oracle_rows{
        {OracleRowId{"frozen", 0, 0}, true}
    };
    macro_exceptions.add("manifest accessor throws", [&](Context& c) {
        auto manifest = [&]() -> const std::vector<OracleRow>& {
            if (thrown_report.checks != 1)
                throw std::logic_error("manifest evaluated before CHECK");
            ++manifest_reads;
            throw std::runtime_error("manifest accessor failure");
        };
        auto observed = [&]() -> const std::vector<OracleRow>& {
            ++observed_reads;
            return oracle_rows;
        };
        ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(c, manifest(), observed());
    });
    macro_exceptions.add("observed accessor throws", [&](Context& c) {
        auto manifest = [&]() -> const std::vector<OracleRow>& {
            if (thrown_report.checks != 2)
                throw std::logic_error("manifest evaluated before CHECK");
            ++manifest_reads;
            return oracle_rows;
        };
        auto observed = [&]() -> const std::vector<OracleRow>& {
            ++observed_reads;
            throw std::runtime_error("observed accessor failure");
        };
        ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(c, manifest(), observed());
    });
    macro_exceptions.add("replay accessor throws", [&](Context& c) {
        auto replay = [&]() -> OracleReplayPlan {
            if (thrown_report.checks != 3)
                throw std::logic_error("replay evaluated before CHECK");
            ++replay_reads;
            throw std::runtime_error("replay accessor failure");
        };
        ARIADNE_TESTING_EXPECT_ORACLE_REPLAY_CTX(c, replay());
    });
    macro_exceptions.add("left operand throws", [&](Context& c) {
        auto left = [&]() -> int {
            if (thrown_report.checks != 4)
                throw std::logic_error("left evaluated before CHECK");
            ++lhs_reads;
            throw std::runtime_error("left operand failure");
        };
        auto right = [&] { ++rhs_reads; return 2; };
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(
            c, throws_from_binary_predicate, left(), right());
    });
    macro_exceptions.add("right operand throws", [&](Context& c) {
        auto left = [&] {
            if (thrown_report.checks != 5)
                throw std::logic_error("left evaluated before CHECK");
            ++lhs_reads;
            return 1;
        };
        auto right = [&]() -> int {
            ++rhs_reads;
            throw std::runtime_error("right operand failure");
        };
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(
            c, throws_from_binary_predicate, left(), right());
    });
    macro_exceptions.add("binary predicate throws", [&](Context& c) {
        auto left = [&] {
            if (thrown_report.checks != 6)
                throw std::logic_error("left evaluated before CHECK");
            ++lhs_reads;
            return 1;
        };
        auto right = [&] { ++rhs_reads; return 2; };
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(
            c, throws_from_binary_predicate, left(), right());
    });
    const auto thrown_summary = macro_exceptions.run(thrown_report);
    verify(thrown_summary.errors == 6 && thrown_summary.failed == 0 &&
               thrown_summary.passed == 0 && thrown_report.cases.size() == 6 &&
               thrown_report.checks == 6 &&
               manifest_reads == 2 && observed_reads == 1 && replay_reads == 1 &&
               lhs_reads == 3 && rhs_reads == 2,
           "macro exceptions changed deferred evaluation or CHECK ordering");
    if (thrown_report.cases.size() == 6) {
        const std::array<std::string_view, 6> details{
            "manifest accessor failure", "observed accessor failure",
            "replay accessor failure", "left operand failure",
            "right operand failure", "binary predicate failure"};
        for (std::size_t i = 0; i < details.size(); ++i) {
            const auto& result = thrown_report.cases[i];
            verify(result.outcome == Outcome::Error &&
                       result.assertions.size() == 1 &&
                       result.assertions[0].outcome == Outcome::Error &&
                       result.assertions[0].detail == details[i] &&
                       result.assertions[0].description == "Unhandled std::exception",
                   "macro exception or diagnostic lost for " + result.name);
        }
    }
    return ok ? 0 : 1;
}
