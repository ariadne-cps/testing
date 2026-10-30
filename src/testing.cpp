/***************************************************************************
 *            testing.cpp
 *
 *  Copyright  2026  Luca Geretti
 *  Author     Luca Geretti
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


namespace Ariadne {
namespace {

std::string_view outcome_name(Outcome result) {
    switch (result) {
        case Outcome::Passed: return "PASS";
        case Outcome::Failed: return "FAIL";
        case Outcome::Inconclusive: return "INCONCLUSIVE";
        case Outcome::Skipped: return "SKIP";
        case Outcome::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

int priority(Outcome result) {
    switch (result) {
        case Outcome::Passed: return 0;
        case Outcome::Skipped: return 1;
        case Outcome::Inconclusive: return 2;
        case Outcome::Failed: return 3;
        case Outcome::Error: return 4;
        default: return 4;
    }
}

std::string summary_text(std::string_view name, Summary const& summary) {
    std::ostringstream os;
    os << name << ": " << summary.passed << " passed, " << summary.failed << " failed, "
       << summary.inconclusive << " inconclusive, " << summary.skipped << " skipped, "
       << summary.errors << " errors";
    return os.str();
}

} // namespace

ConsoleReporter::ConsoleReporter(std::ostream& output, ConsoleDetail detail)
    : _output(output), _detail(detail) { }

void ConsoleReporter::diagnostic_emitted(CaseResult const& result, DiagnosticEvent const& event) {
    // Scopes and warnings are always flushed for hang diagnosis and safety.
    if (_detail == ConsoleDetail::Concise &&
        event.kind != DiagnosticKind::Scope && event.kind != DiagnosticKind::Warning) return;
    switch (event.kind) {
        case DiagnosticKind::Scope:
            _output << "[SCOPE " << result.case_index << "] ";
            break;
        case DiagnosticKind::Note: _output << "[NOTE] "; break;
        case DiagnosticKind::Warning: _output << "[WARN] "; break;
        case DiagnosticKind::Value: _output << "[VALUE] "; break;
        case DiagnosticKind::Elapsed: _output << "[TIME] "; break;
        default: _output << "[INFO] "; break;
    }
    if (!event.context.empty()) {
        for (std::size_t i = 0; i < event.context.size(); ++i) {
            if (i != 0) _output << " / ";
            _output << event.context[i];
        }
        if (event.kind != DiagnosticKind::Scope) _output << " / ";
    }
    if (event.kind != DiagnosticKind::Scope) _output << event.description;
    if (event.kind == DiagnosticKind::Value) _output << " = " << event.value;
    if (event.kind == DiagnosticKind::Elapsed) _output << " = " << event.duration.count() << " ns";
    _output << '\n' << std::flush;
}

void ConsoleReporter::case_started(CaseResult const& result) {
    _output << "[RUN " << result.case_index << '/' << result.case_total << "] "
            << result.suite_name << " / " << result.name << '\n' << std::flush;
}

void ConsoleReporter::check_started(CaseResult const& result, std::size_t index,
                                    std::string_view description) {
    _output << "[CHECK " << result.case_index << '.' << index << "] "
            << description << '\n' << std::flush;
}

void ConsoleReporter::case_finished(CaseResult const& result) {
    _output << '[' << outcome_name(result.outcome) << "] " << result.name << '\n';
    if (result.outcome == Outcome::Passed) return;
    // In concise mode successful notes are hidden, but restored for a failing case.
    if (_detail == ConsoleDetail::Concise) {
        for (auto const& event : result.diagnostics) {
            if (event.kind == DiagnosticKind::Scope || event.kind == DiagnosticKind::Warning)
                continue; // Already printed while executing.
            _output << "  diagnostic: " << event.description;
            if (event.kind == DiagnosticKind::Value) _output << " = " << event.value;
            if (event.kind == DiagnosticKind::Elapsed) _output << " = " << event.duration.count() << " ns";
            _output << '\n';
        }
    }
    for (auto const& assertion : result.assertions) {
        if (assertion.outcome == Outcome::Passed) continue;
        _output << "  " << outcome_name(assertion.outcome) << ": " << assertion.description << '\n';
        if (!assertion.context.empty()) {
            _output << "    context: ";
            for (std::size_t i = 0; i < assertion.context.size(); ++i) {
                if (i != 0) _output << " / ";
                _output << assertion.context[i];
            }
            _output << '\n';
        }
        if (!assertion.actual.empty()) _output << "    actual: " << assertion.actual << '\n';
        if (!assertion.expected.empty()) _output << "    expected: " << assertion.expected << '\n';
        if (!assertion.detail.empty()) _output << "    detail: " << assertion.detail << '\n';
        if (!assertion.file.empty()) _output << "    at " << assertion.file << ':' << assertion.line << '\n';
    }
}

void ConsoleReporter::suite_finished(std::string_view name, Summary const& summary) {
    _output << summary_text(name,summary) << '\n';
}

Context::~Context() noexcept {
    if (!_work_directory) return;
    bool preserve = false;
    for (auto const& artifact : _result.artifacts) {
        if (artifact.lifetime == ArtifactLifetime::KeepAlways ||
            (artifact.lifetime == ArtifactLifetime::KeepOnFailure &&
             _result.outcome != Outcome::Passed)) preserve = true;
    }
    if (!preserve) {
        std::error_code error;
        std::filesystem::remove_all(*_work_directory, error);
    }
}

std::filesystem::path const& Context::work_directory() {
    if (!_work_directory) {
        auto const base = std::filesystem::temp_directory_path();
        // A stack address is reused for successive test cases. Use a new
        // candidate ID for each allocation, with create_directory as the
        // atomic uniqueness check across concurrent processes.
        static std::atomic<std::uint64_t> next_id{0};
        for (unsigned i=0; i<1000; ++i) {
            auto const id = next_id.fetch_add(1, std::memory_order_relaxed);
            auto candidate = base / ("ariadne-case-" + std::to_string(id));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                _work_directory = std::move(candidate);
                break;
            }
            if (error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error("create work directory",candidate,error);
            }
        }
        if (!_work_directory) throw std::runtime_error("Cannot allocate isolated test directory");
    }
    return *_work_directory;
}

void Context::artifact(std::filesystem::path path, std::string label,
                       std::string media_type, ArtifactKind kind,
                       ArtifactLifetime lifetime,
                       std::vector<std::pair<std::string,std::string>> metadata) {
    if (path.empty() || label.empty())
        throw std::invalid_argument("Artifact path and label must not be empty");
    // Retention applies only to outputs owned by this Context.
    if (lifetime != ArtifactLifetime::External) {
        auto const root = std::filesystem::weakly_canonical(work_directory());
        auto const target = std::filesystem::weakly_canonical(path);
        auto relative = target.lexically_relative(root);
        if (relative.empty() || relative == "." ||
            *relative.begin() == "..")
            throw std::invalid_argument("Managed artifacts must be inside the case directory");
    }
    _result.artifacts.push_back({std::move(path),std::move(label),
        std::move(media_type),kind,lifetime,std::move(metadata)});
}

void Context::expect_file_exists(std::filesystem::path const& path,
                                 std::string_view description,
                                 std::source_location location) {
    auto const index=begin_check(description);
    std::error_code error;
    bool const exists=std::filesystem::is_regular_file(path,error);
    // Some standard libraries report ENOENT/ENOTDIR as an error_code;
    // these still mean an absent regular file, not a testing infrastructure
    // error. Preserve Error for other filesystem failures.
    bool const absent=error == std::errc::no_such_file_or_directory ||
                      error == std::errc::not_a_directory;
    Outcome const status=error && !absent ? Outcome::Error :
                         exists ? Outcome::Passed : Outcome::Failed;
    record(status,description,path.string(),"regular file",
           error && !absent ? error.message() :
           exists ? "" : "file absent or not regular",location,index);
}

void Context::expect_file_equal(std::filesystem::path const& actual,
                                std::filesystem::path const& reference,
                                std::string_view description,
                                std::source_location location) {
    auto const index=begin_check(description);
    std::ifstream a(actual,std::ios::binary);
    std::ifstream b(reference,std::ios::binary);
    if (!a || !b) {
        record(Outcome::Failed,description,actual.string(),reference.string(),
               !a ? "cannot open actual file" : "cannot open reference file",location,index);
        return;
    }
    std::size_t offset=0;
    for (;;) {
        int const x=a.get(),y=b.get();
        if (x!=y) {
            record(Outcome::Failed,description,actual.string(),reference.string(),
                   "first differing byte offset="+std::to_string(offset),location,index);
            return;
        }
        if (x==std::char_traits<char>::eof()) break;
        ++offset;
    }
    record(Outcome::Passed,description,actual.string(),reference.string(),{},location,index);
}

void Context::expect_file_signature(std::filesystem::path const& actual,
                                    std::string_view signature,
                                    std::string_view description,
                                    std::source_location location) {
    auto const index=begin_check(description);
    std::ifstream stream(actual,std::ios::binary);
    if (!stream) {
        record(Outcome::Failed,description,actual.string(),std::string(signature),
               "cannot open actual file",location,index);
        return;
    }
    std::string found(signature.size(),'\0');
    stream.read(found.data(),static_cast<std::streamsize>(found.size()));
    found.resize(static_cast<std::size_t>(stream.gcount()));
    bool const equal=found==signature;
    record(equal ? Outcome::Passed : Outcome::Failed,description,actual.string(),
           "binary signature",equal ? "" : "signature mismatch",location,index);
}

Context::Scope::Scope(Context& context, std::string label, std::source_location location)
    : _context(&context), _depth(context.enter_scope(std::move(label), location)),
      _uncaught(std::uncaught_exceptions()) { }

Context::Scope::Scope(Scope&& other) noexcept
    : _context(std::exchange(other._context, nullptr)),
      _depth(other._depth), _uncaught(other._uncaught) { }

Context::Scope::~Scope() noexcept {
    if (_context) _context->leave_scope(_depth, _uncaught);
}

Context::Scope Context::scope(std::string label, std::source_location location) {
    return Scope(*this, std::move(label), location);
}

std::size_t Context::enter_scope(std::string label, std::source_location location) {
    if (label.empty()) throw std::invalid_argument("Diagnostic scope name must be nonempty");
    auto const depth = _scopes.size();
    _scopes.push_back(std::move(label));
    emit(DiagnosticKind::Scope, _scopes.back(), {}, {}, location);
    return depth;
}

void Context::leave_scope(std::size_t depth, int uncaught_at_entry) noexcept {
    if (std::uncaught_exceptions() > uncaught_at_entry &&
        _scopes.size() > _unwinding_scopes.size()) {
        try { _unwinding_scopes = _scopes; }
        catch (...) { } // RAII destructor must never throw.
    }
    if (depth <= _scopes.size()) _scopes.resize(depth);
}

void Context::emit(DiagnosticKind kind, std::string description, std::string value,
                   std::chrono::nanoseconds duration, std::source_location location) {
    DiagnosticEvent event{kind, std::move(description), std::move(value),
                          _scopes, duration, location.file_name(), location.line()};
    _result.diagnostics.push_back(std::move(event));
    if (_reporter) _reporter->diagnostic_emitted(_result, _result.diagnostics.back());
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
        case Evidence::Proven: return Outcome::Passed;
        case Evidence::Refuted: return Outcome::Failed;
        case Evidence::Indeterminate: return Outcome::Inconclusive;
        default: return Outcome::Error;
    }
}

std::size_t Context::begin_check(std::string_view description) {
    auto const index = ++_check_count;
    _unwinding_scopes.clear(); // A new check supersedes any recovered exception context.
    if (_reporter) _reporter->check_started(_result, index, description);
    return index;
}

void Context::record(Outcome status, std::string_view description,
                     std::string actual, std::string expected, std::string detail,
                     std::source_location location, std::size_t index) {
    _result.assertions.push_back({status, std::string(description), std::move(actual),
        std::move(expected), std::move(detail), location.file_name(), location.line(),
        index, !_scopes.empty() ? _scopes : _unwinding_scopes});
    if (priority(status) > priority(_result.outcome)) _result.outcome = status;
}

void Context::skip(std::string_view reason, std::source_location location) {
    auto const index = begin_check(reason);
    record(Outcome::Skipped, reason, {}, {}, {}, location, index);
}

void Suite::add(std::string name, std::function<void(Context&)> test) {
    if (name.empty() || !test) throw std::invalid_argument("Test name and body must be nonempty");
    _tests.push_back({std::move(name),std::move(test)});
}

Summary Suite::run(Reporter& reporter) const {
    Summary summary;
    for (std::size_t i = 0; i < _tests.size(); ++i) {
        auto const& test = _tests[i];
        CaseResult result;
        result.name = test.name;
        result.suite_name = _name;
        result.case_index = i + 1;
        result.case_total = _tests.size();
        Context context(result, &reporter);
        reporter.case_started(result);
        auto const started = std::chrono::steady_clock::now();
        try {
            test.run(context);
        } catch (detail::AbortCase const&) {
            // A failed requirement is already represented by its assertion.
        } catch (std::exception const& exception) {
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
            case Outcome::Passed: ++summary.passed; break;
            case Outcome::Failed: ++summary.failed; break;
            case Outcome::Inconclusive: ++summary.inconclusive; break;
            case Outcome::Skipped: ++summary.skipped; break;
            case Outcome::Error: ++summary.errors; break;
            default:
                ++summary.errors;
                result.outcome = Outcome::Error;
                break;
        }
        reporter.case_finished(result);
    }
    reporter.suite_finished(_name, summary);
    return summary;
}

} // namespace Ariadne
