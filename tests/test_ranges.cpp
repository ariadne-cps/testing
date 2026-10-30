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
    if(summary.passed!=3 || summary.failed!=4 || summary.inconclusive!=1 ||
       summary.errors!=0 || evaluations!=1 || report.cases.size()!=8) return 1;
    if(report.cases[1].assertions[0].detail.find("first_mismatch_index=1")==std::string::npos ||
       report.cases[1].assertions[0].actual.find("value=7")==std::string::npos) return 2;
    if(report.cases[2].assertions[0].detail.find("actual_count=3, expected_count=2")==std::string::npos) return 3;
    if(report.cases[3].assertions[0].detail.find("actual_count=1, expected_count=3")==std::string::npos) return 4;
    if(report.cases[4].assertions[0].outcome!=Outcome::Inconclusive) return 5;
    if(report.cases[5].assertions[0].detail.find("path=node/1")==std::string::npos) return 6;
    return 0;
}
