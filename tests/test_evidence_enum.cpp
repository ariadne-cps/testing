/***************************************************************************
 *            test_evidence_enum.cpp
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

#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> results;
    void case_finished(const CaseResult& result) override { results.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

struct Item {
    int value;
};
Evidence operator==(const Item& left, const Item& right) {
    return left.value == right.value ? Evidence::True : Evidence::Indeterminate;
}
Evidence operator<(const Item& left, const Item& right) {
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
    const std::vector<Outcome> outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Passed, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Inconclusive, Outcome::Inconclusive,
        Outcome::Inconclusive, Outcome::Inconclusive, Outcome::Failed,
        Outcome::Passed};
    Capture capture;
    const auto summary = suite.run(capture);
    if (summary.passed != 3 || summary.failed != 2 || summary.inconclusive != 8 ||
        summary.errors != 0 || summary.skipped != 0 ||
        capture.results.size() != outcomes.size() || continued_after_requirement != 1) {
        std::cerr << "Unexpected summary " << summary.passed << ' ' << summary.failed
                  << ' ' << summary.inconclusive << ' ' << summary.errors
                  << "; after fatal=" << continued_after_requirement << '\n';
        return 1;
    }
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        const auto& result = capture.results[i];
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
    const int invalid_representation = 99;
    bool rejected_invalid_evidence = false;
    try {
        (void)Detail::negate(static_cast<Evidence>(invalid_representation));
    } catch (const std::logic_error& error) {
        rejected_invalid_evidence = std::string_view(error.what()) ==
                                    "Invalid mathematical evidence";
    }
    if (!rejected_invalid_evidence) {
        std::cerr << "Malformed Evidence was not rejected by negation\n";
        return 3;
    }

    // All meaningful expectation/evidence combinations share one callable
    // specialization. Check classification and diagnostic names for each pair.
    const std::array<Evidence, 3> evidence_values{
        Evidence::True, Evidence::False, Evidence::Indeterminate};
    const std::array<EvidenceExpectation, 4> expectation_values{
        EvidenceExpectation::DefinitelyTrue, EvidenceExpectation::PossiblyTrue,
        EvidenceExpectation::NotPossibly, EvidenceExpectation::Indeterminate};
    const std::array<std::string_view, 3> observed_names{
        "true", "false", "indeterminate"};
    const std::array<std::string_view, 4> expectation_names{
        "definitely true", "possibly true", "not possibly true", "indeterminate"};
    const std::array<std::array<Outcome, 3>, 4> expectation_outcomes{
        {{Outcome::Passed, Outcome::Failed, Outcome::Inconclusive},
         {Outcome::Passed, Outcome::Failed, Outcome::Passed},
         {Outcome::Failed, Outcome::Passed, Outcome::Inconclusive},
         {Outcome::Failed, Outcome::Failed, Outcome::Passed}}
    };
    Suite expectation_matrix("public evidence expectation combinations");
    for (std::size_t i = 0; i < expectation_values.size(); ++i) {
        for (std::size_t j = 0; j < evidence_values.size(); ++j) {
            const auto actual = evidence_values[j];
            const auto expected = expectation_values[i];
            expectation_matrix.add("expectation " + std::to_string(i) + "/" +
                                       std::to_string(j),
                                   [actual, expected](Context& c) {
                                       const std::function<Evidence()> predicate = [actual] { return actual; };
                                       c.expect_evidence(predicate, expected, "mathematical evidence expectation");
                                   });
        }
    }
    Capture expectation_capture;
    const auto expectation_summary = expectation_matrix.run(expectation_capture);
    if (expectation_summary.passed != 5 || expectation_summary.failed != 5 ||
        expectation_summary.inconclusive != 2 || expectation_summary.errors != 0 ||
        expectation_capture.results.size() != 12) {
        std::cerr << "Incorrect evidence expectation matrix summary\n";
        return 4;
    }
    for (std::size_t i = 0; i < expectation_values.size(); ++i) {
        for (std::size_t j = 0; j < evidence_values.size(); ++j) {
            const auto& result = expectation_capture.results[i * 3 + j];
            if (result.outcome != expectation_outcomes[i][j] ||
                result.assertions.size() != 1 ||
                result.assertions[0].outcome != expectation_outcomes[i][j] ||
                result.assertions[0].actual != observed_names[j] ||
                result.assertions[0].expected != expectation_names[i]) {
                std::cerr << "Evidence expectation mismatch at " << i << '/' << j << '\n';
                return 5;
            }
        }
    }
    // Check the documented defensive fallbacks for enum values supplied
    // through the public Context API (for example, corrupted deserialization).
    Suite malformed_enums("malformed mathematical evidence contracts");
    malformed_enums.add("unexpected evidence", [](Context& c) {
        const std::function<Evidence()> predicate = [] {
            return static_cast<Evidence>(99);
        };
        c.expect_evidence(predicate, EvidenceExpectation::DefinitelyTrue,
                          "malformed evidence must not become proof");
    });
    malformed_enums.add("unexpected expectation", [](Context& c) {
        const std::function<Evidence()> predicate = [] { return Evidence::True; };
        c.expect_evidence(predicate, static_cast<EvidenceExpectation>(99),
                          "unsupported evidence expectation must be error");
    });
    Capture malformed_report;
    const auto malformed_summary = malformed_enums.run(malformed_report);
    if (malformed_summary.errors != 2 || malformed_summary.failed != 0 ||
        malformed_report.results.size() != 2) {
        std::cerr << "Malformed evidence values lost their Error classification\n";
        return 6;
    }
    if (malformed_report.results[0].assertions.size() != 1 ||
        malformed_report.results[1].assertions.size() != 1 ||
        malformed_report.results[0].assertions[0].actual != "invalid" ||
        malformed_report.results[0].assertions[0].expected != "definitely true" ||
        malformed_report.results[1].assertions[0].actual != "true" ||
        malformed_report.results[1].assertions[0].expected != "invalid") {
        std::cerr << "Malformed evidence names or expectations changed\n";
        return 7;
    }
    return 0;
}
