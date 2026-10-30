#include "ariadne-testing.hpp"
#include <stdexcept>
#include <string>
#include <vector>

using namespace Ariadne::Testing;

struct PayloadError : std::runtime_error {
    int code;
    PayloadError(int code_, std::string msg) : std::runtime_error(msg), code(code_) {}
};
struct EvidenceValue { bool d; bool p; };
bool definitely(EvidenceValue x) { return x.d; }
bool possibly(EvidenceValue x) { return x.p; }
EvidenceValue operator==(EvidenceValue, EvidenceValue) { return {false,true}; }
struct EvidenceError : std::runtime_error {
    EvidenceValue value;
    EvidenceError() : std::runtime_error("evidence"), value{false,true} {}
};
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(CaseResult const& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, Summary const&) override {}
};
struct Fixture : ContextFixture {
    using ContextFixture::ContextFixture;
    void test_payload() {
        ARIADNE_EXPECT_THROWS_PAYLOAD(PayloadError,
            throw PayloadError(17,"failure"),
            [](PayloadError const& e) { return e.code; },17);
    }
};
int main() {
    Suite s("payload");
    int actions=0, projections=0, expected_calls=0;
    s.add("match", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            (++actions, throw PayloadError(17,"failure")),
            [&](PayloadError const& e) { ++projections; return e.code; },
            (++expected_calls, 17));
    });
    s.add("mismatch", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            throw PayloadError(17,"failure"),
            [](PayloadError const& e) { return e.code; },42);
    });
    s.add("indeterminate", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, EvidenceError,
            throw EvidenceError(),
            [](EvidenceError const& e) { return e.value; },(EvidenceValue{false,true}));
    });
    s.add("wrong type", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            throw std::logic_error("wrong"),
            [](PayloadError const& e) { return e.code; },17);
    });
    s.add("missing", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            (void)0,
            [](PayloadError const& e) { return e.code; },17);
    });
    s.add("projector error", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            throw PayloadError(17,"failure"),
            [](PayloadError const&) -> int { throw std::logic_error("projector"); },17);
    });
    s.add("expected error", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            throw PayloadError(17,"failure"),
            [](PayloadError const& e) { return e.code; },
            ([]() -> int { throw std::logic_error("expected"); })());
    });
    s.add("signedness", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_PAYLOAD_CTX(c, PayloadError,
            throw PayloadError(17,"failure"),
            [](PayloadError const& e) { return e.code; },17u);
    });
    ARIADNE_ADD_TEST_TO(s,Fixture,test_payload);
    Capture r; auto summary=s.run(r);
    if (summary.passed!=3 || summary.failed!=3 || summary.inconclusive!=1 ||
        summary.errors!=2 || r.cases.size()!=9) return 1;
    if (actions!=1 || projections!=1 || expected_calls!=1) return 2;
    if (r.cases[0].assertions[0].actual!="17" ||
        r.cases[1].assertions[0].expected!="42") return 3;
    if (r.cases[2].assertions[0].outcome!=Outcome::Inconclusive) return 4;
    if (r.cases[3].assertions[0].detail!="unexpected exception type" ||
        r.cases[4].assertions[0].detail!="no exception thrown") return 5;
    if (r.cases[5].assertions[0].outcome!=Outcome::Error ||
        r.cases[6].assertions[0].outcome!=Outcome::Error) return 6;
    return 0;
}
