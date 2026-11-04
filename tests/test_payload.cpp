#include "ariadne-testing.hpp"
#include <functional>
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
using IntProjector = std::function<int(PayloadError const&)>;
// Route payload checks through one type-erased instantiation per exception
// type so a single template group observes every verification path.
inline void payload_erased(Context& c, std::function<void()> action,
                           IntProjector projector, std::function<int()> expected,
                           std::string_view desc) {
    c.expect_throws_payload<PayloadError>(
        std::move(action),
        [projector]() -> IntProjector const& { return projector; },
        [expected]() -> int { return expected(); },
        desc);
}

struct Fixture : ContextFixture {
    using ContextFixture::ContextFixture;
    void test_payload() {
        payload_erased(ctx(), [] { throw PayloadError(17,"failure"); },
            [](PayloadError const& e) { return e.code; }, [] { return 17; }, "fixture payload");
    }
};
int main() {
    Suite s("payload");
    int actions=0, projections=0, expected_calls=0;
    s.add("match", [&](Context& c) {
        payload_erased(c,
            [&] { ++actions; throw PayloadError(17,"failure"); },
            [&](PayloadError const& e) { ++projections; return e.code; },
            [&] { ++expected_calls; return 17; }, "payload match");
    });
    s.add("mismatch", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17,"failure"); },
            [](PayloadError const& e) { return e.code; }, [] { return 42; }, "payload mismatch");
    });
    s.add("indeterminate", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_THROWS_PAYLOAD_CTX(c, EvidenceError,
            throw EvidenceError(),
            [](EvidenceError const& e) { return e.value; },(EvidenceValue{false,true}));
    });
    s.add("wrong type", [&](Context& c) {
        payload_erased(c, [] { throw std::logic_error("wrong"); },
            [](PayloadError const& e) { return e.code; }, [] { return 17; }, "wrong type");
    });
    s.add("missing", [&](Context& c) {
        payload_erased(c, [] {},
            [](PayloadError const& e) { return e.code; }, [] { return 17; }, "missing");
    });
    s.add("projector error", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17,"failure"); },
            [](PayloadError const&) -> int { throw std::logic_error("projector"); },
            [] { return 17; }, "projector error");
    });
    s.add("expected error", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17,"failure"); },
            [](PayloadError const& e) { return e.code; },
            []() -> int { throw std::logic_error("expected"); }, "expected error");
    });
    s.add("signedness", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17,"failure"); },
            [](PayloadError const& e) { return e.code; }, [] { return 17; }, "signedness");
    });
    ARIADNE_TESTING_ADD_TEST_TO(s,Fixture,test_payload);
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

    // Distinguish an operation throwing a non-standard type (Failed)
    // from a payload projector throwing it after the right type (Error).
    Suite exception_boundaries("payload exception boundaries");
    exception_boundaries.add("operation throws nonstandard", [](Context& c) {
        payload_erased(c, [] { throw 42; },
            [](PayloadError const& e) { return e.code; }, [] { return 17; }, "nonstandard op");
    });
    exception_boundaries.add("projector throws nonstandard", [](Context& c) {
        payload_erased(c, [] { throw PayloadError(17, "payload"); },
            [](PayloadError const&) -> int { throw 42; }, [] { return 17; }, "nonstandard projector");
    });
    exception_boundaries.add("subsequent case still passes", [](Context& c) {
        c.prove(true, "later payload check still executes");
    });
    Capture boundary_capture;
    auto const boundary_summary = exception_boundaries.run(boundary_capture);
    if (boundary_summary.passed != 1 || boundary_summary.failed != 1 ||
        boundary_summary.errors != 1 || boundary_capture.cases.size() != 3)
        return 7;
    if (boundary_capture.cases[0].assertions[0].outcome != Outcome::Failed ||
        boundary_capture.cases[0].assertions[0].detail != "unexpected non-standard exception" ||
        boundary_capture.cases[1].assertions[0].outcome != Outcome::Error ||
        boundary_capture.cases[1].assertions[0].detail !=
            "payload verification threw a non-standard exception" ||
        boundary_capture.cases[2].outcome != Outcome::Passed)
        return 8;


    // Fatal prerequisites inside payload validation must abort the entire
    // case without being downgraded to a recoverable projector Error.
    Suite fatal_payload("fatal prerequisites in payload verification");
    int continued_after_fatal = 0;
    fatal_payload.add("projector prerequisite fails", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17, "payload"); },
            [&](PayloadError const& error) {
                ARIADNE_TESTING_REQUIRE_CTX(c, error.code == 42);
                return error.code;
            }, [] { return 17; }, "projector prerequisite");
        ++continued_after_fatal;
    });
    fatal_payload.add("expected-value prerequisite is indeterminate", [&](Context& c) {
        payload_erased(c, [] { throw PayloadError(17, "payload"); },
            [](PayloadError const& error) { return error.code; },
            [&]() -> int {
                ARIADNE_TESTING_REQUIRE_CTX(c, (EvidenceValue{false, true}));
                return 17;
            }, "expected prerequisite");
        ++continued_after_fatal;
    });
    fatal_payload.add("next case still executes", [&](Context& c) {
        ++continued_after_fatal;
        c.prove(true, "fatal requirements only terminate their own case");
    });
    Capture fatal_capture;
    auto const fatal_summary = fatal_payload.run(fatal_capture);
    if (fatal_summary.passed != 1 || fatal_summary.failed != 1 ||
        fatal_summary.inconclusive != 1 || fatal_summary.errors != 0 ||
        fatal_summary.skipped != 0 || fatal_capture.cases.size() != 3 ||
        continued_after_fatal != 1)
        return 9;
    if (fatal_capture.cases[0].outcome != Outcome::Failed ||
        fatal_capture.cases[0].assertions.size() != 1 ||
        fatal_capture.cases[0].assertions[0].outcome != Outcome::Failed ||
        fatal_capture.cases[0].assertions[0].description != "error.code == 42" ||
        fatal_capture.cases[1].outcome != Outcome::Inconclusive ||
        fatal_capture.cases[1].assertions.size() != 1 ||
        fatal_capture.cases[1].assertions[0].outcome != Outcome::Inconclusive ||
        fatal_capture.cases[2].outcome != Outcome::Passed ||
        fatal_capture.cases[2].assertions.size() != 1)
        return 10;

    // The same fixture registration must also take the NoThrow policy branch.
    Suite no_throw_suite("payload fixture NoThrow policy");
    add_fixture_test<Fixture, &Fixture::test_payload>(no_throw_suite, "payload",
                                                      FixtureCheck::NoThrow);
    Capture no_throw_capture;
    auto const no_throw_summary = no_throw_suite.run(no_throw_capture);
    if (no_throw_summary.passed != 1 || no_throw_summary.failed != 0 ||
        no_throw_summary.errors != 0 || no_throw_capture.cases.size() != 1 ||
        no_throw_capture.cases[0].assertions.back().description != "operation completes")
        return 11;
    return 0;
}
