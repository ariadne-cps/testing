/***************************************************************************
 *            reporters.cpp
 *
 *  Copyright  2026  Luca Geretti
 ****************************************************************************/
/*
 *  This file is part of Ariadne Testing, distributed under GPLv3 or later.
 *  Ariadne Testing comes WITHOUT ANY WARRANTY.
 */
#include "testing/reporters.hpp"

#include <chrono>
#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <climits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Ariadne::Testing {
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

std::string summary_text(std::string_view name, Summary const& summary) {
    std::ostringstream os;
    os << name << ": " << summary.passed << " passed, " << summary.failed << " failed, "
       << summary.inconclusive << " inconclusive, " << summary.skipped << " skipped, "
       << summary.errors << " errors";
    return os.str();
}


char const* outcome_string(Outcome status) {
    switch (status) {
        case Outcome::Passed: return "passed";
        case Outcome::Failed: return "failed";
        case Outcome::Inconclusive: return "inconclusive";
        case Outcome::Skipped: return "skipped";
        case Outcome::Error: return "error";
        default: return "error";
    }
}
char const* diagnostic_string(DiagnosticKind kind) {
    switch (kind) {
        case DiagnosticKind::Scope: return "scope";
        case DiagnosticKind::Note: return "note";
        case DiagnosticKind::Warning: return "warning";
        case DiagnosticKind::Value: return "value";
        case DiagnosticKind::Elapsed: return "elapsed";
        default: return "unknown";
    }
}
char const* wait_string(WaitStatus status) {
    switch (status) {
        case WaitStatus::Ready: return "ready";
        case WaitStatus::Timeout: return "timeout";
        case WaitStatus::Deferred: return "deferred";
        case WaitStatus::InvalidTimeout: return "invalid_timeout";
        case WaitStatus::Unexpected: return "unexpected";
        default: return "unexpected";
    }
}
char const* artifact_kind(ArtifactKind kind) {
    switch (kind) {
        case ArtifactKind::Output: return "output";
        case ArtifactKind::Reference: return "reference";
        case ArtifactKind::Difference: return "difference";
        default: return "unknown";
    }
}
char const* artifact_lifetime(ArtifactLifetime lifetime) {
    switch (lifetime) {
        case ArtifactLifetime::External: return "external";
        case ArtifactLifetime::Temporary: return "temporary";
        case ArtifactLifetime::KeepOnFailure: return "keep_on_failure";
        case ArtifactLifetime::KeepAlways: return "keep_always";
        default: return "unknown";
    }
}
void json_string(std::ostream& out, std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    out.put('"');
    for (char const raw : value) {
        auto const character = static_cast<unsigned char>(raw);
        switch (character) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (character < 0x20u) {
                    out << "\\u00" << digits[character >> 4u]
                        << digits[character & 0x0fu];
                } else {
                    out.put(static_cast<char>(character));
                }
                break;
        }
    }
    out.put('"');
}
void json_field(std::ostream& out, char const* key, std::string_view value) {
    json_string(out, key); out.put(':'); json_string(out, value);
}
void json_strings(std::ostream& out, std::vector<std::string> const& values) {
    out.put('[');
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out.put(',');
        json_string(out, values[i]);
    }
    out.put(']');
}
void json_assertion(std::ostream& out, AssertionRecord const& a) {
    out << "{\"outcome\":";
    json_string(out, outcome_string(a.outcome));
    out << ',';
    json_field(out, "description", a.description);
    out << ',';
    json_field(out, "actual", a.actual);
    out << ',';
    json_field(out, "expected", a.expected);
    out << ',';
    json_field(out, "detail", a.detail);
    out << ',';
    json_field(out, "file", a.file);
    out << ",\"line\":" << a.line << ",\"check_index\":" << a.check_index;
    out << ",\"context\":";
    json_strings(out, a.context);
    out << ",\"wait\":";
    if (a.wait) {
        out << "{\"status\":";
        json_string(out, wait_string(a.wait->status));
        out << ",\"elapsed_ns\":" << a.wait->elapsed.count()
            << ",\"available_budget_ns\":" << a.wait->available_budget.count()
            << ",\"absolute_deadline\":" << (a.wait->absolute_deadline ? "true" : "false") << '}';
    } else {
        out << "null";
    }
    out.put('}');
}
void json_diagnostic(std::ostream& out, DiagnosticEvent const& e) {
    out << "{\"kind\":";
    json_string(out, diagnostic_string(e.kind));
    out << ',';
    json_field(out, "description", e.description);
    out << ',';
    json_field(out, "value", e.value);
    out << ',';
    json_field(out, "file", e.file);
    out << ",\"line\":" << e.line
        << ",\"duration_ns\":" << e.duration.count() << ",\"context\":";
    json_strings(out, e.context);
    out.put('}');
}
void json_artifact(std::ostream& out, ArtifactRecord const& a) {
    out << '{';
    json_field(out, "path", a.path.generic_string());
    out << ',';
    json_field(out, "label", a.label);
    out << ',';
    json_field(out, "media_type", a.media_type);
    out << ',';
    json_field(out, "kind", artifact_kind(a.kind));
    out << ',';
    json_field(out, "lifetime", artifact_lifetime(a.lifetime));
    out << ",\"metadata\":[";
    for (std::size_t i = 0; i < a.metadata.size(); ++i) {
        if (i != 0) out.put(',');
        out.put('{');
        json_field(out, "name", a.metadata[i].first);
        out.put(',');
        json_field(out, "value", a.metadata[i].second);
        out.put('}');
    }
    out << "]}";
}
void json_case(std::ostream& out, CaseResult const& c) {
    out.put('{');
    json_field(out, "name", c.name);
    out.put(',');
    json_field(out, "suite", c.suite_name);
    out << ",\"case_index\":" << c.case_index
        << ",\"case_total\":" << c.case_total << ",\"outcome\":";
    json_string(out, outcome_string(c.outcome));
    out << ",\"duration_ns\":" << c.duration.count() << ",\"implementation\":";
    json_string(out, c.implementation);
    out << ",\"tags\":";
    json_strings(out, c.tags);
    out << ",\"assertions\":[";
    for (std::size_t i = 0; i < c.assertions.size(); ++i) {
        if (i != 0) out.put(',');
        json_assertion(out, c.assertions[i]);
    }
    out << "],\"diagnostics\":[";
    for (std::size_t i = 0; i < c.diagnostics.size(); ++i) {
        if (i != 0) out.put(',');
        json_diagnostic(out, c.diagnostics[i]);
    }
    out << "],\"artifacts\":[";
    for (std::size_t i = 0; i < c.artifacts.size(); ++i) {
        if (i != 0) out.put(',');
        json_artifact(out, c.artifacts[i]);
    }
    out << "]}";
}

void xml_string(std::ostream& out, std::string_view value) {
    for (char const raw : value) {
        auto const character = static_cast<unsigned char>(raw);
        switch (character) {
            case '&': out << "&amp;"; break;
            case '<': out << "&lt;"; break;
            case '>': out << "&gt;"; break;
            case '"': out << "&quot;"; break;
            case '\'': out << "&apos;"; break;
            default:
                // The disallowed ASCII controls cannot occur in XML 1.0.
                if (character < 0x20u && character != '\t' &&
                    character != '\n' && character != '\r') out.put('?');
                else out.put(static_cast<char>(character));
                break;
        }
    }
}
void xml_attribute(std::ostream& out, char const* name, std::string_view value) {
    out.put(' ');
    out << name << "=\"";
    xml_string(out, value);
    out.put('"');
}
std::string seconds(std::chrono::nanoseconds duration) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(9)
        << std::chrono::duration<double>(duration).count();
    return out.str();
}
std::string case_description(CaseResult const& result) {
    std::ostringstream out;
    for (auto const& a : result.assertions) {
        out << outcome_string(a.outcome) << " #" << a.check_index
            << ": " << a.description << '\n';
        for (auto const& scope : a.context) out << "  context: " << scope << '\n';
        if (!a.actual.empty()) out << "  actual: " << a.actual << '\n';
        if (!a.expected.empty()) out << "  expected: " << a.expected << '\n';
        if (!a.detail.empty()) out << "  detail: " << a.detail << '\n';
        if (a.wait) out << "  wait: " << wait_string(a.wait->status)
                        << " elapsed_ns=" << a.wait->elapsed.count()
                        << " available_budget_ns=" << a.wait->available_budget.count()
                        << " absolute_deadline=" << (a.wait->absolute_deadline ? "true" : "false") << '\n';
        if (!a.file.empty()) out << "  location: " << a.file << ':' << a.line << '\n';
    }
    for (auto const& d : result.diagnostics) {
        out << "diagnostic " << diagnostic_string(d.kind) << ": " << d.description
            << " value=" << d.value << " duration_ns=" << d.duration.count() << '\n';
        for (auto const& scope : d.context) out << "  context: " << scope << '\n';
    }
    for (auto const& artifact : result.artifacts)
        out << "artifact " << artifact_kind(artifact.kind) << ": "
            << artifact.path.generic_string() << " label=" << artifact.label << '\n';
    return out.str();
}
} // namespace


namespace {
// Each active suite runs its cases on the invoking thread. A handler that
// interrupts that thread reads only the published (inactive-to-writer) buffer.
// Check descriptions are rendered before the checked expression runs.
struct CrashBreadcrumb {
    char text[2][1024]{};
    unsigned short length[2]{};
    volatile std::sig_atomic_t published = 0;
};
thread_local CrashBreadcrumb crash_breadcrumb{};
volatile std::sig_atomic_t crash_enabled = 0;

void crash_checkpoint(CaseResult const& result, std::size_t check,
                      std::string_view description) {
    if (!crash_enabled) return;
    auto const target = 1 - crash_breadcrumb.published;
    auto& buffer = crash_breadcrumb.text[target];
    auto const name_length = static_cast<int>(std::min(result.name.size(), std::size_t{260}));
    auto const suite_length = static_cast<int>(std::min(result.suite_name.size(), std::size_t{160}));
    auto const desc_length = static_cast<int>(std::min(description.size(), std::size_t{450}));
    int size;
    if (check == 0) {
        size = std::snprintf(buffer, sizeof(buffer),
            "[CRASH CONTEXT]\n"
            "  suite: %.*s\n"
            "  case: %.*s\n"
            "  position: %zu/%zu\n"
            "  last check: (not started)\n",
            suite_length, result.suite_name.data(),
            name_length, result.name.data(), result.case_index, result.case_total);
    } else {
        size = std::snprintf(buffer, sizeof(buffer),
            "[CRASH CONTEXT]\n"
            "  suite: %.*s\n"
            "  case: %.*s\n"
            "  position: %zu/%zu\n"
            "  last check: [%zu.%zu] %.*s\n",
            suite_length, result.suite_name.data(),
            name_length, result.name.data(), result.case_index, result.case_total,
            result.case_index, check, desc_length, description.data());
    }
    // snprintf with only %.*s/%zu conversions cannot return a negative size;
    // even a hypothetical negative value would be capped by the min() below.
    crash_breadcrumb.length[target] =
        static_cast<unsigned short>(std::min(static_cast<std::size_t>(size), sizeof(buffer)-1));
    std::atomic_signal_fence(std::memory_order_release);
    crash_breadcrumb.published = target;
}
void crash_checkpoint_clear() {
    if (!crash_enabled) return;
    const auto target = 1 - crash_breadcrumb.published;
    crash_breadcrumb.length[target] = 0;
    std::atomic_signal_fence(std::memory_order_release);
    crash_breadcrumb.published = target;
}
void crash_emit_checkpoint() noexcept {
    // crash_enabled == 0 implies a zeroed breadcrumb (reset before disabling),
    // so the empty-count check below already guards the disabled case.
    const auto current = crash_breadcrumb.published;
    std::atomic_signal_fence(std::memory_order_acquire);
    auto const count = crash_breadcrumb.length[current];
    if (!count) return;
#ifdef _WIN32
    DWORD written = 0;
    HANDLE const error = ::GetStdHandle(STD_ERROR_HANDLE);
    if (error != INVALID_HANDLE_VALUE && error != nullptr)
        (void)::WriteFile(error, crash_breadcrumb.text[current], count, &written, nullptr);
#else
    // POSIX write is async-signal-safe. Do not allocate or use iostreams here.
    const ssize_t written = ::write(STDERR_FILENO, crash_breadcrumb.text[current], count);
    if (written < 0) return; // Best effort in an async-signal-safe handler.
#endif
}
#ifndef _WIN32
void crash_signal_handler(int signal_number) noexcept {
    crash_emit_checkpoint();
    // Installed with SA_RESETHAND: restore the normal signal termination.
    // raise is async-signal-safe on POSIX; pending signal fires on return.
    (void)::raise(signal_number);
}
#else
LONG WINAPI crash_exception_handler(EXCEPTION_POINTERS*) {
    crash_emit_checkpoint();
    return EXCEPTION_CONTINUE_SEARCH; // Preserve original Windows failure status.
}
#endif
} // namespace

struct CrashDiagnostics::Impl {
#ifdef _WIN32
    LPTOP_LEVEL_EXCEPTION_FILTER previous = nullptr;
#else
    // Without SIGBUS the fifth entry would duplicate SIGSEGV, so it is
    // omitted at compile time instead of being skipped at runtime.
    static constexpr int count =
#ifdef SIGBUS
        5;
#else
        4;
#endif
    int signals[count] = {SIGSEGV, SIGABRT, SIGILL, SIGFPE
#ifdef SIGBUS
        , SIGBUS
#endif
    };
    struct sigaction previous[count]{};
#endif
};

CrashDiagnostics::CrashDiagnostics() : _impl(new Impl) {
    crash_breadcrumb = CrashBreadcrumb{};
#ifdef _WIN32
    _impl->previous = ::SetUnhandledExceptionFilter(crash_exception_handler);
#else
    for (int i = 0; i < Impl::count; ++i) {
        struct sigaction handler{};
        handler.sa_handler = crash_signal_handler;
        sigemptyset(&handler.sa_mask);
        handler.sa_flags = static_cast<decltype(handler.sa_flags)>(SA_RESETHAND);
        // sigaction cannot fail here: every entry is a valid signal number and
        // both pointers refer to valid storage (only EINVAL/EFAULT are possible).
        (void)::sigaction(_impl->signals[i], &handler, &_impl->previous[i]);
    }
#endif
    crash_enabled = 1;
}

CrashDiagnostics::~CrashDiagnostics() {
    // Reset the breadcrumb before disabling, so crash_enabled == 0 always
    // implies an empty published checkpoint.
    crash_breadcrumb = CrashBreadcrumb{};
    crash_enabled = 0;
#ifdef _WIN32
    ::SetUnhandledExceptionFilter(_impl->previous);
#else
    for (int i = 0; i < Impl::count; ++i)
        (void)::sigaction(_impl->signals[i], &_impl->previous[i], nullptr);
#endif
    delete _impl;
}

ConsoleReporter::ConsoleReporter(std::ostream& output, ConsoleDetail detail)
    : _output(output), _detail(detail) { }

void ConsoleReporter::diagnostic_emitted(CaseResult const& result, DiagnosticEvent const& event) {
    if (_detail == ConsoleDetail::Concise) return;
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

void ConsoleReporter::selection_started(SelectionInfo const& info) {
    if (_detail == ConsoleDetail::Concise) return;
    _output << "[SELECTION " << info.selected << '/' << info.registered << "] ";
    if (!info.filter.name_contains.empty())
        _output << "name_contains=" << info.filter.name_contains << ' ';
    if (!info.filter.implementation.empty())
        _output << "implementation=" << info.filter.implementation << ' ';
    if (!info.filter.any_tags.empty()) {
        _output << "any_tags=";
        for (std::size_t i = 0; i < info.filter.any_tags.size(); ++i) {
            if (i) _output << ',';
            _output << info.filter.any_tags[i];
        }
        _output << ' ';
    }
    _output << (info.complete() ? "complete" : "partial") << '\n' << std::flush;
}

void ConsoleReporter::case_started(CaseResult const& result) {
    _last_check_index = 0;
    _last_check.clear();
    if (_detail == ConsoleDetail::Concise) {
        crash_checkpoint(result, 0, {});
        return;
    }
    _output << "[RUN " << result.case_index << '/' << result.case_total << "] "
            << result.suite_name << " / " << result.name << '\n' << std::flush;
}

void ConsoleReporter::check_started(CaseResult const& result, std::size_t index,
                                    std::string_view description) {
    if (_detail == ConsoleDetail::Concise) {
        _last_check_index = index;
        _last_check = description;
        crash_checkpoint(result, index, description);
        return;
    }
    _output << "[CHECK " << result.case_index << '.' << index << "] "
            << description << '\n' << std::flush;
}

void ConsoleReporter::case_finished(CaseResult const& result) {
    if (_detail == ConsoleDetail::Concise) crash_checkpoint_clear();
    if (result.outcome == Outcome::Passed || result.outcome == Outcome::Skipped) {
        if (_detail == ConsoleDetail::All)
            _output << '[' << outcome_name(result.outcome) << "] "
                    << result.name << '\n' << std::flush;
        return;
    }
    if (_detail == ConsoleDetail::All)
        _output << '[' << outcome_name(result.outcome) << "] " << result.name << '\n';

    ProblemCase problem{result.outcome, result.name, result.case_index, result.case_total,
                        _last_check_index, _last_check, {}, {}, {}, {}, {}, 0, {}, {}};
    for (auto const& assertion : result.assertions) {
        if (assertion.outcome != result.outcome) continue;
        problem.description = assertion.description;
        problem.actual = assertion.actual;
        problem.expected = assertion.expected;
        problem.detail = assertion.detail;
        if (assertion.check_index != 0) {
            problem.file = assertion.file;
            problem.line = assertion.line;
        }
        problem.context = assertion.context;
        break;
    }
    _problems.push_back(std::move(problem));

    // Concise prints only the final suite totals and failure index.
    // Keep at most 16 non-scope diagnostics per problematic case for that index.
    if (_detail == ConsoleDetail::Concise) {
        auto& issue = _problems.back();
        for (auto const& event : result.diagnostics) {
            if (event.kind == DiagnosticKind::Scope) continue;
            if (issue.diagnostics.size() == 16) {
                issue.diagnostics.emplace_back("additional events omitted");
                break;
            }
            std::ostringstream diagnostic;
            diagnostic << event.description;
            if (event.kind == DiagnosticKind::Value) diagnostic << " = " << event.value;
            if (event.kind == DiagnosticKind::Elapsed)
                diagnostic << " = " << event.duration.count() << " ns";
            issue.diagnostics.push_back(diagnostic.str());
        }
        return;
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
    if (_detail == ConsoleDetail::Concise) crash_checkpoint_clear();
    _output << summary_text(name,summary) << '\n';
    if (!_problems.empty()) {
        _output << "[FAILURE SUMMARY] " << _problems.size()
                << " problematic case(s) in " << name << '\n';
        for (auto const& problem : _problems) {
            _output << "  [" << outcome_name(problem.outcome) << ' '
                    << problem.case_index << '/' << problem.case_total << "] "
                    << problem.name << '\n';
            for (auto const& diagnostic : problem.diagnostics)
                _output << "    diagnostic: " << diagnostic << '\n';
            if (problem.last_check_index != 0)
                _output << "    last check [" << problem.case_index << '.'
                        << problem.last_check_index << "]: " << problem.last_check << '\n';
            if (!problem.description.empty())
                _output << "    " << problem.description << '\n';
            if (!problem.detail.empty()) _output << "    detail: " << problem.detail << '\n';
            if (!problem.context.empty()) {
                _output << "    context: ";
                for (std::size_t i = 0; i < problem.context.size(); ++i) {
                    if (i) _output << " / ";
                    _output << problem.context[i];
                }
                _output << '\n';
            }
            if (!problem.actual.empty()) _output << "    actual: " << problem.actual << '\n';
            if (!problem.expected.empty()) _output << "    expected: " << problem.expected << '\n';
            if (!problem.file.empty())
                _output << "    at " << problem.file << ':' << problem.line << '\n';
        }
    }
    _problems.clear();
    _last_check_index = 0;
    _last_check.clear();
    _output << std::flush;
}

namespace {
SelectionInfo effective_selection(std::optional<SelectionInfo> const& selection,
                                  std::size_t case_count) {
    return selection.value_or(SelectionInfo{case_count, case_count, {}});
}
void json_selection(std::ostream& out, SelectionInfo const& selection, bool filtered) {
    out << "{\"registered\":" << selection.registered
        << ",\"selected\":" << selection.selected
        << ",\"complete\":" << (selection.complete() ? "true" : "false")
        << ",\"filtered\":" << (filtered ? "true" : "false")
        << ",\"filter\":{";
    json_field(out, "name_contains", selection.filter.name_contains);
    out << ',';
    json_field(out, "implementation", selection.filter.implementation);
    out << ",\"any_tags\":";
    json_strings(out, selection.filter.any_tags);
    out << "}}";
}
void xml_suite_property(std::ostream& out, std::string_view name, std::string_view value) {
    out << "    <property";
    xml_attribute(out, "name", name);
    xml_attribute(out, "value", value);
    out << "/>\n";
}
} // namespace

void JsonReporter::selection_started(SelectionInfo const& info) { _selection = info; }
void JsonReporter::case_finished(CaseResult const& result) { _cases.push_back(result); }
void JsonReporter::suite_finished(std::string_view name, Summary const& summary) {
    _output << "{\"suite\":";
    json_string(_output, name);
    auto const selection = effective_selection(_selection, _cases.size());
    _output << ",\"selection\":";
    json_selection(_output, selection, _selection.has_value());
    _output << ",\"selected_successful\":" << (summary.successful() ? "true" : "false")
            << ",\"full_suite_successful\":"
            << ((selection.complete() && summary.successful()) ? "true" : "false");
    _output << ",\"summary\":{\"passed\":" << summary.passed
            << ",\"failed\":" << summary.failed
            << ",\"inconclusive\":" << summary.inconclusive
            << ",\"skipped\":" << summary.skipped
            << ",\"errors\":" << summary.errors << "},\"cases\":[";
    for (std::size_t i = 0; i < _cases.size(); ++i) {
        if (i != 0) _output.put(',');
        json_case(_output, _cases[i]);
    }
    _output << "]}\n" << std::flush;
    _cases.clear();
    _selection.reset();
}
void JUnitReporter::selection_started(SelectionInfo const& info) { _selection = info; }
void JUnitReporter::case_finished(CaseResult const& result) { _cases.push_back(result); }
void JUnitReporter::suite_finished(std::string_view name, Summary const& summary) {
    auto const selection = effective_selection(_selection, _cases.size());
    bool const incomplete = !selection.complete();
    std::chrono::nanoseconds total{};
    for (auto const& c : _cases) total += c.duration;
    _output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite";
    xml_attribute(_output, "name", name);
    xml_attribute(_output, "tests", std::to_string(_cases.size() + (incomplete ? 1u : 0u)));
    xml_attribute(_output, "failures",
                  std::to_string(summary.failed + summary.inconclusive + (incomplete ? 1u : 0u)));
    xml_attribute(_output, "errors", std::to_string(summary.errors));
    xml_attribute(_output, "skipped", std::to_string(summary.skipped));
    xml_attribute(_output, "time", seconds(total));
    _output << ">\n  <properties>\n";
    xml_suite_property(_output, "selection.registered", std::to_string(selection.registered));
    xml_suite_property(_output, "selection.selected", std::to_string(selection.selected));
    xml_suite_property(_output, "selection.complete", selection.complete() ? "true" : "false");
    xml_suite_property(_output, "selection.filtered", _selection ? "true" : "false");
    xml_suite_property(_output, "selection.name_contains", selection.filter.name_contains);
    xml_suite_property(_output, "selection.implementation", selection.filter.implementation);
    for (std::size_t i = 0; i < selection.filter.any_tags.size(); ++i)
        xml_suite_property(_output, "selection.any_tag." + std::to_string(i),
                           selection.filter.any_tags[i]);
    _output << "  </properties>\n";
    if (incomplete) {
        _output << "  <testcase name=\"__incomplete_selection__\"";
        xml_attribute(_output, "classname", name);
        _output << " time=\"0\">\n    <failure type=\"incomplete_coverage\"";
        xml_attribute(_output, "message", "Selected " + std::to_string(selection.selected) +
                      " of " + std::to_string(selection.registered) + " registered cases");
        _output << ">Explicit partial selection cannot establish full-suite coverage"
                << "</failure>\n  </testcase>\n";
    }
    for (auto const& c : _cases) {
        _output << "  <testcase";
        xml_attribute(_output, "name", c.name);
        xml_attribute(_output, "classname", c.suite_name);
        xml_attribute(_output, "time", seconds(c.duration));
        _output << ">\n";
        if (!c.artifacts.empty() || !c.tags.empty() || !c.implementation.empty()) {
            _output << "    <properties>\n";
            if (!c.implementation.empty())
                xml_suite_property(_output, "implementation", c.implementation);
            for (std::size_t i = 0; i < c.tags.size(); ++i)
                xml_suite_property(_output, "tag." + std::to_string(i), c.tags[i]);
            for (std::size_t i = 0; i < c.artifacts.size(); ++i) {
                _output << "      <property";
                xml_attribute(_output, "name", "artifact." + std::to_string(i) + ".path");
                xml_attribute(_output, "value", c.artifacts[i].path.generic_string());
                _output << "/>\n";
            }
            _output << "    </properties>\n";
        }
        if (c.outcome == Outcome::Skipped) {
            _output << "    <skipped message=\"skipped\"/>\n";
        } else if (c.outcome != Outcome::Passed) {
            bool const is_error = c.outcome == Outcome::Error;
            _output << (is_error ? "    <error" : "    <failure");
            xml_attribute(_output, "type", outcome_string(c.outcome));
            xml_attribute(_output, "message", c.assertions.empty() ?
                          std::string_view{"case has no assertion"} :
                          std::string_view{c.assertions.back().description});
            _output << '>';
            xml_string(_output, case_description(c));
            _output << (is_error ? "</error>\n" : "</failure>\n");
        }
        _output << "    <system-out>";
        xml_string(_output, case_description(c));
        _output << "</system-out>\n  </testcase>\n";
    }
    _output << "</testsuite>\n" << std::flush;
    _cases.clear();
    _selection.reset();
}
ReporterFanout::ReporterFanout(std::initializer_list<std::reference_wrapper<Reporter>> targets)
    : _targets(targets) {
    if (_targets.empty()) throw std::invalid_argument("ReporterFanout requires a reporter");
}
bool ReporterFanout::capture_passing_values() const noexcept {
    // The richest attached reporter determines the retained successful
    // operand payload; JSON/JUnit/custom capture must not lose information.
    return std::any_of(_targets.begin(), _targets.end(),
        [](auto const& target) { return target.get().capture_passing_values(); });
}
void ReporterFanout::selection_started(SelectionInfo const& info) {
    for (auto& r : _targets) r.get().selection_started(info);
}
void ReporterFanout::case_started(CaseResult const& result) {
    for (auto& r : _targets) r.get().case_started(result);
}
void ReporterFanout::check_started(CaseResult const& result, std::size_t index,
                                  std::string_view description) {
    for (auto& r : _targets) r.get().check_started(result, index, description);
}
void ReporterFanout::diagnostic_emitted(CaseResult const& result, DiagnosticEvent const& event) {
    for (auto& r : _targets) r.get().diagnostic_emitted(result, event);
}
void ReporterFanout::case_finished(CaseResult const& result) {
    for (auto& r : _targets) r.get().case_finished(result);
}
void ReporterFanout::suite_finished(std::string_view name, Summary const& summary) {
    for (auto& r : _targets) r.get().suite_finished(name, summary);
}
} // namespace Ariadne::Testing
