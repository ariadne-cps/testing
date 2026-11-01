#include "ariadne-testing.hpp"
#include <array>
#include <utility>
#include <string_view>
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
          compact.find("[INCONCLUSIVE]")==std::string::npos &&
          compact.find("[ERROR]")==std::string::npos &&
          compact.find("[FAIL]")==std::string::npos &&
          compact.rfind("compact diagnostics: 1 passed, 1 failed, 1 inconclusive, 0 skipped, 1 errors",0)==0,
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
          information_output.str().rfind("structured informational output: 1 passed, 1 failed, 0 inconclusive, 0 skipped, 0 errors", 0) == 0 &&
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
    std::ostringstream direct_output, default_cli_output, full_output, concise_cli_output, invalid_output;
    check(run_suite(cli_suite, direct_output)==0 &&
          direct_output.str().find("[CHECK 1.2] second")!=std::string::npos,
          "direct suite runner must remain full by default");
    check(run_suite(cli_suite, 1, full_args, default_cli_output)==0 &&
          default_cli_output.str().find("[CHECK 1.2] second")!=std::string::npos,
          "argc/argv runner must remain full without flags");
    check(run_suite(cli_suite, 2, full_args, full_output)==0 &&
          full_output.str().find("[CHECK 1.2] second")!=std::string::npos,
          "explicit --full must preserve all checks");
    check(run_suite(cli_suite, 2, concise_args, concise_cli_output)==0 &&
          concise_cli_output.str().find("[CHECK ")==std::string::npos &&
          concise_cli_output.str().find("[RUN ")==std::string::npos &&
          concise_cli_output.str().find("[PASS]")==std::string::npos &&
          concise_cli_output.str().find("console option parsing: 1 passed")!=std::string::npos,
          "--concise must emit only the summary for successful suites");
    // Repeating the same verbosity option is valid and must not conflict.
    char const* const repeated_full[] = {"selftest", "--full", "--full"};
    char const* const repeated_concise[] = {"selftest", "--concise", "--concise"};
    std::ostringstream repeated_full_output, repeated_concise_output;
    check(run_suite(cli_suite, 3, repeated_full, repeated_full_output) == 0 &&
          repeated_full_output.str().find("[CHECK 1.2] second") != std::string::npos &&
          run_suite(cli_suite, 3, repeated_concise, repeated_concise_output) == 0 &&
          repeated_concise_output.str().find("[CHECK ") == std::string::npos &&
          repeated_concise_output.str().find("console option parsing: 1 passed") != std::string::npos,
          "repeated identical verbosity options must be accepted");
    const int executions_before_invalid = cli_executions;
    check(run_suite(cli_suite, 3, conflict_args, invalid_output)==2 &&
          run_suite(cli_suite, 2, wrong_args, invalid_output)==2 &&
          run_suite(cli_suite, 2, help_args, invalid_output)==0 &&
          cli_executions == executions_before_invalid &&
          invalid_output.str().find("Usage:")!=std::string::npos,
          "unknown/conflicting/help arguments must not run tests");
    // Direct reporter input exercises all defined wait observations without
    // depending on timing or scheduling a real asynchronous operation.
    std::ostringstream wait_json_stream, wait_xml_stream;
    JsonReporter wait_json(wait_json_stream);
    JUnitReporter wait_xml(wait_xml_stream);
    const std::array<std::pair<WaitStatus, std::string_view>, 5> waits{{
        {WaitStatus::Ready, "ready"},
        {WaitStatus::Timeout, "timeout"},
        {WaitStatus::Deferred, "deferred"},
        {WaitStatus::InvalidTimeout, "invalid_timeout"},
        {WaitStatus::Unexpected, "unexpected"}
    }};
    for (std::size_t i = 0; i < waits.size(); ++i) {
        CaseResult result;
        result.suite_name = "wait serialization";
        result.name = std::string(waits[i].second);
        result.case_index = i + 1;
        result.case_total = waits.size();
        AssertionRecord assertion{};
        assertion.outcome = Outcome::Passed;
        assertion.description = "wait observation";
        assertion.check_index = 1;
        assertion.wait = WaitObservation{waits[i].first,
            std::chrono::nanoseconds{17}, std::chrono::nanoseconds{23}, i % 2 == 0};
        result.assertions.push_back(assertion);
        wait_json.case_finished(result);
        wait_xml.case_finished(result);
    }
    Summary wait_summary;
    wait_summary.passed = waits.size();
    wait_json.suite_finished("wait serialization", wait_summary);
    wait_xml.suite_finished("wait serialization", wait_summary);
    for (auto const& [status, name] : waits) {
        (void)status;
        std::string const label(name);
        check(wait_json_stream.str().find("\"status\":\"" + label + "\"") != std::string::npos &&
              wait_xml_stream.str().find("wait: " + label + " elapsed_ns=17 available_budget_ns=23") != std::string::npos,
              "JSON/JUnit wait status missing: " + label);
    }
    check(wait_json_stream.str().find("\"absolute_deadline\":true") != std::string::npos &&
          wait_json_stream.str().find("\"absolute_deadline\":false") != std::string::npos,
          "JSON wait observation lost absolute deadline classification");
    // The full reporting stack must retain multiple observations and
    // selection metadata, including controls escaped for each output format.
    Suite rich("rich reporting");
    rich.add("rich symbols", [](Context& c) {
        auto outer = c.scope("outer\'s layer");
        auto inner = c.scope("inner");
        c.prove(true, "first observation");
        c.prove(true, "second observation");
        c.note(std::string("ctrl ") + char{1});
        c.warning("secondary observation");
        c.inspect("number", 42);
        c.artifact("first.dat", "first", "application/octet-stream",
                   ArtifactKind::Output, ArtifactLifetime::External,
                   {{"key1", "value1"}, {"key2", "value2"}});
        c.artifact("difference.dat", "second", "application/octet-stream",
                   ArtifactKind::Difference, ArtifactLifetime::External);
    }, CaseMetadata{{"io", "report"}, "native"});
    rich.add("excluded", [](Context& c) { c.prove(false, "not selected"); },
             CaseMetadata{{"slow"}, "native"});
    std::ostringstream rich_console, rich_json, rich_xml;
    ConsoleReporter rich_live(rich_console);
    JsonReporter rich_structured(rich_json);
    JUnitReporter rich_junit(rich_xml);
    ReporterFanout rich_fanout{std::ref(rich_live),
                                std::ref(rich_structured), std::ref(rich_junit)};
    auto const rich_result = rich.run_selected(
        rich_fanout, CaseFilter{"rich", {"io", "regression"}, "native"});
    check(rich_result.selection.selected == 1 &&
          rich_result.selected_successful() && !rich_result.successful(),
          "rich selection must execute exactly one passing case");
    check(rich_console.str().find("any_tags=io,regression partial") != std::string::npos &&
          rich_console.str().find("name_contains=rich implementation=native") != std::string::npos,
          "console did not preserve the complete selection filter");
    check(rich_json.str().find("\"tags\":[\"io\",\"report\"]") != std::string::npos &&
          rich_json.str().find("\"context\":[\"outer\'s layer\",\"inner\"]") != std::string::npos &&
          rich_json.str().find("\"key2\"") != std::string::npos &&
          rich_json.str().find("\"path\":\"difference.dat\"") != std::string::npos &&
          rich_json.str().find("\\u0001") != std::string::npos,
          "JSON lost repeated tags, scopes, artifacts, metadata or control escaping");
    check(rich_xml.str().find("outer&apos;s layer") != std::string::npos &&
          rich_xml.str().find("ctrl ?") != std::string::npos &&
          rich_xml.str().find("artifact.1.path") != std::string::npos &&
          rich_xml.str().find("selection.any_tag.1") != std::string::npos,
          "JUnit lost XML escaping, repeated artifact properties or selection tags");
    // JSON string escaping must preserve each supported control character;
    // this is a normal reporter input, independent of any failing test case.
    CaseResult escaped_case;
    escaped_case.suite_name = "string escaping";
    escaped_case.name = "slash\\ quote\" backspace\b formfeed\f newline\n return\r tab\t";
    escaped_case.case_index = 1;
    escaped_case.case_total = 1;
    std::ostringstream escaped_json_output, escaped_xml_output;
    JsonReporter escaped_json(escaped_json_output);
    JUnitReporter escaped_xml(escaped_xml_output);
    escaped_json.case_finished(escaped_case);
    escaped_xml.case_finished(escaped_case);
    Summary escaped_summary;
    escaped_summary.passed = 1;
    escaped_json.suite_finished("string escaping", escaped_summary);
    escaped_xml.suite_finished("string escaping", escaped_summary);
    std::string const escaped_name =
        R"escaped("name":"slash\\ quote\" backspace\b formfeed\f newline\n return\r tab\t")escaped";
    check(escaped_json_output.str().find(escaped_name) != std::string::npos,
          "JSON must escape quotes, backslashes and every named control character");
    check(escaped_xml_output.str().find("quote&quot; backspace? formfeed?") !=
              std::string::npos,
          "JUnit XML must escape quotes and replace forbidden XML controls");

    Suite crowded("bounded concise diagnostics");
    crowded.add("failure with many notes", [](Context& c) {
        for (int i = 0; i < 18; ++i)
            c.note("event-" + std::to_string(i));
        c.prove(false, "failure after diagnostics");
    });
    std::ostringstream crowded_output;
    ConsoleReporter crowded_reporter(crowded_output, ConsoleDetail::Concise);
    auto const crowded_summary = crowded.run(crowded_reporter);
    check(crowded_summary.failed == 1 &&
          crowded_output.str().find("diagnostic: event-15") != std::string::npos &&
          crowded_output.str().find("diagnostic: additional events omitted") != std::string::npos &&
          crowded_output.str().find("diagnostic: event-16") == std::string::npos,
          "concise diagnostic limit changed");

    bool rejected_empty_fanout = false;
    try {
        ReporterFanout empty_targets(std::initializer_list<std::reference_wrapper<Reporter>>{});
    } catch (std::invalid_argument const& error) {
        rejected_empty_fanout =
            std::string_view(error.what()) == "ReporterFanout requires a reporter";
    }
    check(rejected_empty_fanout, "empty reporter fanout must be rejected");

    CaseResult assertionless;
    assertionless.suite_name = "external cases";
    assertionless.name = "external failure";
    assertionless.outcome = Outcome::Failed;
    assertionless.case_index = 1;
    assertionless.case_total = 1;
    Summary assertionless_summary;
    assertionless_summary.failed = 1;
    std::ostringstream assertionless_xml;
    JUnitReporter assertionless_junit(assertionless_xml);
    assertionless_junit.case_finished(assertionless);
    assertionless_junit.suite_finished("external cases", assertionless_summary);
    check(assertionless_xml.str().find("failures=\"1\"") != std::string::npos &&
          assertionless_xml.str().find("message=\"case has no assertion\"") != std::string::npos,
          "JUnit must retain the classification of assertionless cases");

    CaseResult retention_case;
    retention_case.suite_name = "artifact retention";
    retention_case.name = "retention policies";
    retention_case.case_index = 1;
    retention_case.case_total = 1;
    std::array<ArtifactLifetime, 4> const lifetimes{
        ArtifactLifetime::External, ArtifactLifetime::Temporary,
        ArtifactLifetime::KeepOnFailure, ArtifactLifetime::KeepAlways};
    for (std::size_t i = 0; i < lifetimes.size(); ++i) {
        ArtifactRecord record;
        record.path = "artifact-" + std::to_string(i) + ".dat";
        record.label = "captured";
        record.media_type = "application/octet-stream";
        record.lifetime = lifetimes[i];
        retention_case.artifacts.push_back(std::move(record));
    }
    std::ostringstream retention_json;
    JsonReporter retention_reporter(retention_json);
    retention_reporter.case_finished(retention_case);
    Summary retention_summary;
    retention_summary.passed = 1;
    retention_reporter.suite_finished("artifact retention", retention_summary);
    for (auto const* label : {"external", "temporary", "keep_on_failure", "keep_always"})
        check(retention_json.str().find(std::string("\"lifetime\":\"") + label + "\"") != std::string::npos,
              "JSON must preserve artifact lifetime: " + std::string(label));
    return ok?0:1;
}
