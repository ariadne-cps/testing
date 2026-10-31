#include "ariadne-testing.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <iterator>

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
        ARIADNE_EXPECT_RANGES((std::vector<int>{1,2}), (std::vector<int>{1,2}));
    }
};
int main() {
    Suite suite("ranges");
    int evaluations=0;
    suite.add("equal", [&](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c, (++evaluations, std::vector<int>{1,2,3}),
                                (std::vector<unsigned>{1u,2u,3u}));
    });
    suite.add("mismatch", [](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c, (std::vector<int>{1,7,3}),
                                (std::vector<int>{1,2,3}));
    });
    suite.add("different lengths", [](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c, (std::vector<int>{1,2,3}),
                                (std::vector<int>{1,2}));
    });
    suite.add("missing elements", [](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c, (std::vector<int>{1}),
                                (std::vector<int>{1,2,3}));
    });
    suite.add("custom predicate indeterminate", [](Context& c) {
        ARIADNE_EXPECT_RANGES_BY_CTX(c, (std::vector<Uncertain>{{1},{3}}),
            (std::vector<Uncertain>{{1},{2}}), same_value);
    });
    suite.add("custom path", [](Context& c) {
        ARIADNE_EXPECT_RANGES_AT_CTX(c, (std::vector<int>{1,3}),
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
        ARIADNE_EXPECT_RANGES_BY_CTX(c, (std::vector<int>{1,2}),
            (std::vector<int>{1,2}), comparator);
    });
    suite.add("named path function", [](Context& c) {
        ARIADNE_EXPECT_RANGES_AT_CTX(c, (std::vector<Uncertain>{{1},{2}}),
            (std::vector<Uncertain>{{1},{2}}), same_value,
            [](std::size_t i) { return std::to_string(i); });
    });
    suite.add("single pass", [](Context& c) {
        std::istringstream lhs("1 2 3");
        std::istringstream rhs("1 2 3");
        auto a=std::ranges::subrange(std::istream_iterator<int>(lhs),std::istream_iterator<int>());
        auto b=std::ranges::subrange(std::istream_iterator<int>(rhs),std::istream_iterator<int>());
        ARIADNE_EXPECT_RANGES_CTX(c,a,b);
    });
    ARIADNE_ADD_TEST_TO(suite,Fixture,test_short);
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
        ARIADNE_EXPECT_RANGES_BY_CTX(c, (std::vector<int>{0,7,5}),
            (std::vector<int>{0,2,5}),
            [](int actual, int expected) -> Evidence {
                if (actual == 0) return Evidence::Indeterminate;
                return actual == expected ? Evidence::True : Evidence::False;
            });
    });
    range_boundaries.add("long values bounded in diagnostics", [](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c,
            (std::vector<std::string>{std::string(120, 'a')}),
            (std::vector<std::string>{std::string(120, 'b')}));
    });
    range_boundaries.add("two empty ranges compare equal", [](Context& c) {
        ARIADNE_EXPECT_RANGES_CTX(c, (std::vector<int>{}),
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

    return 0;
}
