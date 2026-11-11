/***************************************************************************
 *            test_alternatives.cpp
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

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

struct SolverResult {
    bool witness;
    int progress;
};
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
    void check_started(const CaseResult &, std::size_t, std::string_view) override {
        ++checks;
    }
    void case_finished(const CaseResult &result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary &) override {}
};

int main() {
    int operations = 0;
    int result_verifications = 0;
    int exception_verifications = 0;
    Suite suite("result or exception with explicit postconditions");
    Capture capture;
    suite.add("result with witness", [&](Context &c) {
        c.expect_alternative([&] {
                ++operations;
                if (capture.checks != 1) throw std::logic_error("CHECK not announced");
                return SolverResult{true, 9}; },
                             [&](const SolverResult &result) {
                                 ++result_verifications;
                                 return result.witness && result.progress == 9;
                             },
                             [&](const std::exception &) {
                                 ++exception_verifications;
                                 return false;
                             },
                             "complete result");
    });
    suite.add("incomplete with valid partial progress", [&](Context &c) {
        c.expect_alternative([&]() -> SolverResult {
                ++operations;
                if (capture.checks != 2) throw std::logic_error("CHECK not announced");
                throw Incomplete(7); },
                             [&](const SolverResult &) { ++result_verifications; return false; },
                             [&](const std::exception &error) {
                                 ++exception_verifications;
                                 const auto *partial = dynamic_cast<const Incomplete *>(&error);
                                 return partial && partial->progress == 7;
                             },
                             "partial result accepted");
    });
    suite.add("budget alternative accepted", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Deadline(50); },
                             [](const SolverResult &) { return false; },
                             [](const std::exception &error) {
                                 const auto *deadline = dynamic_cast<const Deadline *>(&error);
                                 return deadline && deadline->budget >= 50;
                             },
                             "explicit budget exhaustion");
    });
    suite.add("returned result rejected", [](Context &c) {
        c.expect_alternative([] { return SolverResult{false, 9}; },
                             [](const SolverResult &r) { return r.witness; },
                             [](const std::exception &) { return false; }, "missing witness");
    });
    suite.add("wrong exception type rejected", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw std::logic_error("other"); },
                             [](const SolverResult &) { return true; },
                             [](const std::exception &e) {
                                 return dynamic_cast<const Incomplete *>(&e) != nullptr;
                             },
                             "unexpected solver failure");
    });
    suite.add("incorrect partial payload rejected", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(3); },
                             [](const SolverResult &) { return true; },
                             [](const std::exception &e) {
                                 const auto *partial = dynamic_cast<const Incomplete *>(&e);
                                 return partial && partial->progress == 7;
                             },
                             "wrong partial state");
    });
    suite.add("nonstandard operation exception rejected", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw 42; },
                             [](const SolverResult &) { return true; },
                             [](const std::exception &) { return true; }, "unexpected nonstandard throw");
    });
    suite.add("result verifier throws", [](Context &c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
                             [](const SolverResult &) -> bool {
                                 throw std::runtime_error("result verification failed");
                             },
                             [](const std::exception &) { return true; }, "result verifier error");
    });
    suite.add("exception verifier throws", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
                             [](const SolverResult &) { return true; },
                             [](const std::exception &) -> bool {
                                 throw std::runtime_error("exception verification failed");
                             },
                             "exception verifier error");
    });
    suite.add("result verifier throws nonstandard", [](Context &c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
                             [](const SolverResult &) -> bool { throw 17; },
                             [](const std::exception &) { return true; }, "nonstandard result verifier");
    });
    suite.add("exception verifier throws nonstandard", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
                             [](const SolverResult &) { return true; },
                             [](const std::exception &) -> bool { throw 18; },
                             "nonstandard exception verifier");
    });
    suite.add("uncertain result verification", [](Context &c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
                             [](const SolverResult &) { return Evidence::Indeterminate; },
                             [](const std::exception &) { return Evidence::True; }, "unproven result");
    });
    suite.add("uncertain partial state verification", [](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
                             [](const SolverResult &) { return Evidence::True; },
                             [](const std::exception &) { return Evidence::Indeterminate; },
                             "unproven partial state");
    });
    suite.add("void result accepted", [](Context &c) {
        c.expect_alternative([] {},
                             [](const std::monostate &) { return true; },
                             [](const std::exception &) { return false; }, "void success");
    });
    suite.add("void operation exception accepted", [](Context &c) {
        c.expect_alternative([] { throw Deadline(10); },
                             [](const std::monostate &) { return false; },
                             [](const std::exception &e) {
                                 const auto *deadline = dynamic_cast<const Deadline *>(&e);
                                 return deadline && deadline->budget == 10;
                             },
                             "void budget exception");
    });
    suite.add("later case still runs", [](Context &c) {
        c.prove(true, "subsequent case unaffected");
    });

    const auto summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool condition, const std::string &message) {
        if (!condition) {
            std::cerr << message << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 6 && summary.failed == 4 &&
               summary.inconclusive == 2 && summary.errors == 4 &&
               summary.skipped == 0 && capture.cases.size() == 16 &&
               capture.checks == 16,
           "alternative summary or CHECK count changed");
    verify(operations == 2 && result_verifications == 1 &&
               exception_verifications == 1,
           "operation or postcondition evaluated on the wrong branch");
    if (capture.cases.size() == 16) {
        const std::vector<Outcome> expected{
            Outcome::Passed, Outcome::Passed, Outcome::Passed,
            Outcome::Failed, Outcome::Failed, Outcome::Failed, Outcome::Failed,
            Outcome::Error, Outcome::Error, Outcome::Error, Outcome::Error,
            Outcome::Inconclusive, Outcome::Inconclusive,
            Outcome::Passed, Outcome::Passed, Outcome::Passed};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            verify(capture.cases[i].outcome == expected[i],
                   "unexpected outcome for alternative " + std::to_string(i));
            verify(capture.cases[i].assertions.size() == 1,
                   "incorrect assertion count for alternative " + std::to_string(i));
        }
        if (capture.cases[6].assertions.size() == 1)
            verify(capture.cases[6].assertions[0].detail ==
                       "non-standard operation exception",
                   "nonstandard operation classification");
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

    // A fatal precondition is not an acceptable exception alternative and
    // must not be swallowed by a verifier or converted into an Error.
    Suite aborts("fatal prerequisites in alternatives");
    int continued_after_fatal = 0;
    int unexpected_verifiers = 0;
    aborts.add("operation prerequisite aborts", [&](Context &c) {
        c.expect_alternative([&]() -> SolverResult {
                c.require([] { return false; }, "operation prerequisite");
                ++continued_after_fatal;
                return SolverResult{true, 9}; },
                             [&](const SolverResult &) { ++unexpected_verifiers; return true; },
                             [&](const std::exception &) { ++unexpected_verifiers; return true; },
                             "operation must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("exception verifier prerequisite aborts", [&](Context &c) {
        c.expect_alternative([]() -> SolverResult { throw Incomplete(7); },
                             [&](const SolverResult &) { ++unexpected_verifiers; return true; },
                             [&](const std::exception &) {
                                 c.require([] { return false; }, "exception verifier prerequisite");
                                 ++continued_after_fatal;
                                 return true;
                             },
                             "exception verification must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("result verifier prerequisite aborts", [&](Context &c) {
        c.expect_alternative([] { return SolverResult{true, 9}; },
                             [&](const SolverResult &) {
                                 c.require([] { return false; }, "result verifier prerequisite");
                                 ++continued_after_fatal;
                                 return true;
                             },
                             [&](const std::exception &) { ++unexpected_verifiers; return true; },
                             "result verification must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("subsequent alternative case still runs", [](Context &c) {
        c.prove(true, "suite resumes after fatal prerequisites");
    });
    Capture aborted_capture;
    const auto aborted_summary = aborts.run(aborted_capture);
    verify(aborted_summary.failed == 3 && aborted_summary.passed == 1 &&
               aborted_summary.errors == 0 && aborted_summary.inconclusive == 0 &&
               aborted_capture.cases.size() == 4 && aborted_capture.checks == 7 &&
               continued_after_fatal == 0 && unexpected_verifiers == 0,
           "fatal prerequisites must abort alternatives without suppressing later cases");
    if (aborted_capture.cases.size() == 4) {
        const std::string prerequisites[] = {
            "operation prerequisite", "exception verifier prerequisite",
            "result verifier prerequisite"};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto &fatal = aborted_capture.cases[i];
            verify(fatal.outcome == Outcome::Failed && fatal.assertions.size() == 1 &&
                       fatal.assertions[0].outcome == Outcome::Failed &&
                       fatal.assertions[0].description == prerequisites[i] &&
                       fatal.assertions[0].check_index == 2,
                   "fatal alternative must retain only its failed prerequisite at " +
                       std::to_string(i));
        }
        verify(aborted_capture.cases[3].outcome == Outcome::Passed &&
                   aborted_capture.cases[3].assertions.size() == 1,
               "fatal alternative must not affect the following case");
    }

    // Keep all result/exception outcomes in the same concrete template
    // instantiation. Unlike unrelated lambda callsites, the same operation
    // and verifier signatures encounter every valid branch at runtime.
    Suite alternatives_matrix("alternative outcome and verifier matrix");
    int matrix_operations = 0, matrix_result_verifiers = 0, matrix_exception_verifiers = 0;
    const std::vector<Outcome> alternative_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Error, Outcome::Error, Outcome::Failed,
        Outcome::Error, Outcome::Error};
    for (int mode = 0; mode < static_cast<int>(alternative_outcomes.size()); ++mode) {
        alternatives_matrix.add("alternative mode " + std::to_string(mode), [&, mode](Context &c) {
            std::function<SolverResult()> operation = [&, mode]() -> SolverResult {
                ++matrix_operations;
                if (mode >= 3 && mode <= 6)
                    throw Incomplete(7);
                if (mode == 8)
                    throw 42;
                if (mode == 10)
                    throw Incomplete(7);
                return SolverResult{true, 9};
            };
            std::function<Evidence(const SolverResult &)> result_verifier =
                [&, mode](const SolverResult &) -> Evidence {
                ++matrix_result_verifiers;
                if (mode == 7)
                    throw std::runtime_error("bad result proof");
                if (mode == 9)
                    throw 17;
                if (mode == 1)
                    return Evidence::False;
                if (mode == 2)
                    return Evidence::Indeterminate;
                return Evidence::True;
            };
            std::function<Evidence(const std::exception &)> exception_verifier =
                [&, mode](const std::exception &error) -> Evidence {
                ++matrix_exception_verifiers;
                if (dynamic_cast<const Incomplete *>(&error) == nullptr)
                    return Evidence::False;
                if (mode == 6)
                    throw std::runtime_error("bad exception proof");
                if (mode == 10)
                    throw 17;
                if (mode == 4)
                    return Evidence::False;
                if (mode == 5)
                    return Evidence::Indeterminate;
                return Evidence::True;
            };
            c.expect_alternative(operation, result_verifier, exception_verifier,
                                 "accepted result or exception");
        });
    }
    Capture alternatives_capture;
    const auto alternatives_summary = alternatives_matrix.run(alternatives_capture);
    verify(alternatives_summary.passed == 2 && alternatives_summary.failed == 3 &&
               alternatives_summary.inconclusive == 2 && alternatives_summary.errors == 4 &&
               alternatives_capture.checks == 11 &&
               alternatives_capture.cases.size() == alternative_outcomes.size() &&
               matrix_operations == 11 && matrix_result_verifiers == 5 &&
               matrix_exception_verifiers == 5,
           "alternative runtime matrix summary or callback isolation changed");
    if (alternatives_capture.cases.size() == alternative_outcomes.size()) {
        for (std::size_t i = 0; i < alternative_outcomes.size(); ++i) {
            const auto &result = alternatives_capture.cases[i];
            verify(result.outcome == alternative_outcomes[i] &&
                       result.assertions.size() == 1 &&
                       result.assertions[0].outcome == alternative_outcomes[i],
                   "alternative runtime matrix classification changed at mode " +
                       std::to_string(i));
        }
        verify(alternatives_capture.cases[1].assertions[0].detail ==
                       "result alternative rejected" &&
                   alternatives_capture.cases[2].assertions[0].detail ==
                       "result postcondition indeterminate" &&
                   alternatives_capture.cases[4].assertions[0].detail ==
                       "exception alternative rejected" &&
                   alternatives_capture.cases[5].assertions[0].detail ==
                       "exception postcondition indeterminate" &&
                   alternatives_capture.cases[6].assertions[0].detail ==
                       "exception verifier threw: bad exception proof" &&
                   alternatives_capture.cases[7].assertions[0].detail ==
                       "result verifier threw: bad result proof" &&
                   alternatives_capture.cases[8].assertions[0].detail ==
                       "non-standard operation exception" &&
                   alternatives_capture.cases[9].assertions[0].detail ==
                       "result verifier threw a non-standard exception" &&
                   alternatives_capture.cases[10].assertions[0].detail ==
                       "exception verifier threw a non-standard exception",
               "alternative runtime matrix lost detailed failure reasons");
    }
    return ok ? 0 : 1;
}
