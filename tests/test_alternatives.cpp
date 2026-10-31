#include "ariadne-testing.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace Ariadne::Testing;

struct SolverResult { bool witness; int progress; };
struct Incomplete : std::runtime_error {
    int progress;
    explicit Incomplete(int n) : std::runtime_error("incomplete"), progress(n) {}
};
struct Deadline : std::runtime_error {
    int budget;
    explicit Deadline(int n) : std::runtime_error("deadline"), budget(n) {}
};
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
    int operations = 0;
    int result_verifications = 0;
    int exception_verifications = 0;
    Suite suite("result or exception with explicit postconditions");
    Capture capture;
    suite.add("result with witness", [&](Context& c) {
        c.expect_alternative([&] {
                ++operations;
                if (capture.checks != 1) throw std::logic_error("CHECK not announced");
                return SolverResult{true, 9};
            },
            [&](SolverResult const& result) {
                ++result_verifications;
                return result.witness && result.progress == 9;
            },
            [&](std::exception const&) {
                ++exception_verifications;
                return false;
            }, "complete result");
    });
    suite.add("incomplete with valid partial progress", [&](Context& c) {
        c.expect_alternative([&]() -> SolverResult {
                ++operations;
                if (capture.checks != 2) throw std::logic_error("CHECK not announced");
                throw Incomplete(7);
            },
            [&](SolverResult const&) { ++result_verifications; return false; },
            [&](std::exception const& error) {
                ++exception_verifications;
                auto const* partial = dynamic_cast<Incomplete const*>(&error);
                return partial && partial->progress == 7;
            }, "partial result accepted");
    });
    suite.add("budget alternative accepted", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw Deadline(50); },
            [](SolverResult const&) { return false; },
            [](std::exception const& error) {
                auto const* deadline = dynamic_cast<Deadline const*>(&error);
                return deadline && deadline->budget >= 50;
            }, "explicit budget exhaustion");
    });
    suite.add("returned result rejected", [](Context& c) {
        c.expect_alternative([] { return SolverResult{false, 9}; },
            [](SolverResult const& r) { return r.witness; },
            [](std::exception const&) { return false; }, "missing witness");
    });
    suite.add("wrong exception type rejected", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw std::logic_error("other"); },
            [](SolverResult const&) { return true; },
            [](std::exception const& e) {
                return dynamic_cast<Incomplete const*>(&e) != nullptr;
            }, "unexpected solver failure");
    });
    suite.add("incorrect partial payload rejected", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(3); },
            [](SolverResult const&) { return true; },
            [](std::exception const& e) {
                auto const* partial = dynamic_cast<Incomplete const*>(&e);
                return partial && partial->progress == 7;
            }, "wrong partial state");
    });
    suite.add("nonstandard operation exception rejected", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw 42; },
            [](SolverResult const&) { return true; },
            [](std::exception const&) { return true; }, "unexpected nonstandard throw");
    });
    suite.add("result verifier throws", [](Context& c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
            [](SolverResult const&) -> bool {
                throw std::runtime_error("result verification failed");
            },
            [](std::exception const&) { return true; }, "result verifier error");
    });
    suite.add("exception verifier throws", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return true; },
            [](std::exception const&) -> bool {
                throw std::runtime_error("exception verification failed");
            }, "exception verifier error");
    });
    suite.add("result verifier throws nonstandard", [](Context& c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
            [](SolverResult const&) -> bool { throw 17; },
            [](std::exception const&) { return true; }, "nonstandard result verifier");
    });
    suite.add("exception verifier throws nonstandard", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return true; },
            [](std::exception const&) -> bool { throw 18; },
            "nonstandard exception verifier");
    });
    suite.add("uncertain result verification", [](Context& c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
            [](SolverResult const&) { return Evidence::Indeterminate; },
            [](std::exception const&) { return Evidence::True; }, "unproven result");
    });
    suite.add("uncertain partial state verification", [](Context& c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return Evidence::True; },
            [](std::exception const&) { return Evidence::Indeterminate; },
            "unproven partial state");
    });
    suite.add("void result accepted", [](Context& c) {
        c.expect_alternative([] {},
            [](std::monostate const&) { return true; },
            [](std::exception const&) { return false; }, "void success");
    });
    suite.add("void operation exception accepted", [](Context& c) {
        c.expect_alternative([] { throw Deadline(10); },
            [](std::monostate const&) { return false; },
            [](std::exception const& e) {
                auto const* deadline = dynamic_cast<Deadline const*>(&e);
                return deadline && deadline->budget == 10;
            }, "void budget exception");
    });
    suite.add("later case still runs", [](Context& c) {
        c.prove(true, "subsequent case unaffected");
    });

    auto const summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool condition, std::string const& message) {
        if (!condition) {
            std::cerr << message << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 6 && summary.failed == 4 &&
           summary.inconclusive == 2 && summary.errors == 4 &&
           summary.skipped == 0 && capture.cases.size() == 16 &&
           capture.checks == 16, "alternative summary or CHECK count changed");
    verify(operations == 2 && result_verifications == 1 &&
           exception_verifications == 1,
           "operation or postcondition evaluated on the wrong branch");
    if (capture.cases.size() == 16) {
        std::vector<Outcome> const expected{
            Outcome::Passed, Outcome::Passed, Outcome::Passed,
            Outcome::Failed, Outcome::Failed, Outcome::Failed, Outcome::Failed,
            Outcome::Error, Outcome::Error, Outcome::Error, Outcome::Error,
            Outcome::Inconclusive, Outcome::Inconclusive,
            Outcome::Passed, Outcome::Passed, Outcome::Passed
        };
        for (std::size_t i = 0; i < expected.size(); ++i) {
            verify(capture.cases[i].outcome == expected[i],
                   "unexpected outcome for alternative " + std::to_string(i));
            verify(capture.cases[i].assertions.size() == 1,
                   "incorrect assertion count for alternative " + std::to_string(i));
        }
        if (capture.cases[6].assertions.size() == 1)
            verify(capture.cases[6].assertions[0].detail ==
                   "non-standard operation exception", "nonstandard operation classification");
        if (capture.cases[8].assertions.size() == 1)
            verify(capture.cases[8].assertions[0].detail ==
                   "exception verifier threw: exception verification failed",
                   "exception verifier error must not become an allowed alternative");
        if (capture.cases[10].assertions.size() == 1)
            verify(capture.cases[10].assertions[0].detail ==
                   "exception verifier threw a non-standard exception",
                   "nonstandard verifier must be Error");
        if (capture.cases[13].assertions.size() == 1)
            verify(capture.cases[13].assertions[0].actual == "void",
                   "void result must remain observable");
    }
    return ok ? 0 : 1;
}
