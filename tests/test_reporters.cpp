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
    return ok?0:1;
}
