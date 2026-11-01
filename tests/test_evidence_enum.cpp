#include "ariadne-testing.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> results;
    void case_finished(CaseResult const& result) override { results.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

struct Item { int value; };
Evidence operator==(Item const& left, Item const& right) {
    return left.value == right.value ? Evidence::True : Evidence::Indeterminate;
}
Evidence operator<(Item const& left, Item const& right) {
    return left.value < right.value ? Evidence::True : Evidence::Indeterminate;
}

int main() {
    Suite suite("native Evidence propagation");
    int continued_after_requirement = 0;
    suite.add("true evidence", [](Context& c) { c.prove(Evidence::True, "proved"); });
    suite.add("false evidence", [](Context& c) { c.prove(Evidence::False, "refuted"); });
    suite.add("unknown evidence", [](Context& c) {
        c.prove(Evidence::Indeterminate, "undetermined");
    });
    suite.add("deferred check", [](Context& c) {
        c.check([] { return Evidence::Indeterminate; }, "direct evidence check");
    });
    suite.add("indeterminate expectation", [](Context& c) {
        c.expect_evidence([] { return Evidence::Indeterminate; },
                          EvidenceExpectation::Indeterminate, "expected uncertainty");
    });
    suite.add("not possibly expectation", [](Context& c) {
        c.expect_evidence([] { return Evidence::Indeterminate; },
                          EvidenceExpectation::NotPossibly, "cannot exclude truth");
    });
    suite.add("unary predicate", [](Context& c) {
        c.check_unary([](int) { return Evidence::Indeterminate; },
                      [] { return 3; }, "unary unknown", true);
    });
    suite.add("binary predicate", [](Context& c) {
        c.check_binary([](int, int) { return Evidence::Indeterminate; },
                       [] { return 2; }, [] { return 5; }, "binary unknown", true);
    });
    suite.add("domain equality", [](Context& c) {
        c.expect_equal(Item{2}, Item{5}, "domain equality is unknown");
    });
    suite.add("domain ordering", [](Context& c) {
        c.check_compare<Comparison::Less>([] { return Item{5}; },
            [] { return Item{2}; }, "domain ordering is unknown");
    });
    suite.add("fatal unknown requirement", [&](Context& c) {
        c.require([] { return Evidence::Indeterminate; }, "unknown prerequisite");
        ++continued_after_requirement;
    });
    suite.add("fatal false requirement", [&](Context& c) {
        c.require([] { return Evidence::False; }, "false prerequisite");
        ++continued_after_requirement;
    });
    suite.add("later case still runs", [&](Context& c) {
        ++continued_after_requirement;
        c.prove(true, "unaffected subsequent case");
    });
    std::vector<Outcome> const outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Passed, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Inconclusive, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Inconclusive, Outcome::Failed,
        Outcome::Passed
    };
    Capture capture;
    auto const summary = suite.run(capture);
    if (summary.passed != 3 || summary.failed != 2 || summary.inconclusive != 8 ||
        summary.errors != 0 || summary.skipped != 0 ||
        capture.results.size() != outcomes.size() || continued_after_requirement != 1) {
        std::cerr << "Unexpected summary " << summary.passed << ' ' << summary.failed
                  << ' ' << summary.inconclusive << ' ' << summary.errors
                  << "; after fatal=" << continued_after_requirement << '\n';
        return 1;
    }
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        auto const& result = capture.results[i];
        if (result.outcome != outcomes[i] || result.assertions.size() != 1 ||
            result.assertions[0].outcome != outcomes[i]) {
            std::cerr << "Mismatch case " << i << ": " << result.name
                      << ", got " << static_cast<int>(result.outcome)
                      << " instead of " << static_cast<int>(outcomes[i]) << '\n';
            return 2;
        }
    }
    // The internal negation helper explicitly rejects a malformed Evidence
    // representation instead of converting it to a boolean success.
    int const invalid_representation = 99;
    bool rejected_invalid_evidence = false;
    try {
        (void) Detail::negate(static_cast<Evidence>(invalid_representation));
    } catch (std::logic_error const& error) {
        rejected_invalid_evidence = std::string_view(error.what()) ==
                                    "Invalid mathematical evidence";
    }
    if (!rejected_invalid_evidence) {
        std::cerr << "Malformed Evidence was not rejected by negation\n";
        return 3;
    }
    return 0;
}
