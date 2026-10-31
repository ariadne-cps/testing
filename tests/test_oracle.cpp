#include "ariadne-testing.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    Capture capture;auto summary=suite.run(capture);
    bool ok=true;
    auto check=[&](bool condition,std::string const& label){if(!condition){std::cerr<<label<<'\n';ok=false;}};
    check(summary.passed==3 && summary.failed==5 && summary.errors==3 &&
          capture.cases.size()==11 && capture.checks==12,
          "oracle outcomes: passed="+std::to_string(summary.passed)+
          " failed="+std::to_string(summary.failed)+
          " errors="+std::to_string(summary.errors));
    if(capture.cases.size()==11){
        const std::vector<Outcome> expect{
            Outcome::Passed,Outcome::Failed,Outcome::Failed,Outcome::Failed,
            Outcome::Failed,Outcome::Failed,Outcome::Error,Outcome::Error,
            Outcome::Error,Outcome::Passed,Outcome::Passed};
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
    return ok?0:1;
}
