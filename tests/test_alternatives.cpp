#include "ariadne-testing.hpp"

#include <functional>
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

// Route alternative checks through one type-erased instantiation so a single
// template group observes both outcomes at runtime.
template<class Vr>
auto erase_result(Vr&& vr) {
    return [vr = std::forward<Vr>(vr)]<class R>(R const& r) mutable -> Evidence {
        return Detail::evidence(vr(r));
    };
}
template<class Ve>
auto erase_exception(Ve&& ve) {
    return [ve = std::forward<Ve>(ve)](std::exception const& e) mutable -> Evidence {
        return Detail::evidence(ve(e));
    };
}
template<class Op, class Vr, class Ve>
void expect_alt(Context& c, Op&& op, Vr&& vr, Ve&& ve, std::string_view desc) {
    c.expect_alternative(std::function<SolverResult()>(std::forward<Op>(op)),
                         std::function<Evidence(SolverResult const&)>(erase_result(std::forward<Vr>(vr))),
                         std::function<Evidence(std::exception const&)>(erase_exception(std::forward<Ve>(ve))),
                         desc);
}
template<class Op, class Vr, class Ve>
void expect_alt_void(Context& c, Op&& op, Vr&& vr, Ve&& ve, std::string_view desc) {
    c.expect_alternative(std::function<void()>(std::forward<Op>(op)),
                         std::function<Evidence(std::monostate const&)>(erase_result(std::forward<Vr>(vr))),
                         std::function<Evidence(std::exception const&)>(erase_exception(std::forward<Ve>(ve))),
                         desc);
}

int main() {
    int operations = 0;
    int result_verifications = 0;
    int exception_verifications = 0;
    Suite suite("result or exception with explicit postconditions");
    Capture capture;
    suite.add("result with witness", [&](Context& c) {
        expect_alt(c, [&] {
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
        expect_alt(c, [&]() -> SolverResult {
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
        expect_alt(c, []() -> SolverResult { throw Deadline(50); },
            [](SolverResult const&) { return false; },
            [](std::exception const& error) {
                auto const* deadline = dynamic_cast<Deadline const*>(&error);
                return deadline && deadline->budget >= 50;
            }, "explicit budget exhaustion");
    });
    suite.add("returned result rejected", [](Context& c) {
        expect_alt(c, [] { return SolverResult{false, 9}; },
            [](SolverResult const& r) { return r.witness; },
            [](std::exception const&) { return false; }, "missing witness");
    });
    suite.add("wrong exception type rejected", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw std::logic_error("other"); },
            [](SolverResult const&) { return true; },
            [](std::exception const& e) {
                return dynamic_cast<Incomplete const*>(&e) != nullptr;
            }, "unexpected solver failure");
    });
    suite.add("incorrect partial payload rejected", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw Incomplete(3); },
            [](SolverResult const&) { return true; },
            [](std::exception const& e) {
                auto const* partial = dynamic_cast<Incomplete const*>(&e);
                return partial && partial->progress == 7;
            }, "wrong partial state");
    });
    suite.add("nonstandard operation exception rejected", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw 42; },
            [](SolverResult const&) { return true; },
            [](std::exception const&) { return true; }, "unexpected nonstandard throw");
    });
    suite.add("result verifier throws", [](Context& c) {
        expect_alt(c, [] { return SolverResult{true, 9}; },
            [](SolverResult const&) -> bool {
                throw std::runtime_error("result verification failed");
            },
            [](std::exception const&) { return true; }, "result verifier error");
    });
    suite.add("exception verifier throws", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return true; },
            [](std::exception const&) -> bool {
                throw std::runtime_error("exception verification failed");
            }, "exception verifier error");
    });
    suite.add("result verifier throws nonstandard", [](Context& c) {
        expect_alt(c, [] { return SolverResult{true, 9}; },
            [](SolverResult const&) -> bool { throw 17; },
            [](std::exception const&) { return true; }, "nonstandard result verifier");
    });
    suite.add("exception verifier throws nonstandard", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return true; },
            [](std::exception const&) -> bool { throw 18; },
            "nonstandard exception verifier");
    });
    suite.add("uncertain result verification", [](Context& c) {
        expect_alt(c, [] { return SolverResult{true, 9}; },
            [](SolverResult const&) { return Evidence::Indeterminate; },
            [](std::exception const&) { return Evidence::True; }, "unproven result");
    });
    suite.add("uncertain partial state verification", [](Context& c) {
        expect_alt(c, []() -> SolverResult { throw Incomplete(7); },
            [](SolverResult const&) { return Evidence::True; },
            [](std::exception const&) { return Evidence::Indeterminate; },
            "unproven partial state");
    });
    suite.add("void result accepted", [](Context& c) {
        expect_alt_void(c, [] {},
            [](std::monostate const&) { return true; },
            [](std::exception const&) { return false; }, "void success");
    });
    suite.add("void operation exception accepted", [](Context& c) {
        expect_alt_void(c, [] { throw Deadline(10); },
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

    // A fatal precondition is not an acceptable exception alternative and
    // must not be swallowed by a verifier or converted into an Error.
    Suite aborts("fatal prerequisites in alternatives");
    int continued_after_fatal = 0;
    int unexpected_verifiers = 0;
    aborts.add("operation prerequisite aborts", [&](Context& c) {
        expect_alt(c, [&]() -> SolverResult {
                c.require([] { return false; }, "operation prerequisite");
                ++continued_after_fatal;
                return SolverResult{true, 9};
            },
            [&](SolverResult const&) { ++unexpected_verifiers; return true; },
            [&](std::exception const&) { ++unexpected_verifiers; return true; },
            "operation must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("exception verifier prerequisite aborts", [&](Context& c) {
        expect_alt(c, []() -> SolverResult { throw Incomplete(7); },
            [&](SolverResult const&) { ++unexpected_verifiers; return true; },
            [&](std::exception const&) {
                c.require([] { return false; }, "exception verifier prerequisite");
                ++continued_after_fatal;
                return true;
            }, "exception verification must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("result verifier prerequisite aborts", [&](Context& c) {
        expect_alt(c, [] { return SolverResult{true, 9}; },
            [&](SolverResult const&) {
                c.require([] { return false; }, "result verifier prerequisite");
                ++continued_after_fatal;
                return true;
            },
            [&](std::exception const&) { ++unexpected_verifiers; return true; },
            "result verification must stop on fatal prerequisite");
        ++continued_after_fatal;
    });
    aborts.add("subsequent alternative case still runs", [](Context& c) {
        c.prove(true, "suite resumes after fatal prerequisites");
    });
    Capture aborted_capture;
    auto const aborted_summary = aborts.run(aborted_capture);
    verify(aborted_summary.failed == 3 && aborted_summary.passed == 1 &&
           aborted_summary.errors == 0 && aborted_summary.inconclusive == 0 &&
           aborted_capture.cases.size() == 4 && aborted_capture.checks == 7 &&
           continued_after_fatal == 0 && unexpected_verifiers == 0,
           "fatal prerequisites must abort alternatives without suppressing later cases");
    if (aborted_capture.cases.size() == 4) {
        std::string const prerequisites[] = {
            "operation prerequisite", "exception verifier prerequisite",
            "result verifier prerequisite"
        };
        for (std::size_t i = 0; i < 3; ++i) {
            auto const& fatal = aborted_capture.cases[i];
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
    std::vector<Outcome> const alternative_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Error, Outcome::Error, Outcome::Failed,
        Outcome::Error, Outcome::Error
    };
    for (int mode = 0; mode < static_cast<int>(alternative_outcomes.size()); ++mode) {
        alternatives_matrix.add("alternative mode " + std::to_string(mode), [&, mode](Context& c) {
            std::function<SolverResult()> operation = [&, mode]() -> SolverResult {
                ++matrix_operations;
                if (mode >= 3 && mode <= 6) throw Incomplete(7);
                if (mode == 8) throw 42;
                if (mode == 10) throw Incomplete(7);
                return SolverResult{true, 9};
            };
            std::function<Evidence(SolverResult const&)> result_verifier =
                [&, mode](SolverResult const&) -> Evidence {
                    ++matrix_result_verifiers;
                    if (mode == 7) throw std::runtime_error("bad result proof");
                    if (mode == 9) throw 17;
                    if (mode == 1) return Evidence::False;
                    if (mode == 2) return Evidence::Indeterminate;
                    return Evidence::True;
                };
            std::function<Evidence(std::exception const&)> exception_verifier =
                [&, mode](std::exception const& error) -> Evidence {
                    ++matrix_exception_verifiers;
                    if (dynamic_cast<Incomplete const*>(&error) == nullptr)
                        return Evidence::False;
                    if (mode == 6) throw std::runtime_error("bad exception proof");
                    if (mode == 10) throw 17;
                    if (mode == 4) return Evidence::False;
                    if (mode == 5) return Evidence::Indeterminate;
                    return Evidence::True;
                };
            expect_alt(c, operation, result_verifier, exception_verifier,
                                 "accepted result or exception");
        });
    }
    Capture alternatives_capture;
    auto const alternatives_summary = alternatives_matrix.run(alternatives_capture);
    verify(alternatives_summary.passed == 2 && alternatives_summary.failed == 3 &&
           alternatives_summary.inconclusive == 2 && alternatives_summary.errors == 4 &&
           alternatives_capture.checks == 11 &&
           alternatives_capture.cases.size() == alternative_outcomes.size() &&
           matrix_operations == 11 && matrix_result_verifiers == 5 &&
           matrix_exception_verifiers == 5,
           "alternative runtime matrix summary or callback isolation changed");
    if (alternatives_capture.cases.size() == alternative_outcomes.size()) {
        for (std::size_t i = 0; i < alternative_outcomes.size(); ++i) {
            auto const& result = alternatives_capture.cases[i];
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
