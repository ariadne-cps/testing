/***************************************************************************
 *            testing.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Testing.
 *
 *  Ariadne Testing is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Testing is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Testing. If not, see <https://www.gnu.org/licenses/>.
 */

#include "testing/testing.hpp"

#include <atomic>
#include <exception>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace Ariadne::Testing {
namespace {

int priority(Outcome result) {
    switch (result) {
        case Outcome::Passed:
            return 0;
        case Outcome::Skipped:
            return 1;
        case Outcome::Inconclusive:
            return 2;
        case Outcome::Failed:
            return 3;
        case Outcome::Error:
            return 4;
        default:
            return 4;
    }
}

} // namespace

Context::~Context() noexcept {
    if (!_work_directory)
        return;
    bool preserve = false;
    for (const auto& artifact: _result.artifacts) {
        if (artifact.lifetime == ArtifactLifetime::KeepAlways ||
            (artifact.lifetime == ArtifactLifetime::KeepOnFailure &&
             _result.outcome != Outcome::Passed))
            preserve = true;
    }
    if (!preserve) {
        std::error_code error;
        std::filesystem::remove_all(*_work_directory, error);
    }
}

const std::filesystem::path& Context::work_directory() {
    if (!_work_directory) {
        const auto base = std::filesystem::temp_directory_path();
        // A stack address is reused for successive test cases. Use a new
        // candidate ID for each allocation, with create_directory as the
        // atomic uniqueness check across concurrent processes.
        // Distinct names across concurrent CTest executables.
#if defined(_WIN32)
        const auto process_id = std::to_string(_getpid());
#else
        const auto process_id = std::to_string(getpid());
#endif
        static std::atomic<std::uint64_t> next_id{0};
        for (unsigned i = 0; i < 1000; ++i) {
            const auto id = next_id.fetch_add(1, std::memory_order_relaxed);
            auto candidate = base / ("ariadne-case-" + process_id + "-" + std::to_string(id));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                _work_directory = std::move(candidate);
                break;
            }
            if (error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error("create work directory", candidate, error);
            }
        }
        if (!_work_directory)
            throw std::runtime_error("Cannot allocate isolated test directory");
    }
    return *_work_directory;
}

namespace {
bool valid_oracle_id(const OracleRow& row) {
    return row.parsed && !row.id.key.empty();
}
struct OracleRowLess {
    bool operator()(const OracleRowId& a, const OracleRowId& b) const {
        return std::tie(a.key, a.iteration, a.repetition) <
               std::tie(b.key, b.iteration, b.repetition);
    }
};
const char* oracle_issue_name(OracleIssueKind kind) {
    switch (kind) {
        case OracleIssueKind::InvalidManifest:
            return "invalid manifest";
        case OracleIssueKind::Malformed:
            return "malformed row";
        case OracleIssueKind::Duplicate:
            return "duplicate row";
        case OracleIssueKind::Missing:
            return "missing row";
        case OracleIssueKind::Extra:
            return "extra row";
        case OracleIssueKind::OutOfOrder:
            return "out-of-order row";
        default:
            return "unknown oracle issue";
    }
}
} // namespace

OracleIntegrity validate_oracle_rows(std::span<const OracleRow> manifest,
                                     std::span<const OracleRow> observed) {
    OracleIntegrity report{manifest.size(), observed.size(), {}};
    std::map<OracleRowId, std::size_t, OracleRowLess> required;
    if (manifest.empty())
        report.issues.push_back({OracleIssueKind::InvalidManifest, 0, {}});
    for (std::size_t i = 0; i < manifest.size(); ++i) {
        const auto& row = manifest[i];
        if (!valid_oracle_id(row) || !required.emplace(row.id, i).second)
            report.issues.push_back({OracleIssueKind::InvalidManifest, i, row.id});
    }
    if (report.invalid_manifest())
        return report;
    std::map<OracleRowId, std::size_t, OracleRowLess> found;
    for (std::size_t i = 0; i < observed.size(); ++i) {
        const auto& row = observed[i];
        if (!valid_oracle_id(row)) {
            report.issues.push_back({OracleIssueKind::Malformed, i, row.id});
            continue;
        }
        if (!found.emplace(row.id, i).second)
            report.issues.push_back({OracleIssueKind::Duplicate, i, row.id});
        if (!required.contains(row.id))
            report.issues.push_back({OracleIssueKind::Extra, i, row.id});
    }
    for (const auto& [id, i]: required)
        if (!found.contains(id))
            report.issues.push_back({OracleIssueKind::Missing, i, id});

    // Report ordering only if the exact multiset agrees: otherwise the
    // earlier missing/duplicate/extra/malformed issue is more actionable.
    if (report.issues.empty())
        for (std::size_t i = 0; i < manifest.size(); ++i)
            if (!(manifest[i].id == observed[i].id)) {
                report.issues.push_back({OracleIssueKind::OutOfOrder, i, observed[i].id});
                break;
            }
    return report;
}

std::string OracleIntegrity::description() const {
    auto summary = "expected_rows=" + std::to_string(expected_rows) +
                   "; observed_rows=" + std::to_string(observed_rows);
    if (issues.empty())
        return summary + "; integrity verified";
    const auto& first = issues.front();
    return summary + "; issues=" + std::to_string(issues.size()) +
           "; first=" + oracle_issue_name(first.kind) +
           " at index=" + std::to_string(first.index) +
           "; key=" + first.id.key +
           "; iteration=" + std::to_string(first.id.iteration) +
           "; repetition=" + std::to_string(first.id.repetition);
}

OracleReplayPlan plan_oracle_replay(std::span<const OracleRow> manifest,
                                    std::span<const OracleRowId> requested) {
    OracleReplayPlan plan;
    plan.total_rows = manifest.size();

    // The manifest must itself be a valid and complete independent frozen
    // reference. Never infer it from the observed implementation's output.
    const auto integrity = validate_oracle_rows(manifest, manifest);
    if (!integrity.valid()) {
        plan.valid = false;
        plan.reason = "invalid frozen manifest: " + integrity.description();
        return plan;
    }

    std::map<OracleRowId, std::size_t, OracleRowLess> known;
    for (std::size_t i = 0; i < manifest.size(); ++i)
        known.emplace(manifest[i].id, i);

    if (requested.empty()) {
        for (std::size_t i = 0; i < manifest.size(); ++i)
            plan.indices.push_back(i);
        plan.complete = true;
        return plan;
    }

    std::map<OracleRowId, bool, OracleRowLess> selected;
    for (const auto& id: requested) {
        if (id.key.empty()) {
            plan.valid = false;
            plan.reason = "selected row has an empty key";
            return plan;
        }
        if (!selected.emplace(id, true).second) {
            plan.valid = false;
            plan.reason = "duplicate selected row: " + id.key;
            return plan;
        }
        if (!known.contains(id)) {
            plan.valid = false;
            plan.reason = "unknown selected row: " + id.key;
            return plan;
        }
    }

    for (std::size_t i = 0; i < manifest.size(); ++i)
        if (selected.contains(manifest[i].id))
            plan.indices.push_back(i);
    plan.complete = plan.indices.size() == manifest.size();
    return plan;
}

std::string OracleReplayPlan::description() const {
    auto summary = "replay_selected=" + std::to_string(indices.size()) +
                   "; manifest_total=" + std::to_string(total_rows);
    if (!valid)
        return summary + "; invalid selection: " + reason;
    if (!structurally_valid())
        return summary + "; invalid replay plan";
    if (!complete)
        return summary + "; partial replay, full coverage not established";
    return summary + "; full replay";
}

void Context::expect_oracle_replay(const OracleReplayPlan& plan,
                                   std::string_view description,
                                   std::source_location location) {
    const auto index = begin_check(description);
    record_oracle_replay(plan, description, location, index);
}

void Context::record_oracle_replay(const OracleReplayPlan& plan,
                                   std::string_view description,
                                   std::source_location location,
                                   std::size_t index) {
    const auto result = !plan.structurally_valid() ? Outcome::Error : plan.complete ? Outcome::Passed
                                                                                    : Outcome::Inconclusive;
    record(result, description, std::to_string(plan.indices.size()),
           std::to_string(plan.total_rows), plan.description(), location, index);
}

void Context::register_oracle(const OracleMetadata& metadata) {
    if (metadata.reference.empty() || metadata.label.empty() ||
        metadata.row_key_column.empty() ||
        (metadata.precision_bits.has_value() && *metadata.precision_bits == std::uint32_t{0}))
        throw std::invalid_argument("Oracle reference, label and row key must be nonempty and precision positive");
    std::vector<std::pair<std::string, std::string>> attrs{
        {"oracle.row_key_column", metadata.row_key_column}
    };
    if (metadata.seed)
        attrs.emplace_back("oracle.seed", std::to_string(*metadata.seed));
    if (metadata.precision_bits)
        attrs.emplace_back("oracle.precision_bits", std::to_string(*metadata.precision_bits));
    std::map<std::string, bool> seen;
    for (const auto& [name, value]: metadata.input_attributes) {
        if (name.empty() || value.empty() || !seen.emplace(name, true).second)
            throw std::invalid_argument("Oracle input attribute names/values must be nonempty and unique");
        attrs.emplace_back("input." + name, value);
    }
    artifact(metadata.reference, metadata.label, "application/octet-stream",
             ArtifactKind::Reference, ArtifactLifetime::External, std::move(attrs));
}

void Context::expect_oracle_rows(std::span<const OracleRow> manifest,
                                 std::span<const OracleRow> observed,
                                 std::string_view description,
                                 std::source_location location) {
    const auto index = begin_check(description);
    record_oracle_rows(manifest, observed, description, location, index);
}

void Context::record_oracle_rows(std::span<const OracleRow> manifest,
                                 std::span<const OracleRow> observed,
                                 std::string_view description,
                                 std::source_location location,
                                 std::size_t index) {
    const auto report = validate_oracle_rows(manifest, observed);
    record(report.valid() ? Outcome::Passed : report.invalid_manifest() ? Outcome::Error
                                                                        : Outcome::Failed,
           description, std::to_string(report.observed_rows),
           std::to_string(report.expected_rows), report.description(),
           location, index);
}

void Context::artifact(std::filesystem::path path, std::string label,
                       std::string media_type, ArtifactKind kind,
                       ArtifactLifetime lifetime,
                       std::vector<std::pair<std::string, std::string>> metadata) {
    if (path.empty() || label.empty())
        throw std::invalid_argument("Artifact path and label must not be empty");
    // Retention applies only to outputs owned by this Context.
    if (lifetime != ArtifactLifetime::External) {
        const auto root = std::filesystem::weakly_canonical(work_directory());
        const auto target = std::filesystem::weakly_canonical(path);
        auto relative = target.lexically_relative(root);
        if (relative.empty() || relative == "." ||
            *relative.begin() == "..")
            throw std::invalid_argument("Managed artifacts must be inside the case directory");
    }
    _result.artifacts.push_back({std::move(path), std::move(label),
                                 std::move(media_type), kind, lifetime, std::move(metadata)});
}

void Context::expect_file_exists(const std::filesystem::path& path,
                                 std::string_view description,
                                 std::source_location location) {
    const auto index = begin_check(description);
    std::error_code error;
    const bool exists = std::filesystem::is_regular_file(path, error);
    // Some standard libraries report ENOENT/ENOTDIR as an error_code;
    // these still mean an absent regular file, not a testing infrastructure
    // error. Preserve Error for other filesystem failures.
    const bool absent = error == std::errc::no_such_file_or_directory ||
                        error == std::errc::not_a_directory;
    const Outcome status = error && !absent ? Outcome::Error : exists ? Outcome::Passed
                                                                      : Outcome::Failed;
    record(status, description, path.string(), "regular file",
           error && !absent ? error.message() : exists ? ""
                                                       : "file absent or not regular",
           location, index);
}

void Context::expect_file_equal(const std::filesystem::path& actual,
                                const std::filesystem::path& reference,
                                std::string_view description,
                                std::source_location location) {
    const auto index = begin_check(description);
    std::ifstream a(actual, std::ios::binary);
    std::ifstream b(reference, std::ios::binary);
    if (!a || !b) {
        record(Outcome::Failed, description, actual.string(), reference.string(),
               !a ? "cannot open actual file" : "cannot open reference file", location, index);
        return;
    }
    // Opening a directory as an input stream succeeds on some platforms.
    // It must not be mistaken for an empty regular file.
    const bool actual_regular = std::filesystem::is_regular_file(actual);
    const bool reference_regular = std::filesystem::is_regular_file(reference);
    if (!actual_regular || !reference_regular) {
        record(Outcome::Failed, description, actual.string(), reference.string(),
               !actual_regular ? "actual path is not a regular file"
                               : "reference path is not a regular file",
               location, index);
        return;
    }
    std::size_t offset = 0;
    for (;;) {
        const int x = a.get(), y = b.get();
        if (a.bad() || b.bad()) {
            record(Outcome::Error, description, actual.string(), reference.string(),
                   a.bad() ? "error reading actual file" : "error reading reference file",
                   location, index);
            return;
        }
        if (x != y) {
            record(Outcome::Failed, description, actual.string(), reference.string(),
                   "first differing byte offset=" + std::to_string(offset), location, index);
            return;
        }
        if (x == std::char_traits<char>::eof())
            break;
        ++offset;
    }
    record(Outcome::Passed, description, actual.string(), reference.string(), {}, location, index);
}

void Context::expect_file_signature(const std::filesystem::path& actual,
                                    std::string_view signature,
                                    std::string_view description,
                                    std::source_location location) {
    const auto index = begin_check(description);
    std::ifstream stream(actual, std::ios::binary);
    if (!stream) {
        record(Outcome::Failed, description, actual.string(), std::string(signature),
               "cannot open actual file", location, index);
        return;
    }
    if (!std::filesystem::is_regular_file(actual)) {
        record(Outcome::Failed, description, actual.string(), std::string(signature),
               "actual path is not a regular file", location, index);
        return;
    }
    std::string found(signature.size(), '\0');
    stream.read(found.data(), static_cast<std::streamsize>(found.size()));
    if (stream.bad()) {
        record(Outcome::Error, description, actual.string(), std::string(signature),
               "error reading actual file", location, index);
        return;
    }
    found.resize(static_cast<std::size_t>(stream.gcount()));
    const bool equal = found == signature;
    record(equal ? Outcome::Passed : Outcome::Failed, description, actual.string(),
           "binary signature", equal ? "" : "signature mismatch", location, index);
}

Context::Scope::Scope(Context& context, std::string label, std::source_location location)
    : _context(&context), _depth(context.enter_scope(std::move(label), location)),
      _uncaught(std::uncaught_exceptions()) {}

Context::Scope::Scope(Scope&& other) noexcept
    : _context(std::exchange(other._context, nullptr)),
      _depth(other._depth), _uncaught(other._uncaught) {}

Context::Scope::~Scope() noexcept {
    if (_context)
        _context->leave_scope(_depth, _uncaught);
}

Context::Scope Context::scope(std::string label, std::source_location location) {
    return Scope(*this, std::move(label), location);
}

std::size_t Context::enter_scope(std::string label, std::source_location location) {
    if (label.empty())
        throw std::invalid_argument("Diagnostic scope name must be nonempty");
    const auto depth = _scopes.size();
    _scopes.push_back(std::move(label));
    emit(DiagnosticKind::Scope, _scopes.back(), {}, {}, location);
    return depth;
}

void Context::leave_scope(std::size_t depth, int uncaught_at_entry) noexcept {
    if (std::uncaught_exceptions() > uncaught_at_entry &&
        _scopes.size() > _unwinding_scopes.size()) {
        try {
            _unwinding_scopes = _scopes;
        } catch (...) {
        } // RAII destructor must never throw.
    }
    if (depth <= _scopes.size())
        _scopes.resize(depth);
}

void Context::emit(DiagnosticKind kind, std::string description, std::string value,
                   std::chrono::nanoseconds duration, std::source_location location) {
    DiagnosticEvent event{kind, std::move(description), std::move(value),
                          _scopes, duration, location.file_name(), location.line()};
    _result.diagnostics.push_back(std::move(event));
    if (_reporter)
        _reporter->diagnostic_emitted(_result, _result.diagnostics.back());
}

bool Context::capture_passing_values() const noexcept {
    // A null reporter retains the old full snapshot contract.
    return !_reporter || _reporter->capture_passing_values();
}

void Context::note(std::string description, std::source_location location) {
    emit(DiagnosticKind::Note, std::move(description), {}, {}, location);
}

void Context::warning(std::string description, std::source_location location) {
    emit(DiagnosticKind::Warning, std::move(description), {}, {}, location);
}

void Context::elapsed(std::string description, std::chrono::nanoseconds duration,
                      std::source_location location) {
    emit(DiagnosticKind::Elapsed, std::move(description), {}, duration, location);
}

Outcome Context::outcome(Evidence value) {
    switch (value) {
        case Evidence::True:
            return Outcome::Passed;
        case Evidence::False:
            return Outcome::Failed;
        case Evidence::Indeterminate:
            return Outcome::Inconclusive;
        default:
            return Outcome::Error;
    }
}

std::size_t Context::begin_check(std::string_view description) {
    const auto index = ++_check_count;
    _unwinding_scopes.clear(); // A new check supersedes any recovered exception context.
    if (_reporter)
        _reporter->check_started(_result, index, description);
    return index;
}

void Context::record(Outcome status, std::string_view description,
                     std::string actual, std::string expected, std::string detail,
                     std::source_location location, std::size_t index,
                     std::optional<WaitObservation> wait) {
    _result.assertions.push_back({status, std::string(description), std::move(actual),
                                  std::move(expected), std::move(detail), location.file_name(), location.line(),
                                  index, !_scopes.empty() ? _scopes : _unwinding_scopes, std::move(wait)});
    if (priority(status) > priority(_result.outcome))
        _result.outcome = status;
}

void Context::skip(std::string_view reason, std::source_location location) {
    const auto index = begin_check(reason);
    record(Outcome::Skipped, reason, {}, {}, {}, location, index);
}

void Suite::add(std::string name, std::function<void(Context&)> test,
                CaseMetadata metadata) {
    if (name.empty() || !test)
        throw std::invalid_argument("Test name and body must be nonempty");
    for (std::size_t i = 0; i < metadata.tags.size(); ++i) {
        if (metadata.tags[i].empty())
            throw std::invalid_argument("Case tag must be nonempty");
        for (std::size_t j = 0; j < i; ++j)
            if (metadata.tags[j] == metadata.tags[i])
                throw std::invalid_argument("Case tags must be unique");
    }
    _tests.push_back({std::move(name), std::move(test), std::move(metadata)});
}

Summary Suite::run(Reporter& reporter) const {
    return run_selected(reporter, {}).summary;
}

SelectionResult Suite::run_selected(Reporter& reporter, const CaseFilter& filter) const {
    for (const auto& tag: filter.any_tags)
        if (tag.empty())
            throw std::invalid_argument("Filter tags must be nonempty");
    SelectionResult output;
    output.selection.registered = _tests.size();
    output.selection.filter = filter;

    auto matches = [&](const TestCase& test) {
        if (!filter.name_contains.empty() &&
            test.name.find(filter.name_contains) == std::string::npos)
            return false;
        if (!filter.implementation.empty() &&
            test.metadata.implementation != filter.implementation)
            return false;
        if (!filter.any_tags.empty()) {
            bool matched = false;
            for (const auto& tag: filter.any_tags)
                if (std::find(test.metadata.tags.begin(), test.metadata.tags.end(), tag) != test.metadata.tags.end())
                    matched = true;
            if (!matched)
                return false;
        }
        return true;
    };
    for (const auto& test: _tests)
        if (matches(test))
            ++output.selection.selected;

    // Filter notices are streamed before any selected body, never after.
    if (filter.active())
        reporter.selection_started(output.selection);
    for (std::size_t i = 0; i < _tests.size(); ++i) {
        const auto& test = _tests[i];
        if (!matches(test))
            continue;
        CaseResult result;
        result.name = test.name;
        result.suite_name = _name;
        result.case_index = i + 1;
        result.case_total = _tests.size();
        result.tags = test.metadata.tags;
        result.implementation = test.metadata.implementation;
        Context context(result, &reporter);
        reporter.case_started(result);
        const auto started = std::chrono::steady_clock::now();
        try {
            test.run(context);
        } catch (const Detail::AbortCase&) {
            // A failed requirement is already represented by its assertion.
        } catch (const std::exception& exception) {
            context.record(Outcome::Error, "Unhandled std::exception", {}, {}, exception.what(),
                           std::source_location::current(), 0);
        } catch (...) {
            context.record(Outcome::Error, "Unhandled non-standard exception", {}, {}, {},
                           std::source_location::current(), 0);
        }
        result.duration = std::chrono::steady_clock::now() - started;
        if (result.assertions.empty()) {
            context.record(Outcome::Skipped, "No assertions recorded", {}, {}, {},
                           std::source_location::current(), 0);
        }
        switch (result.outcome) {
            case Outcome::Passed:
                ++output.summary.passed;
                break;
            case Outcome::Failed:
                ++output.summary.failed;
                break;
            case Outcome::Inconclusive:
                ++output.summary.inconclusive;
                break;
            case Outcome::Skipped:
                ++output.summary.skipped;
                break;
            case Outcome::Error:
                ++output.summary.errors;
                break;
            default:
                ++output.summary.errors;
                result.outcome = Outcome::Error;
                break;
        }
        reporter.case_finished(result);
    }
    reporter.suite_finished(_name, output.summary);
    return output;
}

} // namespace Ariadne::Testing
