#include "ariadne-testing.hpp"
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using namespace Ariadne::Testing;

struct Captured : Reporter {
    std::vector<CaseResult> results;
    void case_finished(CaseResult const& result) override { results.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

struct ThreeValued {
    bool definite;
    bool possible;
};
bool definitely(ThreeValued v) { return v.definite; }
bool possibly(ThreeValued v) { return v.possible; }

struct Fixture : ContextFixture {
    using ContextFixture::ContextFixture;
    void test_fixture() {
        ARIADNE_REQUIRE(true);
        ARIADNE_EXPECT_THROWS_MESSAGE(std::runtime_error,
            throw std::runtime_error("expected text"), "text");
    }
};

int main() {
    Suite suite("requirements");
    int evaluated = 0;
    int continued = 0;
    suite.add("passed", [&](Context& c) {
        ARIADNE_REQUIRE_CTX(c, ++evaluated == 1);
        ARIADNE_EXPECT_THROWS_MESSAGE_CTX(c, std::runtime_error,
            throw std::runtime_error("source: missing item"), "missing");
    });
    suite.add("failed fatal", [&](Context& c) {
        auto scope = c.scope("phase", 7);
        ARIADNE_REQUIRE_CTX(c, false);
        ++continued;
    });
    suite.add("indeterminate fatal", [&](Context& c) {
        ARIADNE_REQUIRE_CTX(c, (ThreeValued{false, true}));
        ++continued;
    });
    suite.add("nested exception checker", [&](Context& c) {
        c.expect_exception([&] { ARIADNE_REQUIRE_CTX(c, false); }, "fatal is not exception");
        ++continued;
    });
    suite.add("wrong message", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_MESSAGE_CTX(c, std::runtime_error,
            throw std::runtime_error("actual"), "expected");
    });
    suite.add("wrong type", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_MESSAGE_CTX(c, std::runtime_error,
            throw std::logic_error("actual"), "actual");
    });
    suite.add("missing exception", [&](Context& c) {
        ARIADNE_EXPECT_THROWS_MESSAGE_CTX(c, std::runtime_error,
            (void)0, "expected");
    });
    suite.add("ordinary exception in prerequisite", [&](Context& c) {
        ARIADNE_REQUIRE_CTX(c, ([]() -> bool { throw std::runtime_error("bad prerequisite"); })());
    });
    suite.add("after abort", [&](Context& c) {
        ++continued;
        ARIADNE_PROVE_CTX(c, true);
    });
    ARIADNE_ADD_TEST_TO(suite, Fixture, test_fixture);
    Captured capture;
    Summary sum = suite.run(capture);
    if(sum.passed != 3 || sum.failed != 5 || sum.inconclusive != 1 || sum.errors != 1)
        return 1;
    if(capture.results.size() != 10 || evaluated != 1 || continued != 1) return 2;
    if(capture.results[1].assertions.size() != 1 ||
       capture.results[1].assertions[0].context.size() != 1) return 3;
    if(capture.results[2].assertions[0].outcome != Outcome::Inconclusive) return 4;
    if(capture.results[3].assertions.size() != 1 ||
       capture.results[3].assertions[0].outcome != Outcome::Failed) return 5;
    if(capture.results[4].assertions[0].actual != "actual" ||
       capture.results[4].assertions[0].expected != "expected") return 6;
    if(capture.results[5].assertions[0].detail != "unexpected exception type") return 7;
    if(capture.results[6].assertions[0].detail != "no exception thrown") return 8;
    if(capture.results[7].assertions[0].outcome != Outcome::Error) return 9;

    // The previously uncovered NoThrow path belongs to this exact Fixture
    // instantiation; verify a passing method under that execution policy.
    // Also check that an explicit non-prefixed name is preserved verbatim.
    Suite named_fixture("explicit names and NoThrow");
    add_fixture_test<Fixture>(named_fixture, "without_prefix",
        &Fixture::test_fixture, FixtureCheck::NoThrow);
    Captured named_capture;
    auto const named_summary = named_fixture.run(named_capture);
    if (named_summary.passed != 1 || named_summary.failed != 0 ||
        named_summary.errors != 0 || named_capture.results.size() != 1 ||
        named_capture.results[0].name != "without_prefix" ||
        named_capture.results[0].assertions.size() != 3 ||
        named_capture.results[0].assertions[2].outcome != Outcome::Passed)
        return 10;
    return 0;
}
