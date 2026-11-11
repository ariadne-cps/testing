/***************************************************************************
 *            testing/reporters.hpp
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

/** @file testing/reporters.hpp
 *  @brief Console, JSON, JUnit, and fan-out test result reporters.
 */
#ifndef ARIADNE_TESTING_REPORTERS_HPP
#define ARIADNE_TESTING_REPORTERS_HPP

#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <vector>

#include "testing/results.hpp"

namespace Ariadne::Testing {

/** @brief All streams full diagnostics; Concise emits final summaries and crash-only breadcrumbs. */
enum class ConsoleDetail { All,
                           Concise };

/** @brief Callback interface for live case and diagnostic reporting. */
class Reporter {
  public:
    /** @brief Destroy the object and release its owned resources. */
    virtual ~Reporter() = default;
    /** @brief Whether passing comparisons need their rendered operand values.
     *  @details Structured and custom reporters retain historical full payloads
     *  by default. A console-only run can omit costly successful-value
     *  formatting; failed and inconclusive comparisons always retain values.
     */
    virtual bool capture_passing_values() const noexcept { return true; }
    // Optional lifecycle notifications, compatible with existing reporters.
    /** @brief Called before any selected case, only for active filters. */
    virtual void selection_started(const SelectionInfo&) {}
    /** @brief Handle notification that a test case has started. */
    virtual void case_started(const CaseResult&) {}
    /** @brief Handle notification that an assertion check has started. */
    virtual void check_started(const CaseResult&, std::size_t, std::string_view) {}
    /** @brief Handle a diagnostic event for the current case. */
    virtual void diagnostic_emitted(const CaseResult&, const DiagnosticEvent&) {}
    /** @brief Handle the completed test-case result. */
    virtual void case_finished(const CaseResult& result) = 0;
    /** @brief Handle the final suite summary. */
    virtual void suite_finished(std::string_view name, const Summary& summary) = 0;
};

/** @brief Scope a crash-only stderr breadcrumb for an argc/argv --concise run.
 *  @details Supports POSIX fatal signals and unhandled Windows exceptions.
 *  An unexpected process crash prints the active case and most recently
 *  started CHECK without streaming ordinary test progress. Does not suppress
 *  the original crash or exit status. No effect on normal suite results.
 */
class CrashDiagnostics final {
  public:
    /** @brief Construct a \c CrashDiagnostics with the specified characteristics. */
    CrashDiagnostics();
    /** @brief Restore the previous crash-diagnostic handlers. */
    ~CrashDiagnostics();
    /** @brief Disallow this construction to preserve the ownership contract. */
    CrashDiagnostics(const CrashDiagnostics&) = delete;
    /** @brief Disallow assignment to preserve the ownership contract. */
    CrashDiagnostics& operator=(const CrashDiagnostics&) = delete;

  private:
    struct Impl;
    Impl* _impl;
};

/** @brief Human-readable reporter with configurable diagnostic verbosity. */
class ConsoleReporter final : public Reporter {
  public:
    /** @brief Construct a \c ConsoleReporter from output, detail. */
    explicit ConsoleReporter(std::ostream& output, ConsoleDetail detail = ConsoleDetail::All);
    bool capture_passing_values() const noexcept override { return false; }
    void selection_started(const SelectionInfo& info) override;
    /** @brief Handle a diagnostic event for the current case. */
    void diagnostic_emitted(const CaseResult& result, const DiagnosticEvent& event) override;
    /** @brief Handle notification that a test case has started. */
    void case_started(const CaseResult& result) override;
    /** @brief Handle notification that an assertion check has started. */
    void check_started(const CaseResult& result, std::size_t index,
                       std::string_view description) override;
    /** @brief Handle the completed test-case result. */
    void case_finished(const CaseResult& result) override;
    /** @brief Handle the final suite summary. */
    void suite_finished(std::string_view name, const Summary& summary) override;

  private:
    /** @brief Retained failed-case diagnostics for the console summary. */
    struct ProblemCase {
        Outcome outcome;
        std::string name;
        std::size_t case_index;
        std::size_t case_total;
        std::size_t last_check_index;
        std::string last_check;
        std::string description;
        std::string actual;
        std::string expected;
        std::string detail;
        std::string file;
        std::uint_least32_t line;
        std::vector<std::string> context;
        std::vector<std::string> diagnostics;
    };

    std::ostream& _output;
    ConsoleDetail _detail;
    std::vector<ProblemCase> _problems;
    std::size_t _last_check_index = 0;
    std::string _last_check;
};

/** @brief One JSON document per completed suite, with full result fidelity.
 *  @details All five outcomes, ordered assertions, nested contexts, wait
 *  observations, nonasserting diagnostics, durations and artifacts are
 *  serialized. Report generation does not change result classifications.
 *  The caller owns the output stream and must keep it alive.
 */
class JsonReporter final : public Reporter {
  public:
    /** @brief Construct a \c JsonReporter from output. */
    explicit JsonReporter(std::ostream& output) : _output(output) {}
    void selection_started(const SelectionInfo& info) override;
    /** @brief Handle the completed test-case result. */
    void case_finished(const CaseResult& result) override;
    /** @brief Handle the final suite summary. */
    void suite_finished(std::string_view name, const Summary& summary) override;

  private:
    std::ostream& _output;
    std::vector<CaseResult> _cases;
    std::optional<SelectionInfo> _selection;
};

/** @brief JUnit XML reporter with non-lossy outcome classification.
 *  @details Failed and Inconclusive map to distinct <failure> types;
 *  Error maps to <error>, and Skipped to <skipped>. Original diagnostics,
 *  assertion contexts and artifact references are included in system-out;
 *  artifacts also appear as testcase properties. JUnit cannot represent
 *  indeterminate evidence natively, so the failure type retains its label.
 *  Explicit incomplete selection adds a clearly labelled synthetic failure
 *  in JUnit only, so consuming that report cannot mistake partial coverage
 *  for a fully passing regression; actual CaseResult data remain unchanged.
 */
class JUnitReporter final : public Reporter {
  public:
    /** @brief Construct a \c JUnitReporter from output. */
    explicit JUnitReporter(std::ostream& output) : _output(output) {}
    void selection_started(const SelectionInfo& info) override;
    /** @brief Handle the completed test-case result. */
    void case_finished(const CaseResult& result) override;
    /** @brief Handle the final suite summary. */
    void suite_finished(std::string_view name, const Summary& summary) override;

  private:
    std::ostream& _output;
    std::vector<CaseResult> _cases;
    std::optional<SelectionInfo> _selection;
};

/** @brief Forward every Reporter event in order to several owned-by-caller reporters.
 *  @details Put ConsoleReporter first to preserve immediate RUN/CHECK flushes.
 *  It is not required to generate a JSON/JUnit report; no global Logging or
 *  platform-specific API is involved. Reporters must outlive this object.
 */
class ReporterFanout final : public Reporter {
  public:
    /** @brief Construct a \c ReporterFanout from targets. */
    explicit ReporterFanout(std::initializer_list<std::reference_wrapper<Reporter>> targets);
    bool capture_passing_values() const noexcept override;
    void selection_started(const SelectionInfo& info) override;
    /** @brief Handle notification that a test case has started. */
    void case_started(const CaseResult& result) override;
    /** @brief Handle notification that an assertion check has started. */
    void check_started(const CaseResult& result, std::size_t index,
                       std::string_view description) override;
    /** @brief Handle a diagnostic event for the current case. */
    void diagnostic_emitted(const CaseResult& result, const DiagnosticEvent& event) override;
    /** @brief Handle the completed test-case result. */
    void case_finished(const CaseResult& result) override;
    /** @brief Handle the final suite summary. */
    void suite_finished(std::string_view name, const Summary& summary) override;

  private:
    std::vector<std::reference_wrapper<Reporter>> _targets;
};

} // namespace Ariadne::Testing
#endif // ARIADNE_TESTING_REPORTERS_HPP
