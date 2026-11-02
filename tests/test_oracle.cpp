#include "ariadne-testing.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Ariadne::Testing;
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    int checks=0;
    void check_started(CaseResult const&,std::size_t,std::string_view) override {++checks;}
    void case_finished(CaseResult const& result) override {cases.push_back(result);}
    void suite_finished(std::string_view,Summary const&) override {}
};
int main(){
    using K=OracleRowId;
    using R=OracleRow;
    std::vector<R> frozen{{K{"a",0,0},true},{K{"a",0,1},true},{K{"b",1,0},true}};
    std::vector<R> valid=frozen;
    std::vector<R> missing{frozen[0],frozen[2]};
    std::vector<R> duplicate{frozen[0],frozen[0],frozen[2]};
    std::vector<R> extra=frozen;extra.push_back({K{"z",0,0},true});
    std::vector<R> malformed=frozen;malformed[1].parsed=false;
    std::vector<R> unordered{frozen[2],frozen[1],frozen[0]};
    std::vector<R> bad_manifest=frozen;bad_manifest.push_back(frozen[0]);
    std::vector<R> missing_key=frozen;missing_key[1].id.key.clear();
    Suite suite("frozen oracle integrity");
    auto add=[&](std::string name,std::vector<R> const& expected,std::vector<R> const& actual){
        suite.add(std::move(name),[&,expected,actual](Context& c){
            ARIADNE_TESTING_EXPECT_ORACLE_ROWS_CTX(c,expected,actual);
        });
    };
    add("complete match",frozen,valid);
    add("missing",frozen,missing);
    add("duplicate",frozen,duplicate);
    add("unexpected extra",frozen,extra);
    add("malformed",frozen,malformed);
    add("wrong order",frozen,unordered);
    add("duplicate manifest",bad_manifest,valid);
    add("empty manifest",{},valid);
    add("missing manifest key",missing_key,valid);
    std::filesystem::path ref;
    suite.add("register immutable provenance",[&](Context& c){
        ref=c.work_directory()/"frozen.tsv";
        {std::ofstream output(ref,std::ios::binary);output<<"frozen data\n";}
        OracleMetadata m; m.reference=ref;m.label="golden";
        m.row_key_column="id";m.seed=0;m.precision_bits=128;
        m.input_attributes={{"mode","validated"},{"input","x=1"}};
        c.register_oracle(m);
        c.expect_file_signature(ref,"frozen data\n","reference unchanged");
    });
    suite.add("reject malformed metadata without adding artifact",[](Context& c){
        OracleMetadata m;m.reference="frozen.tsv";m.label="oracle";m.row_key_column="id";
        m.input_attributes={{"same","1"},{"same","2"}};
        bool rejected=false;
        try{c.register_oracle(m);}catch(std::invalid_argument const&){rejected=true;}
        c.prove(rejected && c.work_directory()!=std::filesystem::path{}, "duplicates rejected");
    });
    suite.add("reject zero precision",[](Context& c){
        OracleMetadata m; m.reference="frozen.tsv"; m.label="oracle";
        m.row_key_column="id"; m.precision_bits=std::uint32_t{0};
        bool rejected=false;
        try { c.register_oracle(m); } catch(std::invalid_argument const&) { rejected=true; }
        c.prove(rejected,"zero precision must not be accepted");
    });
    suite.add("absent precision is optional",[](Context& c){
        OracleMetadata m; m.reference="frozen.tsv"; m.label="oracle";
        m.row_key_column="id";
        c.register_oracle(m);
        c.prove(true,"absent precision is permitted");
    });
    Capture capture;auto summary=suite.run(capture);
    bool ok=true;
    auto check=[&](bool condition,std::string const& label){if(!condition){std::cerr<<label<<'\n';ok=false;}};
    check(summary.passed==5 && summary.failed==5 && summary.errors==3 &&
          capture.cases.size()==13 && capture.checks==13,
          "oracle outcomes: passed="+std::to_string(summary.passed)+
          " failed="+std::to_string(summary.failed)+
          " errors="+std::to_string(summary.errors));
    if(capture.cases.size()==13){
        const std::vector<Outcome> expect{
            Outcome::Passed,Outcome::Failed,Outcome::Failed,Outcome::Failed,
            Outcome::Failed,Outcome::Failed,Outcome::Error,Outcome::Error,
            Outcome::Error,Outcome::Passed,Outcome::Passed,
            Outcome::Passed,Outcome::Passed};
        for(std::size_t i=0;i<expect.size();++i)
            check(capture.cases[i].outcome==expect[i],"wrong case outcome "+std::to_string(i));
        const char* labels[]={"missing row","duplicate row","extra row",
                              "malformed row","out-of-order row","invalid manifest"};
        for(std::size_t j=0;j<6;++j)
            check(capture.cases[j+1].assertions.size()==1 &&
                  capture.cases[j+1].assertions[0].detail.find(labels[j])!=std::string::npos,
                  "missing oracle diagnostic "+std::to_string(j));
        auto const& artifacts=capture.cases[9].artifacts;
        check(artifacts.size()==1 && artifacts[0].kind==ArtifactKind::Reference &&
              artifacts[0].lifetime==ArtifactLifetime::External &&
              artifacts[0].metadata.size()==5,
              "oracle reference metadata not registered");
        check(capture.cases[10].artifacts.empty(),"bad metadata was registered");
    }
    auto identity=validate_oracle_rows(frozen,valid);
    check(identity.valid() && identity.expected_rows==3 &&
          identity.observed_rows==3,"oracle identities not reusable");
    // Direct Context API must provide the same observable outcomes as the
    // deferred macro entry point, including an invalid frozen manifest.
    Suite direct_rows("direct oracle rows");
    direct_rows.add("matching rows", [&](Context& c) {
        c.expect_oracle_rows(frozen, valid, "direct complete oracle match");
    });
    direct_rows.add("missing rows", [&](Context& c) {
        c.expect_oracle_rows(frozen, missing, "direct missing oracle row");
    });
    direct_rows.add("invalid frozen reference", [&](Context& c) {
        c.expect_oracle_rows(bad_manifest, valid, "direct invalid manifest");
    });
    Capture direct_capture;
    auto const direct_summary = direct_rows.run(direct_capture);
    check(direct_summary.passed==1 && direct_summary.failed==1 &&
          direct_summary.errors==1 && direct_capture.checks==3 &&
          direct_capture.cases.size()==3,
          "direct oracle rows result classification");
    if (direct_capture.cases.size()==3) {
        check(direct_capture.cases[0].assertions.size()==1 &&
              direct_capture.cases[0].assertions[0].outcome==Outcome::Passed &&
              direct_capture.cases[1].assertions[0].outcome==Outcome::Failed &&
              direct_capture.cases[2].assertions[0].outcome==Outcome::Error,
              "direct oracle rows must record ordered assertion outcomes");
    }
    // Reject invalid public metadata before any artifact is registered.
    // Each independent invalid field must fail with invalid_argument.
    Suite metadata_contracts("oracle metadata contracts");
    OracleMetadata valid_metadata;
    valid_metadata.reference = "independent-reference.tsv";
    valid_metadata.label = "golden reference";
    valid_metadata.row_key_column = "row-id";
    auto rejected_metadata = [&](std::string name, OracleMetadata metadata) {
        metadata_contracts.add(std::move(name), [metadata](Context& c) {
            bool rejected_input = false;
            try { c.register_oracle(metadata); }
            catch (std::invalid_argument const&) { rejected_input = true; }
            c.prove(rejected_input, "invalid metadata must be rejected");
        });
    };
    auto empty_reference = valid_metadata;
    empty_reference.reference.clear();
    rejected_metadata("empty reference", empty_reference);
    auto empty_label = valid_metadata;
    empty_label.label.clear();
    rejected_metadata("empty label", empty_label);
    auto empty_row_key = valid_metadata;
    empty_row_key.row_key_column.clear();
    rejected_metadata("empty row key", empty_row_key);
    auto empty_attribute_name = valid_metadata;
    empty_attribute_name.input_attributes = {{"", "value"}};
    rejected_metadata("empty attribute name", empty_attribute_name);
    auto empty_attribute_value = valid_metadata;
    empty_attribute_value.input_attributes = {{"name", ""}};
    rejected_metadata("empty attribute value", empty_attribute_value);
    Capture metadata_capture;
    auto const metadata_summary = metadata_contracts.run(metadata_capture);
    check(metadata_summary.passed == 5 && metadata_summary.failed == 0 &&
          metadata_summary.errors == 0 && metadata_capture.cases.size() == 5 &&
          metadata_capture.checks == 5,
          "oracle metadata rejection contracts failed");
    for (auto const& outcome : metadata_capture.cases)
        check(outcome.outcome == Outcome::Passed && outcome.artifacts.empty(),
              "rejected Oracle metadata must not register an artifact");
    // Public integrity objects may carry issue kinds from a newer consumer.
    // Rendering must preserve the record and use an explicit unknown label.
    OracleIntegrity unrecognized;
    unrecognized.expected_rows = 4;
    unrecognized.observed_rows = 5;
    unrecognized.issues.push_back({
        static_cast<OracleIssueKind>(255), 3, K{"future", 7, 9}});
    check(!unrecognized.valid() &&
          unrecognized.description() ==
              "expected_rows=4; observed_rows=5; issues=1; first=unknown oracle issue"
              " at index=3; key=future; iteration=7; repetition=9",
          "unknown public Oracle issue kind must remain diagnosable");
    return ok?0:1;
}
