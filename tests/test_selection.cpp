#include "ariadne-testing.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Ariadne::Testing;
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    std::vector<SelectionInfo> selections;
    void selection_started(SelectionInfo const& info) override { selections.push_back(info); }
    void case_finished(CaseResult const& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};
struct LegacyReporter final : Reporter {
    int finished = 0;
    int suites = 0;
    // Deliberately do not override optional selection_started().
    void case_finished(CaseResult const&) override { ++finished; }
    void suite_finished(std::string_view, Summary const&) override { ++suites; }
};
int main() {
    Suite suite("selection");
    int fast=0,slow=0,other=0;
    suite.add("unrelated", [&](Context& c){++other;c.prove(false,"must not run");});
    suite.add("fast a", [&](Context& c){++fast;c.prove(true,"selected");},
              CaseMetadata{{"fast","numeric"},"A"});
    suite.add("slow a", [&](Context& c){++slow;c.prove(true,"selected");},
              CaseMetadata{{"slow"},"A"});
    auto b=suite_variant(suite,"B");
    b.add("fast b",[&](Context& c){++fast;c.prove(true,"selected");},{"fast"});
    Capture capture;
    auto const selected=suite.run_selected(capture,CaseFilter{"",{"fast"},"A"});
    bool ok=true;
    auto verify=[&](bool value,std::string const& msg) {
        if(!value){std::cerr<<msg<<'\n';ok=false;}
    };
    verify(selected.selection.registered==4 && selected.selection.selected==1 &&
           !selected.selection.complete() && selected.selected_successful() &&
           !selected.successful() && selected.summary.passed==1,
           "partial selection incorrectly treated as full success");
    verify(fast==1 && slow==0 && other==0 && capture.cases.size()==1 &&
           capture.cases[0].case_index==2 && capture.cases[0].case_total==4 &&
           capture.cases[0].implementation=="A" && capture.cases[0].tags.size()==2,
           "unselected bodies executed or metadata lost");
    verify(capture.selections.size()==1 && capture.selections[0].selected==1,
           "selection notice missing");
    Capture none;
    auto const empty=suite.run_selected(none,CaseFilter{"nonexistent",{},""});
    verify(empty.selection.selected==0 && !empty.successful() &&
           !empty.selected_successful() && none.cases.empty(),
           "empty selection must never be successful");
    Capture all;
    auto const complete=suite.run_selected(all,{});
    verify(complete.selection.complete() && complete.selection.selected==4 &&
           all.cases.size()==4 && other==1 && slow==1 && fast==3 &&
           complete.summary.failed==1 && !complete.successful(),
           "unfiltered execution must run every registered case");
    Capture variant;
    auto const bv=suite.run_selected(variant,CaseFilter{"",{},"B"});
    verify(bv.selection.selected==1 && variant.cases.size()==1 &&
           variant.cases[0].implementation=="B",
           "variant implementation identity not available");
    bool rejected=false;
    try {suite.add("invalid",[](Context&){},CaseMetadata{{"dup","dup"},"A"});}
    catch(std::invalid_argument const&){rejected=true;}
    verify(rejected,"duplicate tags must be rejected");
    std::ostringstream console;
    ConsoleReporter live(console);
    auto const subset=suite.run_selected(live,CaseFilter{"fast",{},"B"});
    verify(subset.selection.selected==1 &&
           console.str().find("[SELECTION 1/4]")!=std::string::npos &&
           console.str().find("[RUN 4/4]")!=std::string::npos,
           "live selection message or original registration index missing");
    // The optional selection lifecycle callback must remain compatible with
    // reporters implemented before selection support was introduced.
    LegacyReporter legacy;
    auto const legacy_selection=suite.run_selected(legacy,CaseFilter{"fast",{},"B"});
    verify(legacy_selection.selection.selected==1 &&
           legacy_selection.selected_successful() &&
           legacy.finished==1 && legacy.suites==1,
           "selection must work with a reporter using the default lifecycle hook");
    // An unfiltered run with every case passing must be a complete success,
    // not merely a successful selected subset.
    Suite passing_suite("complete successful selection");
    passing_suite.add("pass", [](Context& c) { c.prove(true,"passed"); });
    Capture passing_capture;
    auto const passing=passing_suite.run_selected(passing_capture,{});
    verify(passing.selection.complete() && passing.successful() &&
           passing.selected_successful() && passing_capture.cases.size()==1 &&
           passing_capture.cases[0].outcome==Outcome::Passed,
           "complete successful selection must pass the full-suite gate");
    return ok?0:1;
}
