#include "ariadne-testing.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
    std::filesystem::path equal_removed, invalid_removed, always_retained, conditional_removed;
    std::filesystem::path not_directory_removed, binary_removed;
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
        reference=std::filesystem::temp_directory_path() / ("ariadne-golden-" + retained.filename().string() + ".bin");
        { std::ofstream out(reference,std::ios::binary); out<<"ABD"; }
        c.artifact(reference,"golden","application/octet-stream",ArtifactKind::Reference);
        c.expect_file_equal(f,reference,"golden differs");
        c.expect_file_signature(f,"BAD","wrong signature");
    });
    s.add("missing file", [](Context& c) {
        c.expect_file_exists(c.work_directory()/"absent","missing");
        c.expect_file_exists(c.work_directory()/"absent-parent"/"artifact.bin",
                             "missing nested file");
        c.expect_file_exists(c.work_directory(),"directory is not a regular file");
        c.expect_file_equal(c.work_directory()/"absent",c.work_directory()/"other",
                            "cannot compare");
    });
    s.add("identical golden content", [&](Context& c) {
        equal_removed = c.work_directory();
        auto const actual = equal_removed / "matching-actual.bin";
        auto const golden = equal_removed / "matching-golden.bin";
        { std::ofstream out(actual, std::ios::binary); out << "ABC"; }
        { std::ofstream out(golden, std::ios::binary); out << "ABC"; }
        c.artifact(actual, "matching output", "application/octet-stream",
                   ArtifactKind::Output, ArtifactLifetime::Temporary);
        c.expect_file_equal(actual, golden, "identical binary output");
    });
    s.add("reject invalid artifact registration", [&](Context& c) {
        invalid_removed = c.work_directory();
        auto const valid = invalid_removed / "valid.bin";
        auto const outside = invalid_removed.parent_path() / "not-owned-by-this-case.bin";
        auto rejects = [&](std::filesystem::path const& target,
                           std::string const& label, ArtifactLifetime lifetime) {
            try {
                c.artifact(target, label, "application/octet-stream",
                           ArtifactKind::Output, lifetime);
            } catch (std::invalid_argument const&) {
                return true;
            }
            return false;
        };
        c.prove(rejects({}, "missing path", ArtifactLifetime::External),
                "reject empty artifact path");
        c.prove(rejects(valid, "", ArtifactLifetime::External),
                "reject empty artifact label");
        c.prove(rejects(outside, "outside case", ArtifactLifetime::Temporary),
                "reject managed output outside case directory");
        c.prove(rejects(invalid_removed, "case root", ArtifactLifetime::KeepAlways),
                "reject managed artifact equal to case root");
    });
    s.add("keep always on passing case", [&](Context& c) {
        always_retained = c.work_directory();
        auto const output = always_retained / "keep.bin";
        { std::ofstream out(output, std::ios::binary); out << "keep"; }
        c.artifact(output, "preserved output", "application/octet-stream",
                   ArtifactKind::Output, ArtifactLifetime::KeepAlways,
                   {{"generator", "test_artifacts"}});
        c.expect_file_exists(output, "preserved file exists");
    });
    s.add("keep on failure cleans passing case", [&](Context& c) {
        conditional_removed = c.work_directory();
        auto const output = conditional_removed / "conditional.bin";
        { std::ofstream out(output, std::ios::binary); out << "temporary"; }
        c.artifact(output, "conditional output", "application/octet-stream",
                   ArtifactKind::Output, ArtifactLifetime::KeepOnFailure);
        c.expect_file_exists(output, "output exists before cleanup");
    });
    s.add("missing golden and signature", [](Context& c) {
        auto const actual = c.work_directory() / "actual.bin";
        auto const missing = c.work_directory() / "missing-golden.bin";
        { std::ofstream out(actual, std::ios::binary); out << "ABC"; }
        c.expect_file_equal(actual, missing, "missing reference fails");
        c.expect_file_signature(missing, "ABC", "missing signature fails");
    });
    s.add("regular file cannot be a parent directory", [&](Context& c) {
        not_directory_removed = c.work_directory();
        auto const parent = not_directory_removed / "parent.bin";
        { std::ofstream out(parent, std::ios::binary); out << "content"; }
        c.expect_file_exists(parent / "child.bin",
                             "a regular file cannot contain a child");
    });
    s.add("binary prefix and end-of-file comparison", [&](Context& c) {
        binary_removed = c.work_directory();
        auto const shorter = binary_removed / "three-bytes.bin";
        auto const longer = binary_removed / "four-bytes.bin";
        {
            std::ofstream out(shorter, std::ios::binary);
            out.write("A\0B", 3);
        }
        {
            std::ofstream out(longer, std::ios::binary);
            out.write("A\0BZ", 4);
        }
        c.expect_file_equal(shorter, longer, "actual file ends first");
        c.expect_file_equal(longer, shorter, "reference file ends first");
        c.expect_file_signature(shorter, std::string_view("A\0B", 3),
                                "embedded NUL signature matches");
        c.expect_file_signature(shorter, std::string_view("A\0BZ", 4),
                                "signature longer than file fails");
        c.expect_file_equal(shorter, shorter, "binary file equals itself");
    });
    s.add("directories are not binary files", [](Context& c) {
        auto const directory = c.work_directory();
        auto const empty = directory / "empty.bin";
        { std::ofstream output(empty, std::ios::binary); }
        c.expect_file_equal(directory, directory, "two directories cannot compare equal");
        c.expect_file_equal(directory, empty, "directory cannot be actual binary file");
        c.expect_file_equal(empty, directory, "directory cannot be reference binary file");
        c.expect_file_signature(directory, "", "directory cannot match an empty signature");
        c.expect_file_signature(empty, "", "empty regular file matches empty signature");
        c.expect_file_equal(empty, empty, "empty regular file equals itself");
    });
    Capture r; auto sum=s.run(r);
    bool ok = true;
    auto verify = [&](bool condition, std::string const& description) {
        if (!condition) {
            std::cerr << "test_artifacts: " << description << '\n';
            ok = false;
        }
    };
    verify(sum.passed == 5 && sum.failed == 6 && sum.inconclusive == 0 &&
           sum.skipped == 0 && sum.errors == 0,
           "unexpected suite summary: passed=" + std::to_string(sum.passed) +
           ", failed=" + std::to_string(sum.failed) +
           ", inconclusive=" + std::to_string(sum.inconclusive) +
           ", skipped=" + std::to_string(sum.skipped) +
           ", errors=" + std::to_string(sum.errors));
    verify(!removed.empty() && !retained.empty() && removed != retained,
           "case work directories must be distinct");
    verify(!std::filesystem::exists(removed), "successful case directory not removed");
    verify(std::filesystem::exists(retained), "failure artifacts not retained");
    verify(std::filesystem::exists(reference), "external golden reference disappeared");
    verify(r.cases.size() == 11, "unexpected test case count");
    if (r.cases.size() == 11) {
        verify(r.cases[0].outcome == Outcome::Passed, "success-case outcome changed");
        verify(r.cases[1].outcome == Outcome::Failed, "golden-regression case must fail");
        verify(r.cases[2].outcome == Outcome::Failed, "missing file must be Failed, not Error");
        verify(r.cases[2].assertions.size() == 4, "missing-file case assertion count");
        for (auto const& assertion : r.cases[2].assertions) {
            verify(assertion.outcome == Outcome::Failed,
                   "missing or non-regular file check must be Failed");
        }
        verify(r.cases[1].artifacts.size() == 2, "expected two artifact references");
        verify(!r.cases[1].assertions.empty(), "missing golden comparison assertion");
        if (!r.cases[1].assertions.empty())
            verify(r.cases[1].assertions[0].detail.find("offset=2") != std::string::npos,
                   "first differing byte offset must be 2");
    }
    verify(!equal_removed.empty() && !std::filesystem::exists(equal_removed),
           "identical-file case directory should be cleaned");
    verify(!invalid_removed.empty() && !std::filesystem::exists(invalid_removed),
           "invalid-artifact case directory should be cleaned");
    verify(!conditional_removed.empty() && !std::filesystem::exists(conditional_removed),
           "KeepOnFailure must not retain output from a successful case");
    verify(!always_retained.empty() && std::filesystem::exists(always_retained / "keep.bin"),
           "KeepAlways must retain output from a successful case");
    if (r.cases.size() == 11) {
        verify(r.cases[3].outcome == Outcome::Passed &&
               r.cases[3].assertions.size() == 1 &&
               r.cases[3].assertions[0].outcome == Outcome::Passed,
               "identical binary files must compare equal");
        verify(r.cases[4].outcome == Outcome::Passed &&
               r.cases[4].assertions.size() == 4 &&
               r.cases[4].artifacts.empty(),
               "all invalid artifact registrations must be rejected without side effects");
        for (auto const& assertion : r.cases[4].assertions)
            verify(assertion.outcome == Outcome::Passed,
                   "invalid artifact registration did not reject the input");
        verify(r.cases[5].outcome == Outcome::Passed &&
               r.cases[5].artifacts.size() == 1 &&
               r.cases[5].artifacts[0].metadata.size() == 1 &&
               r.cases[5].artifacts[0].metadata[0].first == "generator" &&
               r.cases[5].artifacts[0].metadata[0].second == "test_artifacts",
               "KeepAlways artifact metadata or outcome changed");
        verify(r.cases[6].outcome == Outcome::Passed &&
               r.cases[6].artifacts.size() == 1,
               "KeepOnFailure output must be registered on passing case");
        verify(r.cases[7].outcome == Outcome::Failed &&
               r.cases[7].assertions.size() == 2,
               "missing reference and signature must fail");
        if (r.cases[7].assertions.size() == 2) {
            verify(r.cases[7].assertions[0].outcome == Outcome::Failed &&
                   r.cases[7].assertions[0].detail == "cannot open reference file",
                   "reference-file open failure diagnostic changed");
            verify(r.cases[7].assertions[1].outcome == Outcome::Failed &&
                   r.cases[7].assertions[1].detail == "cannot open actual file",
                   "signature-file open failure diagnostic changed");
        }
    }
    verify(!not_directory_removed.empty() &&
           !std::filesystem::exists(not_directory_removed),
           "file-as-parent case directory should be cleaned");
    verify(!binary_removed.empty() && !std::filesystem::exists(binary_removed),
           "binary prefix case directory should be cleaned");
    if (r.cases.size() == 11) {
        auto const& invalid_parent = r.cases[8];
        verify(invalid_parent.outcome == Outcome::Failed &&
               invalid_parent.assertions.size() == 1 &&
               invalid_parent.assertions[0].outcome == Outcome::Failed &&
               invalid_parent.assertions[0].detail == "file absent or not regular",
               "ENOTDIR must be a missing-file failure, not an infrastructure error");
        auto const& binary = r.cases[9];
        verify(binary.outcome == Outcome::Failed && binary.assertions.size() == 5,
               "unexpected binary prefix case classification");
        if (binary.assertions.size() == 5) {
            verify(binary.assertions[0].outcome == Outcome::Failed &&
                   binary.assertions[0].detail == "first differing byte offset=3",
                   "actual file ending first must report offset 3");
            verify(binary.assertions[1].outcome == Outcome::Failed &&
                   binary.assertions[1].detail == "first differing byte offset=3",
                   "reference file ending first must report offset 3");
            verify(binary.assertions[2].outcome == Outcome::Passed,
                   "binary signature containing NUL should match");
            verify(binary.assertions[3].outcome == Outcome::Failed &&
                   binary.assertions[3].detail == "signature mismatch",
                   "a signature longer than the file must fail");
            verify(binary.assertions[4].outcome == Outcome::Passed,
                   "binary file must equal itself");
        }
    }
    if (r.cases.size() == 11) {
        auto const& directory = r.cases[10];
        verify(directory.outcome == Outcome::Failed && directory.assertions.size() == 6,
               "directory binary-check summary changed");
        if (directory.assertions.size() == 6) {
            for (std::size_t i = 0; i < 4; ++i)
                verify(directory.assertions[i].outcome == Outcome::Failed &&
                       (directory.assertions[i].detail.find("not a regular file") != std::string::npos ||
                        directory.assertions[i].detail.find("cannot open") != std::string::npos),
                       "a directory was accepted as a binary file");
            verify(directory.assertions[4].outcome == Outcome::Passed &&
                   directory.assertions[5].outcome == Outcome::Passed,
                   "an empty regular file was rejected");
        }
    }
    std::error_code error;
    std::filesystem::remove_all(always_retained, error);
    if (error) {
        verify(false, "could not clean KeepAlways fixture: " + error.message());
        error.clear();
    }
    std::filesystem::remove_all(retained,error);
    std::filesystem::remove(reference,error);
#if !defined(_WIN32)
    // Managed artifacts must not escape through an in-tree symbolic link.
    // The canonical target, not the lexical prefix, determines ownership.
    std::filesystem::path external_directory;
    Suite managed_symlinks("managed artifact canonical containment");
    managed_symlinks.add("reject symlink escape, accept nested path", [&](Context& c) {
        auto const root = c.work_directory();
        external_directory = std::filesystem::temp_directory_path() /
                             ("ariadne-external-" + root.filename().string());
        if (!std::filesystem::create_directory(external_directory))
            throw std::runtime_error("cannot reserve an external fixture directory");
        std::filesystem::create_directory_symlink(
            external_directory, root / "external-link");
        bool rejected = false;
        try {
            c.artifact(root / "external-link" / "escaped.bin", "outside",
                       "application/octet-stream", ArtifactKind::Output,
                       ArtifactLifetime::KeepAlways);
        } catch (std::invalid_argument const&) {
            rejected = true;
        }
        c.prove(rejected, "managed output through symlink must be rejected");
        c.prove(std::filesystem::is_directory(external_directory),
                "external directory must remain untouched");
        auto const internal = root / "nested";
        std::filesystem::create_directory(internal);
        c.artifact(internal / "accepted.bin", "inside",
                   "application/octet-stream", ArtifactKind::Output,
                   ArtifactLifetime::Temporary);
        c.prove(true, "nested managed output is accepted");
    });
    Capture symlinks_capture;
    auto const symlinks_summary = managed_symlinks.run(symlinks_capture);
    verify(symlinks_summary.passed == 1 && symlinks_summary.errors == 0 &&
           symlinks_capture.cases.size() == 1,
           "managed symlink containment outcome changed");
    if (symlinks_capture.cases.size() == 1) {
        auto const& result = symlinks_capture.cases[0];
        verify(result.assertions.size() == 3 && result.artifacts.size() == 1,
               "rejected symlink output was registered");
        for (auto const& assertion : result.assertions)
            verify(assertion.outcome == Outcome::Passed,
                   "managed symlink containment assertion failed");
    }
    std::error_code symlink_cleanup_error;
    std::filesystem::remove_all(external_directory, symlink_cleanup_error);
    verify(!symlink_cleanup_error, "external fixture cleanup failed");
#endif

    return ok ? 0 : 1;
}
