#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;
enum class Status { SAT,
                    UNSAT,
                    UNKNOWN };
std::ostream& operator<<(std::ostream& os, Status s) {
    switch (s) {
        case Status::SAT:
            return os << "SAT";
        case Status::UNSAT:
            return os << "UNSAT";
        case Status::UNKNOWN:
            return os << "UNKNOWN";
        default:
            return os << "invalid";
    }
}
struct SolverResult {
    Status status;
    bool witness;
    int reason;
};
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    int checks = 0;
    void check_started(const CaseResult&, std::size_t, std::string_view) override { ++checks; }
    void case_finished(const CaseResult& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, const Summary&) override {}
};
int main() {
    int operations = 0, selectors = 0, validators = 0, forbidden = 0;
    Suite s("typed domain statuses");
    s.add("SAT witness", [&](Context& c) {
        c.expect_status([&] { ++operations; return SolverResult{Status::SAT,true,0}; },
                        [&](const SolverResult& r) { ++selectors; return r.status; },
                        {Status::SAT, Status::UNKNOWN},
                        [&](const SolverResult& r) { ++validators; return r.witness; }, "SAT witness");
    });
    s.add("UNSAT", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNSAT, false, 0}; },
                        [](const SolverResult& r) { return r.status; }, {Status::UNSAT}, "UNSAT");
    });
    s.add("expected UNKNOWN", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNKNOWN, false, 42}; },
                        [](const SolverResult& r) { return r.status; }, {Status::UNKNOWN},
                        [](const SolverResult& r) { return r.reason == 42 && !r.witness; },
                        "resource exhausted");
    });
    s.add("unexpected status", [&](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNSAT, false, 0}; },
                        [](const SolverResult& r) { return r.status; }, {Status::SAT},
                        [&](const SolverResult&) { ++forbidden; return true; }, "not permitted");
    });
    s.add("missing witness", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT, false, 0}; },
                        [](const SolverResult& r) { return r.status; }, {Status::SAT},
                        [](const SolverResult& r) { return r.witness; }, "invalid SAT");
    });
    s.add("wrong UNKNOWN reason", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNKNOWN, false, 1}; },
                        [](const SolverResult& r) { return r.status; }, {Status::UNKNOWN},
                        [](const SolverResult& r) { return r.reason == 42; }, "wrong reason");
    });
    s.add("indeterminate witness proof", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT, true, 0}; },
                        [](const SolverResult& r) { return r.status; }, {Status::SAT},
                        [](const SolverResult&) { return Evidence::Indeterminate; }, "uncertain");
    });
    s.add("verifier exception", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT, true, 0}; },
                        [](const SolverResult& r) { return r.status; }, {Status::SAT},
                        [](const SolverResult&) -> bool { throw std::logic_error("verifier"); }, "error");
    });
    s.add("empty allowed set", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT, true, 0}; },
                        [](const SolverResult& r) { return r.status; },
                        std::initializer_list<Status>{}, "nothing permitted");
    });
    Capture reporter;
    const auto summary = s.run(reporter);
    if (summary.passed != 3 || summary.failed != 4 ||
        summary.inconclusive != 1 || summary.errors != 1 ||
        reporter.cases.size() != 9 || reporter.checks != 9 ||
        operations != 1 || selectors != 1 || validators != 1 || forbidden != 0)
        return 1;
    std::vector<Outcome> expected{Outcome::Passed, Outcome::Passed, Outcome::Passed,
                                  Outcome::Failed, Outcome::Failed, Outcome::Failed, Outcome::Inconclusive,
                                  Outcome::Error, Outcome::Failed};
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (reporter.cases[i].outcome != expected[i])
            return 2;
    if (reporter.cases[2].assertions.size() != 1 ||
        reporter.cases[2].assertions[0].outcome != Outcome::Passed ||
        reporter.cases[2].assertions[0].actual != "UNKNOWN" ||
        reporter.cases[3].assertions[0].detail != "domain status not permitted")
        return 3;

    // Reuse one Operation/Selector/Verifier type-instantiation across all
    // permitted-status configurations and postcondition outcomes.
    Suite status_matrix("typed status lifecycle matrix");
    int operation_calls = 0, selector_calls = 0, verifier_calls = 0;
    const std::vector<Outcome> status_expected{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Failed, Outcome::Failed, Outcome::Passed,
        Outcome::Passed, Outcome::Error, Outcome::Error,
        Outcome::Error, Outcome::Error, Outcome::Passed};
    for (int mode = 0; mode < static_cast<int>(status_expected.size()); ++mode) {
        status_matrix.add("status mode " + std::to_string(mode), [&, mode](Context& c) {
            std::function<SolverResult()> operation = [&, mode] {
                ++operation_calls;
                if (mode == 7)
                    throw std::runtime_error("operation unavailable");
                return SolverResult{
                    mode == 3 ? Status::UNSAT : mode == 11 ? Status::UNKNOWN
                                                           : Status::SAT,
                    true, 42};
            };
            std::function<Status(const SolverResult&)> selector = [&, mode](const SolverResult& r) {
                ++selector_calls;
                if (mode == 8)
                    throw std::runtime_error("status unavailable");
                return r.status;
            };
            std::function<Evidence(const SolverResult&)> verifier = [&, mode](const SolverResult&) {
                ++verifier_calls;
                if (mode == 9)
                    throw std::runtime_error("postcondition unavailable");
                if (mode == 10)
                    throw 17;
                if (mode == 1)
                    return Evidence::False;
                if (mode == 2)
                    return Evidence::Indeterminate;
                return Evidence::True;
            };
            if (mode == 4) {
                c.expect_status(operation, selector, std::initializer_list<Status>{}, verifier, "status with empty allowance");
            } else if (mode == 5) {
                c.expect_status(operation, selector, {Status::UNSAT, Status::SAT},
                                verifier, "second allowed status");
            } else if (mode == 6) {
                c.expect_status(operation, selector, {Status::SAT, Status::SAT},
                                verifier, "duplicate allowed status");
            } else if (mode == 11) {
                c.expect_status(operation, selector, {Status::SAT, Status::UNKNOWN},
                                verifier, "unknown allowed status");
            } else {
                c.expect_status(operation, selector, {Status::SAT},
                                verifier, "status outcome matrix");
            }
        });
    }
    Capture status_capture;
    const auto status_summary = status_matrix.run(status_capture);
    if (status_summary.passed != 4 || status_summary.failed != 3 ||
        status_summary.inconclusive != 1 || status_summary.errors != 4 ||
        status_capture.cases.size() != status_expected.size() ||
        status_capture.checks != static_cast<int>(status_expected.size()) ||
        operation_calls != 12 || selector_calls != 11 || verifier_calls != 8) {
        std::cerr << "Typed status matrix summary, CHECK order, or call isolation changed\n";
        return 4;
    }
    for (std::size_t i = 0; i < status_expected.size(); ++i) {
        const auto& outcome = status_capture.cases[i];
        if (outcome.outcome != status_expected[i] || outcome.assertions.size() != 1 ||
            outcome.assertions[0].outcome != status_expected[i]) {
            std::cerr << "Typed status matrix outcome changed at mode " << i << '\n';
            return 5;
        }
    }
    if (status_capture.cases[1].assertions[0].detail != "status postcondition failed" ||
        status_capture.cases[2].assertions[0].detail != "status postcondition indeterminate" ||
        status_capture.cases[3].assertions[0].detail != "domain status not permitted" ||
        status_capture.cases[4].assertions[0].expected != "one of []" ||
        status_capture.cases[5].assertions[0].expected != "one of [UNSAT, SAT]" ||
        status_capture.cases[6].assertions[0].expected != "one of [SAT, SAT]" ||
        status_capture.cases[11].assertions[0].actual != "UNKNOWN" ||
        status_capture.cases[9].assertions[0].detail != "postcondition unavailable" ||
        status_capture.cases[10].assertions[0].description != "Unhandled non-standard exception") {
        std::cerr << "Typed status matrix lost specific diagnostics or ordering\n";
        return 6;
    }
    return 0;
}
