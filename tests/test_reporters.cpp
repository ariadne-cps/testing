#include "ariadne-testing.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace Ariadne::Testing;

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
          json.str().find("\"suite\":\"empty suite\",\"summary\"")!=std::string::npos &&
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
    return ok?0:1;
}
