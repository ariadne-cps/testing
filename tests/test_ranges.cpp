#include "ariadne-testing.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <iterator>
#include <iostream>

using namespace Ariadne::Testing;

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(CaseResult const& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, Summary const&) override {}
};
struct Uncertain { int value; };
struct Truth { bool definite; bool possible; };
bool definitely(Truth t) { return t.definite; }
bool possibly(Truth t) { return t.possible; }
Truth same_value(Uncertain a, Uncertain b) {
    return a.value==b.value ? Truth{true,true} : Truth{false,true};
}
struct Fixture : ContextFixture {
    using ContextFixture::ContextFixture;
    void test_short() {
        ARIADNE_TESTING_EXPECT_RANGES((std::vector<int>{1,2}), (std::vector<int>{1,2}));
    }
};
int main() {
    Suite suite("ranges");
    int evaluations=0;
    suite.add("equal", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c, (++evaluations, std::vector<int>{1,2,3}),
                                (std::vector<unsigned>{1u,2u,3u}));
    });
    suite.add("mismatch", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c, (std::vector<int>{1,7,3}),
                                (std::vector<int>{1,2,3}));
    });
    suite.add("different lengths", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c, (std::vector<int>{1,2,3}),
                                (std::vector<int>{1,2}));
    });
    suite.add("missing elements", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c, (std::vector<int>{1}),
                                (std::vector<int>{1,2,3}));
    });
    suite.add("custom predicate indeterminate", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_BY_CTX(c, (std::vector<Uncertain>{{1},{3}}),
            (std::vector<Uncertain>{{1},{2}}), same_value);
    });
    suite.add("custom path", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_AT_CTX(c, (std::vector<int>{1,3}),
            (std::vector<int>{1,2}),
            [](int a, int b) { return a==b; },
            [](std::size_t i) { return std::string("node/")+std::to_string(i); });
    });
    suite.add("non-copyable comparator", [](Context& c) {
        struct Comparator {
            Comparator() = default;
            Comparator(Comparator const&) = delete;
            bool operator()(int a, int b) const { return a == b; }
        } comparator;
        ARIADNE_TESTING_EXPECT_RANGES_BY_CTX(c, (std::vector<int>{1,2}),
            (std::vector<int>{1,2}), comparator);
    });
    suite.add("named path function", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_AT_CTX(c, (std::vector<Uncertain>{{1},{2}}),
            (std::vector<Uncertain>{{1},{2}}), same_value,
            [](std::size_t i) { return std::to_string(i); });
    });
    suite.add("single pass", [](Context& c) {
        std::istringstream lhs("1 2 3");
        std::istringstream rhs("1 2 3");
        auto a=std::ranges::subrange(std::istream_iterator<int>(lhs),std::istream_iterator<int>());
        auto b=std::ranges::subrange(std::istream_iterator<int>(rhs),std::istream_iterator<int>());
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c,a,b);
    });
    ARIADNE_TESTING_ADD_TEST_TO(suite,Fixture,test_short);
    Capture report;
    auto const summary=suite.run(report);
    if(summary.passed!=5 || summary.failed!=4 || summary.inconclusive!=1 ||
       summary.errors!=0 || evaluations!=1 || report.cases.size()!=10) return 1;
    if(report.cases[1].assertions[0].detail.find("first_mismatch_index=1")==std::string::npos ||
       report.cases[1].assertions[0].actual.find("value=7")==std::string::npos) return 2;
    if(report.cases[2].assertions[0].detail.find("actual_count=3, expected_count=2")==std::string::npos) return 3;
    if(report.cases[3].assertions[0].detail.find("actual_count=1, expected_count=3")==std::string::npos) return 4;
    if(report.cases[4].assertions[0].outcome!=Outcome::Inconclusive) return 5;
    if(report.cases[5].assertions[0].detail.find("path=node/1")==std::string::npos) return 6;

    // An indeterminate early element cannot hide a later definite mismatch.
    // Both locations must be retained for a useful diagnostic.
    Suite range_boundaries("range diagnostic boundaries");
    range_boundaries.add("unknown before definite mismatch", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_BY_CTX(c, (std::vector<int>{0,7,5}),
            (std::vector<int>{0,2,5}),
            [](int actual, int expected) -> Evidence {
                if (actual == 0) return Evidence::Indeterminate;
                return actual == expected ? Evidence::True : Evidence::False;
            });
    });
    range_boundaries.add("long values bounded in diagnostics", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c,
            (std::vector<std::string>{std::string(120, 'a')}),
            (std::vector<std::string>{std::string(120, 'b')}));
    });
    range_boundaries.add("two empty ranges compare equal", [](Context& c) {
        ARIADNE_TESTING_EXPECT_RANGES_CTX(c, (std::vector<int>{}),
                                  (std::vector<int>{}));
    });
    Capture boundary_capture;
    auto const boundary_summary = range_boundaries.run(boundary_capture);
    if (boundary_summary.passed != 1 || boundary_summary.failed != 2 ||
        boundary_summary.errors != 0 || boundary_capture.cases.size() != 3)
        return 7;
    auto const& mixed_assertion = boundary_capture.cases[0].assertions[0];
    if (mixed_assertion.outcome != Outcome::Failed ||
        mixed_assertion.detail.find("first_mismatch_index=1") == std::string::npos ||
        mixed_assertion.detail.find("first_indeterminate_index=0") == std::string::npos ||
        mixed_assertion.actual.find("value=7") == std::string::npos ||
        mixed_assertion.expected.find("value=2") == std::string::npos)
        return 8;
    auto const& long_assertion = boundary_capture.cases[1].assertions[0];
    if (long_assertion.outcome != Outcome::Failed ||
        long_assertion.actual.find(std::string(93, 'a') + "...") == std::string::npos ||
        long_assertion.expected.find(std::string(93, 'b') + "...") == std::string::npos ||
        long_assertion.actual.find(std::string(94, 'a')) != std::string::npos ||
        long_assertion.expected.find(std::string(94, 'b')) != std::string::npos)
        return 9;
    auto const& empty_assertion = boundary_capture.cases[2].assertions[0];
    if (empty_assertion.outcome != Outcome::Passed ||
        empty_assertion.detail.find("actual_count=0, expected_count=0") == std::string::npos ||
        empty_assertion.detail.find("first_mismatch_index") != std::string::npos)
        return 10;

    // Exercise a single range-checker instantiation with different valid
    // inputs. This is an oracle for first mismatch, uncertainty, truncation,
    // and unequal length, rather than a collection of unrelated templates.
    struct RangeScenario {
        std::string name;
        std::vector<std::string> actual;
        std::vector<std::string> expected;
        Outcome outcome;
        int first_index;
        int first_unknown_if_failed;
    };
    std::string const long_a(120, 'a');
    std::string const long_b(120, 'b');
    std::vector<RangeScenario> const scenarios{
        {"empty", {}, {}, Outcome::Passed, -1, -1},
        {"equal", {"1", "2"}, {"1", "2"}, Outcome::Passed, -1, -1},
        {"one mismatch", {"1", "7", "3"}, {"1", "2", "3"}, Outcome::Failed, 1, -1},
        {"multiple mismatches", {"7", "9", "11"}, {"2", "3", "4"}, Outcome::Failed, 0, -1},
        {"uncertain pair", {"1", "?", "3"}, {"1", "2", "3"}, Outcome::Inconclusive, 1, -1},
        {"multiple uncertain pairs", {"?", "?"}, {"1", "2"}, Outcome::Inconclusive, 0, -1},
        {"uncertainty before failure", {"?", "7"}, {"1", "2"}, Outcome::Failed, 1, 0},
        {"failure before uncertainty", {"7", "?"}, {"1", "2"}, Outcome::Failed, 0, 1},
        {"extra actual element", {"1", "2", "3"}, {"1", "2"}, Outcome::Failed, 2, -1},
        {"extra expected element", {"1", "2"}, {"1", "2", "3"}, Outcome::Failed, 2, -1},
        {"mismatch and extra actual", {"7", "9", "3"}, {"1", "2"}, Outcome::Failed, 0, -1},
        {"mismatch and extra expected", {"7"}, {"1", "2", "3"}, Outcome::Failed, 0, -1},
        {"long mismatched strings", {long_a}, {long_b}, Outcome::Failed, 0, -1},
        {"empty actual", {}, {"1"}, Outcome::Failed, 0, -1},
        {"empty expected", {"1"}, {}, Outcome::Failed, 0, -1},
        {"long equal strings", {long_a}, {long_a}, Outcome::Passed, -1, -1}
    };
    auto compare_scenario = [](Context& c, RangeScenario const& scenario) {
        c.check_ranges(
            [&]() -> std::vector<std::string> const& { return scenario.actual; },
            [&]() -> std::vector<std::string> const& { return scenario.expected; },
            [] {
                return [](std::string const& actual, std::string const& expected) -> Evidence {
                    if (actual == "?" || expected == "?") return Evidence::Indeterminate;
                    return actual == expected ? Evidence::True : Evidence::False;
                };
            },
            [] {
                return [](std::size_t index) {
                    return std::string("element/") + std::to_string(index);
                };
            },
            "structured range equality");
    };
    Suite matrix("range outcome and diagnostics matrix");
    std::size_t passed = 0, failed = 0, inconclusive = 0;
    for (auto const& scenario : scenarios) {
        auto const* entry = &scenario;
        matrix.add(entry->name, [&, entry](Context& c) {
            compare_scenario(c, *entry);
        });
        if (entry->outcome == Outcome::Passed) ++passed;
        else if (entry->outcome == Outcome::Failed) ++failed;
        else if (entry->outcome == Outcome::Inconclusive) ++inconclusive;
    }
    Capture matrix_report;
    auto const matrix_summary = matrix.run(matrix_report);
    if (matrix_summary.passed != passed || matrix_summary.failed != failed ||
        matrix_summary.inconclusive != inconclusive || matrix_summary.errors != 0 ||
        matrix_report.cases.size() != scenarios.size()) {
        std::cerr << "range matrix: incorrect summary or case count\n";
        return 11;
    }
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        auto const& scenario = scenarios[i];
        auto const& result = matrix_report.cases[i];
        if (result.name != scenario.name || result.outcome != scenario.outcome ||
            result.assertions.size() != 1 ||
            result.assertions[0].outcome != scenario.outcome) {
            std::cerr << "range matrix: incorrect outcome for " << scenario.name << '\n';
            return 12;
        }
        auto const& assertion = result.assertions[0];
        auto const counts = "actual_count=" + std::to_string(scenario.actual.size()) +
                            ", expected_count=" + std::to_string(scenario.expected.size());
        if (assertion.detail.find(counts) == std::string::npos) {
            std::cerr << "range matrix: incorrect element counts for " << scenario.name << '\n';
            return 13;
        }
        auto const first = assertion.detail.find("first_mismatch_index=");
        if (scenario.first_index == -1) {
            if (first != std::string::npos) {
                std::cerr << "range matrix: unexpected mismatch for " << scenario.name << '\n';
                return 14;
            }
        } else {
            auto const marker = "first_mismatch_index=" +
                                std::to_string(scenario.first_index);
            auto const path = "path=element/" + std::to_string(scenario.first_index);
            if (first == std::string::npos ||
                assertion.detail.find(marker) == std::string::npos ||
                assertion.detail.find(path) == std::string::npos) {
                std::cerr << "range matrix: incorrect mismatch location for "
                          << scenario.name << '\n';
                return 15;
            }
        }
        auto const unknown = assertion.detail.find("first_indeterminate_index=");
        if (scenario.first_unknown_if_failed == -1) {
            if (unknown != std::string::npos) {
                std::cerr << "range matrix: unexpected secondary uncertainty for "
                          << scenario.name << '\n';
                return 16;
            }
        } else if (assertion.detail.find("first_indeterminate_index=" +
                    std::to_string(scenario.first_unknown_if_failed)) == std::string::npos) {
            std::cerr << "range matrix: lost uncertainty context for " << scenario.name << '\n';
            return 17;
        }
    }
    auto const& truncated = matrix_report.cases[12].assertions[0];
    if (truncated.actual.find(std::string(93, 'a') + "...") == std::string::npos ||
        truncated.expected.find(std::string(93, 'b') + "...") == std::string::npos ||
        truncated.actual.find(std::string(94, 'a')) != std::string::npos ||
        truncated.expected.find(std::string(94, 'b')) != std::string::npos) {
        std::cerr << "range matrix: long mismatched elements were not truncated\n";
        return 18;
    }

    return 0;
}
