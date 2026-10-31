
#include "ariadne-testing.hpp"
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace Ariadne::Testing;

namespace Demo {
struct Value { int identity; int center; int radius; };
std::ostream& operator<<(std::ostream& os, Value const& v) {
    return os << "value(" << v.identity << ',' << v.center << ',' << v.radius << ')';
}
bool operator==(Value const& a, Value const& b) {
    return a.center == b.center && a.radius == b.radius;
}
bool same(Value const& a, Value const& b) {
    return a.identity == b.identity;
}
Evidence equal(Value const& a, Value const& b) {
    if (a.radius == 0 && b.radius == 0 && a.center == b.center) return Evidence::True;
    if (std::abs(a.center - b.center) > a.radius + b.radius) return Evidence::False;
    return Evidence::Indeterminate;
}
bool intersect(Value const& a, Value const& b) {
    return std::abs(a.center - b.center) <= a.radius + b.radius;
}
bool disjoint(Value const& a, Value const& b) { return !intersect(a,b); }
bool contains(Value const& a, Value const& b) {
    return std::abs(a.center - b.center) + b.radius <= a.radius;
}
Evidence refines(Value const& a, Value const& b) {
    if (contains(b,a)) return Evidence::True;
    if (intersect(a,b)) return Evidence::Indeterminate;
    return Evidence::False;
}
bool consistent(Value const& v) { return v.radius >= 0; }
} // namespace Demo

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    std::string last_started;
    void check_started(CaseResult const&, std::size_t, std::string_view name) override {
        last_started = name;
    }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

int main() {
    using Demo::Value;
    Suite suite("independent domain relation semantics");
    Capture capture;

    suite.add("exact equality need not imply identity", [](Context& c) {
        Value a{1,10,0}, b{2,10,0};
        ARIADNE_EXPECT_EQUAL_CTX(c,a,b);
    });
    suite.add("identity does not imply exact equality", [](Context& c) {
        Value a{7,10,0}, b{7,11,0};
        ARIADNE_EXPECT_SAME_CTX(c,a,b);
        ARIADNE_EXPECT_EQUAL_CTX(c,a,b);
    });
    suite.add("exact equality does not imply identity", [](Context& c) {
        Value a{1,10,0}, b{2,10,0};
        ARIADNE_EXPECT_EQUAL_CTX(c,a,b);
        ARIADNE_EXPECT_SAME_CTX(c,a,b);
    });
    suite.add("mathematical equality can be uncertain", [](Context& c) {
        Value a{1,10,1}, b{2,11,1};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,equal,a,b);
    });
    suite.add("mathematical inequality can be proven", [](Context& c) {
        Value a{1,10,0}, b{2,20,0};
        ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(c,equal,a,b);
    });
    suite.add("intersection and disjointness", [](Context& c) {
        Value a{1,10,1}, near{2,12,1}, far{3,20,1};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,intersect,a,near);
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,disjoint,a,far);
    });
    suite.add("directional containment", [](Context& c) {
        Value outer{1,10,5}, inner{2,11,1};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,contains,outer,inner);
        ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX(c,contains,inner,outer);
    });
    suite.add("proven refinement", [](Context& c) {
        Value inner{1,11,1}, outer{2,10,5};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,refines,inner,outer);
    });
    suite.add("uncertain refinement", [](Context& c) {
        Value a{1,10,2}, b{2,12,2};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,refines,a,b);
    });
    suite.add("consistency and its negation", [](Context& c) {
        Value good{1,10,0}, bad{2,10,-1};
        ARIADNE_EXPECT_UNARY_PREDICATE_CTX(c,consistent,good);
        ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX(c,consistent,bad);
    });
    suite.add("numerical tolerance is separate", [](Context& c) {
        ARIADNE_EXPECT_WITHIN_CTX(c,11,10,1);
    });
    suite.add("exact numerical equality rejects discrepancy", [](Context& c) {
        ARIADNE_EXPECT_EQUAL_CTX(c,11,10);
    });
    int left_calls=0, right_calls=0;
    suite.add("ADL relations evaluate once after CHECK", [&](Context& c) {
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,intersect,
            ([&] {
                if (capture.last_started.find("intersect(") == std::string::npos)
                    throw std::logic_error("CHECK event not published");
                ++left_calls;
                return Value{1,10,1};
            })(),
            ([&] {
                if (left_calls != 1) throw std::logic_error("operand order changed");
                ++right_calls;
                return Value{2,11,1};
            })());
    });
    suite.add("disjoint refinement must be rejected", [](Context& c) {
        Value a{1,10,1}, b{2,30,1};
        ARIADNE_EXPECT_BINARY_PREDICATE_CTX(c,refines,a,b);
    });

    auto const summary=suite.run(capture);
    bool ok=true;
    auto verify=[&](bool condition, std::string const& message) {
        if (!condition) { std::cerr << message << '\n'; ok=false; }
    };
    verify(summary.passed == 8 && summary.failed == 4 &&
           summary.inconclusive == 2 && summary.errors == 0 &&
           summary.skipped == 0 && capture.cases.size() == 14,
           "relation summary mismatch: passed=" + std::to_string(summary.passed) +
           " failed=" + std::to_string(summary.failed) +
           " inconclusive=" + std::to_string(summary.inconclusive) +
           " errors=" + std::to_string(summary.errors));
    verify(left_calls == 1 && right_calls == 1, "operands not evaluated once");
    if (capture.cases.size() == 14) {
        std::vector<Outcome> const outcomes{
            Outcome::Passed, Outcome::Failed, Outcome::Failed,
            Outcome::Inconclusive, Outcome::Passed, Outcome::Passed,
            Outcome::Passed, Outcome::Passed, Outcome::Inconclusive,
            Outcome::Passed, Outcome::Passed, Outcome::Failed,
            Outcome::Passed, Outcome::Failed
        };
        for(std::size_t i=0;i<outcomes.size();++i)
            verify(capture.cases[i].outcome == outcomes[i],
                   "unexpected relation outcome " + std::to_string(i));
        auto const& identity=capture.cases[1].assertions;
        verify(identity.size()==2 &&
               identity[0].outcome==Outcome::Passed &&
               identity[1].outcome==Outcome::Failed,
               "identity must not be conflated with equality");
        auto const& equality=capture.cases[2].assertions;
        verify(equality.size()==2 &&
               equality[0].outcome==Outcome::Passed &&
               equality[1].outcome==Outcome::Failed,
               "equality must not be conflated with identity");
        auto const& uncertain=capture.cases[3].assertions;
        verify(uncertain.size()==1 &&
               uncertain[0].outcome==Outcome::Inconclusive &&
               uncertain[0].description.find("equal(")!=std::string::npos,
               "mathematical equality lost evidence or relation name");
        auto const& tolerance=capture.cases[10].assertions;
        verify(tolerance.size()==1 &&
               tolerance[0].outcome==Outcome::Passed &&
               tolerance[0].detail.find("tolerance=")!=std::string::npos,
               "tolerance diagnostic changed");
        auto const& operands=capture.cases[12].assertions;
        verify(operands.size()==1 &&
               operands[0].outcome==Outcome::Passed &&
               operands[0].actual.find("value(")!=std::string::npos &&
               operands[0].expected.find("value(")!=std::string::npos,
               "rendered operands or relation check missing");
    }
    return ok?0:1;
}
