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
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Ariadne::Testing {
namespace {

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
    out << ",\"duration_ns\":" << c.duration.count() << ",\"assertions\":[";
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

void JsonReporter::case_finished(CaseResult const& result) { _cases.push_back(result); }
void JsonReporter::suite_finished(std::string_view name, Summary const& summary) {
    _output << "{\"suite\":";
    json_string(_output, name);
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
}
void JUnitReporter::case_finished(CaseResult const& result) { _cases.push_back(result); }
void JUnitReporter::suite_finished(std::string_view name, Summary const& summary) {
    std::chrono::nanoseconds total{};
    for (auto const& c : _cases) total += c.duration;
    _output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite";
    xml_attribute(_output, "name", name);
    xml_attribute(_output, "tests", std::to_string(_cases.size()));
    xml_attribute(_output, "failures", std::to_string(summary.failed + summary.inconclusive));
    xml_attribute(_output, "errors", std::to_string(summary.errors));
    xml_attribute(_output, "skipped", std::to_string(summary.skipped));
    xml_attribute(_output, "time", seconds(total));
    _output << ">\n";
    for (auto const& c : _cases) {
        _output << "  <testcase";
        xml_attribute(_output, "name", c.name);
        xml_attribute(_output, "classname", c.suite_name);
        xml_attribute(_output, "time", seconds(c.duration));
        _output << ">\n";
        if (!c.artifacts.empty()) {
            _output << "    <properties>\n";
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
}
ReporterFanout::ReporterFanout(std::initializer_list<std::reference_wrapper<Reporter>> targets)
    : _targets(targets) {
    if (_targets.empty()) throw std::invalid_argument("ReporterFanout requires a reporter");
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
