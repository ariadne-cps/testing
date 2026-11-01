#include "ariadne-testing.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace Ariadne::Testing;

struct InformativeFixture : ContextFixture {
    using ContextFixture::ContextFixture;

    void record() {
        ARIADNE_TESTING_NOTE("fixture-note=" << 5);
        ARIADNE_TESTING_INSPECT("fixture-value", 11);
    }
};

int main() {
    std::ostringstream console, json, junit;
    ConsoleReporter live(console);
    JsonReporter structured(json);
    JUnitReporter xml(junit);
    ReporterFanout fanout{std::ref(live),std::ref(structured),std::ref(xml)};
    Suite suite("report & <suite>");
    suite.add("passing", [&](Context& c) {
        if (console.str().find("[RUN 1/5]") == std::string::npos)
            throw std::logic_error("RUN was not streamed before test body");
        c.check([&] {
            if (console.str().find("[CHECK 1.1]") == std::string::npos)
                throw std::logic_error("CHECK was not streamed before predicate");
            return true;
        }, "predicate after CHECK");
        c.elapsed("duration note", std::chrono::nanoseconds{29});
        c.artifact("reference & <file>.dat","golden <input>","text/plain",
                   ArtifactKind::Reference, ArtifactLifetime::External,
                   {{"source","independent"}});
    });
    suite.add("failing",[](Context& c) {
        auto guard=c.scope("outer & <inner>");
        c.expect_equal(1,2,"different <values> & \"quotes\"");
    });
    suite.add("inconclusive",[](Context& c) {
        c.prove(Evidence::Indeterminate,"uncertain relation");
    });
    suite.add("skipped",[](Context& c) {
        c.skip("unavailable <capability>");
    });
    suite.add("error",[](Context&) {
        throw std::runtime_error("runtime & <failure>");
    });
    auto const summary=suite.run(fanout);
    bool ok=true;
    auto check=[&](bool condition,std::string const& message) {
        if(!condition){std::cerr<<message<<'\n';ok=false;}
    };
    check(summary.passed==1 && summary.failed==1 && summary.inconclusive==1 &&
          summary.skipped==1 && summary.errors==1,"suite outcomes changed");
    const std::string j=json.str(),x=junit.str(),l=console.str();
    for(const char* outcome:{"passed","failed","inconclusive","skipped","error"})
        check(j.find(std::string("\"outcome\":\"")+outcome+"\"")!=std::string::npos,
              "JSON lost outcome "+std::string(outcome));
    check(j.find("\"suite\":\"report & <suite>\"")!=std::string::npos &&
          j.find("\"duration_ns\":")!=std::string::npos &&
          j.find("\"duration_ns\":29")!=std::string::npos &&
          j.find("\"context\":[\"outer & <inner>\"]")!=std::string::npos &&
          j.find("\"path\":\"reference & <file>.dat\"")!=std::string::npos &&
          j.find("\"metadata\":[{\"name\":\"source\",\"value\":\"independent\"}]")!=std::string::npos &&
          j.find("\\\"quotes\\\"")!=std::string::npos,
          "JSON lost structured data or escaping");
    check(x.find("<testsuite name=\"report &amp; &lt;suite&gt;\" tests=\"5\"")!=std::string::npos &&
          x.find("failures=\"2\" errors=\"1\" skipped=\"1\"")!=std::string::npos &&
          x.find("<failure type=\"failed\"")!=std::string::npos &&
          x.find("<failure type=\"inconclusive\"")!=std::string::npos &&
          x.find("<error type=\"error\"")!=std::string::npos &&
          x.find("<skipped message=\"skipped\"")!=std::string::npos &&
          x.find("outer &amp; &lt;inner&gt;")!=std::string::npos &&
          x.find("artifact.0.path")!=std::string::npos &&
          x.find("reference &amp; &lt;file&gt;.dat")!=std::string::npos,
          "JUnit lost outcomes, contexts, artifacts or escaping");
    check(l.find("[RUN 1/5] report & <suite> / passing")!=std::string::npos &&
          l.find("[CHECK 1.1] predicate after CHECK")!=std::string::npos &&
          l.find("[RUN 5/5] report & <suite> / error")!=std::string::npos,
          "live console progress lost");
    // Reporter instances can be reused for consecutive suites without
    // leaking completed cases into a subsequent report.
    Suite next("empty suite");
    auto second=next.run(fanout);
    check(second.passed==0 &&
          json.str().find("\"suite\":\"empty suite\",\"selection\"")!=std::string::npos &&
          json.str().find("\"cases\":[]")!=std::string::npos &&
          junit.str().find("<testsuite name=\"empty suite\" tests=\"0\"")!=std::string::npos,
          "reporter reuse leaked cases");
    // A filtered execution must not be misreported as full regression.
    std::ostringstream selected_json,selected_xml;
    JsonReporter sj(selected_json);
    JUnitReporter sx(selected_xml);
    ReporterFanout selected_reporting{std::ref(sj),std::ref(sx)};
    Suite filtered("filtered report");
    filtered.add("first",[](Context& c){c.prove(true,"passed");},
                 CaseMetadata{{"fast"},"A"});
    filtered.add("second",[](Context& c){c.prove(true,"passed");},
                 CaseMetadata{{"slow"},"B"});
    auto const selection=filtered.run_selected(selected_reporting,CaseFilter{"",{"fast"},"A"});
    check(selection.selected_successful() && !selection.successful(),
          "partial selection must not be complete success");
    check(selected_json.str().find("\"selected\":1")!=std::string::npos &&
          selected_json.str().find("\"registered\":2")!=std::string::npos &&
          selected_json.str().find("\"complete\":false")!=std::string::npos &&
          selected_json.str().find("\"full_suite_successful\":false")!=std::string::npos &&
          selected_json.str().find("\"implementation\":\"A\"")!=std::string::npos &&
          selected_json.str().find("\"tags\":[\"fast\"]")!=std::string::npos,
          "JSON silently loses selection coverage or case metadata");
    check(selected_xml.str().find("tests=\"2\" failures=\"1\"")!=std::string::npos &&
          selected_xml.str().find("type=\"incomplete_coverage\"")!=std::string::npos &&
          selected_xml.str().find("selection.selected")!=std::string::npos &&
          selected_xml.str().find("selection.registered")!=std::string::npos &&
          selected_xml.str().find("tag.0")!=std::string::npos,
          "JUnit must report partial coverage as a synthetic failure");
    Suite everything("full report");
    everything.add("only",[](Context& c){c.prove(true,"passed");});
    auto const full=everything.run(selected_reporting);
    check(full.successful() &&
          selected_json.str().find("\"complete\":true")!=std::string::npos &&
          selected_xml.str().find("<testsuite name=\"full report\" tests=\"1\" failures=\"0\"")!=std::string::npos,
          "reused reporters must clear selection and synthetic failure");
    Suite no_match("empty selection");
    no_match.add("only",[](Context& c){c.prove(true,"not run");});
    auto const empty=no_match.run_selected(selected_reporting,CaseFilter{"missing",{},""});
    check(!empty.successful() &&
          selected_xml.str().find("<testsuite name=\"empty selection\" tests=\"1\" failures=\"1\"")!=std::string::npos,
          "empty partial selection must not produce a green JUnit report");

    std::ostringstream concise_output;
    ConsoleReporter concise(concise_output, ConsoleDetail::Concise);
    Suite noisy("compact diagnostics");
    noisy.add("late mismatch", [](Context& c) {
        for (std::size_t i=0; i<501; ++i) {
            auto row=c.scope("row", i);
            c.prove(true, "accepted oracle row");
        }
        c.expect_equal(3, 4, "last comparison failed");
    });
    noisy.add("unhandled exception", [](Context& c) {
        c.prove(true, "precondition satisfied");
        throw std::runtime_error("sentinel exception");
    });
    noisy.add("undetermined result", [](Context& c) {
        c.prove(Evidence::Indeterminate, "uncertain result");
    });
    noisy.add("passing result", [](Context& c) {
        c.prove(true, "a good result");
    });
    const auto concise_summary = noisy.run(concise);
    const std::string compact = concise_output.str();
    check(concise_summary.passed==1 && concise_summary.failed==1 &&
          concise_summary.errors==1 && concise_summary.inconclusive==1,
          "compact console changed case outcomes");
    check(compact.find("[CHECK ")==std::string::npos &&
          compact.find("[RUN ")==std::string::npos &&
          compact.find("[PASS]")==std::string::npos &&
          compact.find("[SKIP]")==std::string::npos &&
          compact.find("[SCOPE ")==std::string::npos &&
          compact.find("[WARN]")==std::string::npos &&
          compact.find("[INCONCLUSIVE] undetermined result")!=std::string::npos &&
          compact.find("[ERROR] unhandled exception")!=std::string::npos &&
          compact.find("[FAIL] late mismatch")!=std::string::npos,
          "concise reporter must print only problematic cases");
    const auto summary_at=compact.find("[FAILURE SUMMARY] 3 problematic case(s) in compact diagnostics");
    check(summary_at!=std::string::npos &&
          compact.find("compact diagnostics: 1 passed, 1 failed, 1 inconclusive, 0 skipped, 1 errors")
             < summary_at,
          "compact report has no trailing failure index");
    if(summary_at!=std::string::npos) {
        const auto tail=compact.substr(summary_at);
        check(tail.find("[FAIL 1/4] late mismatch")!=std::string::npos &&
              tail.find("last check [1.502]: last comparison failed")!=std::string::npos &&
              tail.find("actual: 3")!=std::string::npos &&
              tail.find("expected: 4")!=std::string::npos &&
              tail.find("[ERROR 2/4] unhandled exception")!=std::string::npos &&
              tail.find("sentinel exception")!=std::string::npos &&
              tail.find("[INCONCLUSIVE 3/4] undetermined result")!=std::string::npos &&
              tail.find("uncertain result")!=std::string::npos &&
              tail.find("passing result")==std::string::npos,
              "failure index lost identity, final CHECK, evidence, or exception");
    }
    Suite reuse_console("successful suite");
    reuse_console.add("passing", [](Context& c) { c.prove(true, "ok"); });
    const auto before_reuse=concise_output.str().size();
    const auto reuse_summary=reuse_console.run(concise);
    check(reuse_summary.successful() &&
          concise_output.str().substr(before_reuse).find("[FAILURE SUMMARY]")==std::string::npos,
          "console retained failures from preceding suite");
    Suite informative("structured informational output");
    informative.add("success", [](Context& ctx) {
        ARIADNE_TESTING_NOTE_CTX(ctx, "value=" << 42 << " / " << 3);
        ARIADNE_TESTING_INSPECT_CTX(ctx, "actual", 42);
        ctx.prove(true, "accepted");
    });
    informative.add("failure", [](Context& ctx) {
        ARIADNE_TESTING_NOTE_CTX(ctx, "value=" << 7 << " / " << 2);
        ARIADNE_TESTING_INSPECT_CTX(ctx, "actual", 7);
        InformativeFixture fixture(ctx);
        fixture.record();
        ctx.prove(false, "rejected");
    });
    std::ostringstream information_output;
    ConsoleReporter information_reporter(information_output, ConsoleDetail::Concise);
    auto const information_summary = informative.run(information_reporter);
    check(information_summary.passed == 1 && information_summary.failed == 1 &&
          information_output.str().find("value=42 / 3") == std::string::npos &&
          information_output.str().find("diagnostic: value=7 / 2") != std::string::npos &&
          information_output.str().find("diagnostic: actual = 7") != std::string::npos &&
          information_output.str().find("diagnostic: fixture-note=5") != std::string::npos &&
          information_output.str().find("diagnostic: fixture-value = 11") != std::string::npos &&
          information_output.str().find("actual = 42") == std::string::npos &&
          information_output.str().find("[CHECK ") == std::string::npos,
          "informational stream macros must record and filter without checks");

    Suite cli_suite("console option parsing");
    int cli_executions = 0;
    cli_suite.add("check verbosity", [&](Context& c) {
        ++cli_executions;
        c.prove(true, "first");
        c.prove(true, "second");
    });
    char const* const full_args[] = {"selftest", "--full"};
    char const* const concise_args[] = {"selftest", "--concise"};
    char const* const conflict_args[] = {"selftest", "--full", "--concise"};
    char const* const wrong_args[] = {"selftest", "--bogus"};
    char const* const help_args[] = {"selftest", "--help"};
    std::ostringstream direct_output, full_output, concise_cli_output, invalid_output;
    check(run_suite(cli_suite, direct_output)==0 &&
          direct_output.str().find("[CHECK 1.2] second")!=std::string::npos,
          "direct suite runner must remain full by default");
    check(run_suite(cli_suite, 2, full_args, full_output)==0 &&
          full_output.str().find("[CHECK 1.2] second")!=std::string::npos,
          "explicit --full must preserve all checks");
    check(run_suite(cli_suite, 2, concise_args, concise_cli_output)==0 &&
          concise_cli_output.str().find("[CHECK ")==std::string::npos &&
          concise_cli_output.str().find("[RUN ")==std::string::npos &&
          concise_cli_output.str().find("[PASS]")==std::string::npos &&
          concise_cli_output.str().find("console option parsing: 1 passed")!=std::string::npos,
          "--concise must emit only the summary for successful suites");
    const int executions_before_invalid = cli_executions;
    check(run_suite(cli_suite, 3, conflict_args, invalid_output)==2 &&
          run_suite(cli_suite, 2, wrong_args, invalid_output)==2 &&
          run_suite(cli_suite, 2, help_args, invalid_output)==0 &&
          cli_executions == executions_before_invalid &&
          invalid_output.str().find("Usage:")!=std::string::npos,
          "unknown/conflicting/help arguments must not run tests");
    return ok?0:1;
}
