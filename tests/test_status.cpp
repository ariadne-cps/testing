#include "ariadne-testing.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace Ariadne::Testing;
enum class Status { SAT, UNSAT, UNKNOWN };
std::ostream& operator<<(std::ostream& os, Status s) {
    switch (s) {
        case Status::SAT: return os << "SAT";
        case Status::UNSAT: return os << "UNSAT";
        case Status::UNKNOWN: return os << "UNKNOWN";
        default: return os << "invalid";
    }
}
struct SolverResult { Status status; bool witness; int reason; };
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    int checks = 0;
    void check_started(CaseResult const&, std::size_t, std::string_view) override { ++checks; }
    void case_finished(CaseResult const& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, Summary const&) override {}
};
int main() {
    int operations=0, selectors=0, validators=0, forbidden=0;
    Suite s("typed domain statuses");
    s.add("SAT witness", [&](Context& c) {
        c.expect_status([&] { ++operations; return SolverResult{Status::SAT,true,0}; },
            [&](SolverResult const& r) { ++selectors; return r.status; },
            {Status::SAT,Status::UNKNOWN},
            [&](SolverResult const& r) { ++validators; return r.witness; }, "SAT witness");
    });
    s.add("UNSAT", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNSAT,false,0}; },
            [](SolverResult const& r) { return r.status; }, {Status::UNSAT}, "UNSAT");
    });
    s.add("expected UNKNOWN", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNKNOWN,false,42}; },
            [](SolverResult const& r) { return r.status; }, {Status::UNKNOWN},
            [](SolverResult const& r) { return r.reason==42 && !r.witness; },
            "resource exhausted");
    });
    s.add("unexpected status", [&](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNSAT,false,0}; },
            [](SolverResult const& r) { return r.status; }, {Status::SAT},
            [&](SolverResult const&) { ++forbidden; return true; }, "not permitted");
    });
    s.add("missing witness", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT,false,0}; },
            [](SolverResult const& r) { return r.status; }, {Status::SAT},
            [](SolverResult const& r) { return r.witness; }, "invalid SAT");
    });
    s.add("wrong UNKNOWN reason", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::UNKNOWN,false,1}; },
            [](SolverResult const& r) { return r.status; }, {Status::UNKNOWN},
            [](SolverResult const& r) { return r.reason==42; }, "wrong reason");
    });
    s.add("indeterminate witness proof", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT,true,0}; },
            [](SolverResult const& r) { return r.status; }, {Status::SAT},
            [](SolverResult const&) { return Evidence::Indeterminate; }, "uncertain");
    });
    s.add("verifier exception", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT,true,0}; },
            [](SolverResult const& r) { return r.status; }, {Status::SAT},
            [](SolverResult const&) -> bool { throw std::logic_error("verifier"); }, "error");
    });
    s.add("empty allowed set", [](Context& c) {
        c.expect_status([] { return SolverResult{Status::SAT,true,0}; },
            [](SolverResult const& r) { return r.status; },
            std::initializer_list<Status>{}, "nothing permitted");
    });
    Capture reporter;
    auto const summary=s.run(reporter);
    if (summary.passed!=3 || summary.failed!=4 ||
        summary.inconclusive!=1 || summary.errors!=1 ||
        reporter.cases.size()!=9 || reporter.checks!=9 ||
        operations!=1 || selectors!=1 || validators!=1 || forbidden!=0) return 1;
    std::vector<Outcome> expected{Outcome::Passed,Outcome::Passed,Outcome::Passed,
        Outcome::Failed,Outcome::Failed,Outcome::Failed,Outcome::Inconclusive,
        Outcome::Error,Outcome::Failed};
    for (std::size_t i=0; i<expected.size(); ++i)
        if (reporter.cases[i].outcome!=expected[i]) return 2;
    if (reporter.cases[2].assertions.size()!=1 ||
        reporter.cases[2].assertions[0].outcome!=Outcome::Passed ||
        reporter.cases[2].assertions[0].actual!="UNKNOWN" ||
        reporter.cases[3].assertions[0].detail!="domain status not permitted")
        return 3;
    return 0;
}
