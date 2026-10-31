#include "ariadne-testing.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

struct ThreeValued { bool definite; bool possible; };
bool definitely(ThreeValued value) { return value.definite; }
bool possibly(ThreeValued value) { return value.possible; }

struct PayloadError : std::runtime_error {
    int code;
    explicit PayloadError(int value) : std::runtime_error("payload"), code(value) {}
};

struct SimulatedFuture {
    std::future_status status;
    int calls = 0;
    std::future_status wait_for(std::chrono::milliseconds) {
        ++calls;
        return status;
    }
};

int main() {
    auto verify = [](bool value, std::string const& description) {
        if (!value) std::cerr << description << '\n';
        return value;
    };
    bool ok = true;

    // The same type-instantiation must preserve all evidence states, including
    // contradictory input, rather than silently deciding uncertainty.
    Suite evidence_suite("three-valued inputs");
    std::vector<ThreeValued> const inputs{{true, true}, {false, false},
                                          {false, true}, {true, false}};
    std::vector<Outcome> const evidence_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive, Outcome::Error};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        evidence_suite.add("evidence " + std::to_string(i), [&, i](Context& c) {
            c.prove(inputs[i], "logical evidence");
        });
    }
    Capture evidence_report;
    auto const evidence_summary = evidence_suite.run(evidence_report);
    ok &= verify(evidence_summary.passed == 1 && evidence_summary.failed == 1 &&
                 evidence_summary.inconclusive == 1 && evidence_summary.errors == 1 &&
                 evidence_report.cases.size() == inputs.size(),
                 "evidence: wrong summary");
    if (evidence_report.cases.size() == inputs.size()) {
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            ok &= verify(evidence_report.cases[i].outcome == evidence_outcomes[i],
                         "evidence: unexpected outcome " + std::to_string(i));
        }
        ok &= verify(evidence_report.cases[3].assertions.size() == 1 &&
                     evidence_report.cases[3].assertions[0].detail.find(
                         "Inconsistent logical evidence") != std::string::npos,
                     "evidence: contradictory input must be rejected");
    }

    // One instantiation of the typed-message checker for success, mismatch,
    // absent exception, wrong types and a fatal prerequisite.
    Suite typed_suite("typed exception boundaries");
    std::vector<Outcome> const typed_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Failed,
        Outcome::Failed, Outcome::Failed, Outcome::Failed};
    for (int mode = 0; mode < 6; ++mode) {
        typed_suite.add("typed " + std::to_string(mode), [mode](Context& c) {
            std::function<void()> operation = [mode, &c] {
                switch (mode) {
                    case 0: throw std::runtime_error("token present");
                    case 1: throw std::runtime_error("different message");
                    case 2: throw std::logic_error("wrong exception type");
                    case 3: throw 42;
                    case 4: return;
                    case 5: c.require([] { return false; }, "fatal prerequisite"); return;
                    default: throw std::logic_error("unexpected test mode");
                }
            };
            c.expect_throws_message<std::runtime_error>(operation, "token", "typed message");
        });
    }
    Capture typed_report;
    auto const typed_summary = typed_suite.run(typed_report);
    ok &= verify(typed_summary.passed == 1 && typed_summary.failed == 5 &&
                 typed_summary.errors == 0 && typed_report.cases.size() == 6,
                 "typed exception: wrong summary");
    if (typed_report.cases.size() == 6) {
        std::vector<std::string> const expected_details{
            "", "exception message mismatch", "unexpected exception type",
            "unexpected non-standard exception", "no exception thrown", ""};
        for (std::size_t i = 0; i < 6; ++i) {
            auto const& result = typed_report.cases[i];
            ok &= verify(result.outcome == typed_outcomes[i] &&
                         result.assertions.size() == 1,
                         "typed exception: wrong outcome " + std::to_string(i));
            if (result.assertions.size() == 1 && i != 5)
                ok &= verify(result.assertions[0].detail == expected_details[i],
                             "typed exception: wrong detail " + std::to_string(i));
            if (result.assertions.size() == 1 && i == 5)
                ok &= verify(result.assertions[0].description == "fatal prerequisite",
                             "typed exception: prerequisite must retain original identity");
        }
    }

    // Projector and expectation failures must be distinguished from a
    // payload mismatch and from a fatal prerequisite inside either step.
    Suite payload_suite("payload validation boundaries");
    std::vector<Outcome> const payload_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Error, Outcome::Error,
        Outcome::Error, Outcome::Error, Outcome::Failed, Outcome::Inconclusive};
    for (int mode = 0; mode < 8; ++mode) {
        payload_suite.add("payload " + std::to_string(mode), [mode](Context& c) {
            std::function<void()> action = [] { throw PayloadError(17); };
            auto projector_factory = [&]() {
                return std::function<int(PayloadError const&)>([&](PayloadError const& error) {
                    switch (mode) {
                        case 2: throw std::runtime_error("projector failed");
                        case 3: throw 42;
                        case 6: c.require([] { return false; }, "projector prerequisite"); break;
                        default: break;
                    }
                    return error.code;
                });
            };
            auto expected = [&]() -> int {
                switch (mode) {
                    case 1: return 21;
                    case 4: throw std::runtime_error("expected failed");
                    case 5: throw 42;
                    case 7: c.require([] { return ThreeValued{false, true}; },
                                      "expected prerequisite"); break;
                    default: break;
                }
                return 17;
            };
            c.expect_throws_payload<PayloadError>(action, projector_factory,
                                                  expected, "payload check");
        });
    }
    Capture payload_report;
    auto const payload_summary = payload_suite.run(payload_report);
    ok &= verify(payload_summary.passed == 1 && payload_summary.failed == 2 &&
                 payload_summary.inconclusive == 1 && payload_summary.errors == 4 &&
                 payload_report.cases.size() == 8,
                 "payload: wrong summary");
    if (payload_report.cases.size() == 8) {
        for (std::size_t i = 0; i < 8; ++i) {
            auto const& result = payload_report.cases[i];
            ok &= verify(result.outcome == payload_outcomes[i] &&
                         result.assertions.size() == 1,
                         "payload: wrong outcome " + std::to_string(i));
        }
        ok &= verify(payload_report.cases[6].assertions[0].description ==
                         "projector prerequisite" &&
                     payload_report.cases[7].assertions[0].description ==
                         "expected prerequisite",
                     "payload: fatal prerequisite was swallowed");
    }

    // A simulated Future avoids real sleeps while exercising all specified
    // wait_for classifications using one template instantiation.
    Suite future_suite("future readiness boundaries");
    std::vector<Outcome> const future_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Failed, Outcome::Error};
    for (int mode = 0; mode < 4; ++mode) {
        future_suite.add("future " + std::to_string(mode), [mode](Context& c) {
            SimulatedFuture future{
                mode == 0 ? std::future_status::ready :
                mode == 1 ? std::future_status::timeout : std::future_status::deferred};
            auto const timeout = mode == 3 ? std::chrono::milliseconds(-1) :
                                             std::chrono::milliseconds(0);
            c.expect_ready(future, timeout, "readiness");
            if (future.calls != (mode == 3 ? 0 : 1))
                throw std::runtime_error("invalid wait_for invocation count");
        });
    }
    Capture future_report;
    auto const future_summary = future_suite.run(future_report);
    ok &= verify(future_summary.passed == 1 && future_summary.failed == 2 &&
                 future_summary.errors == 1 && future_report.cases.size() == 4,
                 "future: wrong summary");
    if (future_report.cases.size() == 4) {
        for (std::size_t i = 0; i < 4; ++i)
            ok &= verify(future_report.cases[i].outcome == future_outcomes[i] &&
                         future_report.cases[i].assertions.size() == 1,
                         "future: wrong outcome " + std::to_string(i));
    }

#if !defined(_WIN32)
    // A circular symbolic link is a real filesystem error (ELOOP), not
    // an ordinary missing file. Keep it within a Context-owned directory.
    Suite fs_suite("filesystem error boundaries");
    fs_suite.add("symlink loop", [](Context& c) {
        auto const loop = c.work_directory() / "self-loop";
        std::filesystem::create_symlink(loop.filename(), loop);
        c.expect_file_exists(loop, "cyclic symbolic link");
    });
    Capture fs_report;
    auto const fs_summary = fs_suite.run(fs_report);
    ok &= verify(fs_summary.errors == 1 && fs_summary.failed == 0 &&
                 fs_report.cases.size() == 1, "filesystem: ELOOP should be Error");
    if (fs_report.cases.size() == 1)
        ok &= verify(fs_report.cases[0].assertions.size() == 1 &&
                     fs_report.cases[0].assertions[0].outcome == Outcome::Error &&
                     !fs_report.cases[0].assertions[0].detail.empty(),
                     "filesystem: missing underlying error detail");
#endif

    return ok ? 0 : 1;
}

