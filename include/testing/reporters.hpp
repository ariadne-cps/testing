/***************************************************************************
 *            reporters.hpp
 *
 *  Copyright  2026  Luca Geretti
 ****************************************************************************/
/*
 *  This file is part of Ariadne Testing, distributed under GPLv3 or later.
 *  Ariadne Testing comes WITHOUT ANY WARRANTY.
 */
/** @file testing/reporters.hpp
 *  @brief Structured JSON/JUnit and live fan-out reporters without dependencies.
 */
#ifndef ARIADNE_TESTING_REPORTERS_HPP
#define ARIADNE_TESTING_REPORTERS_HPP

#include "testing/testing.hpp"
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <vector>

namespace Ariadne::Testing {

/** @brief One JSON document per completed suite, with full result fidelity.
 *  @details All five outcomes, ordered assertions, nested contexts, wait
 *  observations, nonasserting diagnostics, durations and artifacts are
 *  serialized. Report generation does not change result classifications.
 *  The caller owns the output stream and must keep it alive.
 */
class JsonReporter final : public Reporter {
  public:
    explicit JsonReporter(std::ostream& output) : _output(output) {}
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::ostream& _output;
    std::vector<CaseResult> _cases;
};

/** @brief JUnit XML reporter with non-lossy outcome classification.
 *  @details Failed and Inconclusive map to distinct <failure> types;
 *  Error maps to <error>, and Skipped to <skipped>. Original diagnostics,
 *  assertion contexts and artifact references are included in system-out;
 *  artifacts also appear as testcase properties. JUnit cannot represent
 *  indeterminate evidence natively, so the failure type retains its label.
 */
class JUnitReporter final : public Reporter {
  public:
    explicit JUnitReporter(std::ostream& output) : _output(output) {}
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::ostream& _output;
    std::vector<CaseResult> _cases;
};

/** @brief Forward every Reporter event in order to several owned-by-caller reporters.
 *  @details Put ConsoleReporter first to preserve immediate RUN/CHECK flushes.
 *  It is not required to generate a JSON/JUnit report; no global Logging or
 *  platform-specific API is involved. Reporters must outlive this object.
 */
class ReporterFanout final : public Reporter {
  public:
    explicit ReporterFanout(std::initializer_list<std::reference_wrapper<Reporter>> targets);
    void case_started(CaseResult const& result) override;
    void check_started(CaseResult const& result, std::size_t index,
                       std::string_view description) override;
    void diagnostic_emitted(CaseResult const& result, DiagnosticEvent const& event) override;
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::vector<std::reference_wrapper<Reporter>> _targets;
};

} // namespace Ariadne::Testing
#endif // ARIADNE_TESTING_REPORTERS_HPP
