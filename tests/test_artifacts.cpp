#include "ariadne-testing.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
using namespace Ariadne::Testing;
struct Capture : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(CaseResult const& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, Summary const&) override {}
};
int main() {
    Suite s("artifacts");
    std::filesystem::path retained, removed, reference;
    s.add("success cleanup", [&](Context& c) {
        removed=c.work_directory();
        auto f=removed/"output.bin";
        { std::ofstream out(f,std::ios::binary); out<<"ABC"; }
        c.artifact(f,"output","application/octet-stream",ArtifactKind::Output,ArtifactLifetime::Temporary);
        c.expect_file_exists(f,"exists");
        c.expect_file_signature(f,"AB","signature");
        c.prove(c.work_directory()==removed,"same directory");
    });
    s.add("retained failure and external golden", [&](Context& c) {
        retained=c.work_directory();
        auto f=retained/"actual.bin";
        { std::ofstream out(f,std::ios::binary); out<<"ABC"; }
        c.artifact(f,"output","application/octet-stream",ArtifactKind::Output,ArtifactLifetime::KeepOnFailure);
        reference=std::filesystem::temp_directory_path()/"ariadne-fixture-reference-golden.bin";
        { std::ofstream out(reference,std::ios::binary); out<<"ABD"; }
        c.artifact(reference,"golden","application/octet-stream",ArtifactKind::Reference);
        c.expect_file_equal(f,reference,"golden differs");
        c.expect_file_signature(f,"BAD","wrong signature");
    });
    s.add("missing file", [](Context& c) {
        c.expect_file_exists(c.work_directory()/"absent","missing");
        c.expect_file_equal(c.work_directory()/"absent",c.work_directory()/"other","cannot compare");
    });
    Capture r; auto sum=s.run(r);
    bool ok=sum.passed==1 && sum.failed==2 && sum.errors==0 &&
        !std::filesystem::exists(removed) && std::filesystem::exists(retained) &&
        std::filesystem::exists(reference) && r.cases.size()==3 &&
        r.cases[1].artifacts.size()==2 &&
        r.cases[1].assertions[0].detail.find("offset=2")!=std::string::npos;
    std::error_code error;
    std::filesystem::remove_all(retained,error);
    std::filesystem::remove(reference,error);
    return ok ? 0 : 1;
}
