/***************************************************************************
 *            testing/results.hpp
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

/** @file testing/results.hpp
 *  @brief Evidence, test outcomes, diagnostics, artifacts, and result summaries.
 *  @details Passive value types; no test execution or reporting dependencies.
 */
#ifndef ARIADNE_TESTING_RESULTS_HPP
#define ARIADNE_TESTING_RESULTS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ariadne::Testing {

/** @brief Three-valued mathematical evidence; Indeterminate is not a boolean pass. */
enum class Evidence { False,
                      True,
                      Indeterminate };
/** @brief Operational classification of a case or assertion. */
enum class Outcome { Passed,
                     Failed,
                     Inconclusive,
                     Skipped,
                     Error };

// The relation is selected at compile time; predicate evidence stays three-valued.
/** @brief Compile-time selection of a binary comparison relation. */
enum class Comparison { NotEqual,
                        Less,
                        LessEqual,
                        Greater,
                        GreaterEqual };

// Desired evidence, not an algorithm's return code or an operational outcome.
/** @brief Expectation for a three-valued logical result. */
enum class EvidenceExpectation { DefinitelyTrue,
                                 PossiblyTrue,
                                 NotPossibly,
                                 Indeterminate };

// Diagnostic events are not assertions and cannot change the test outcome.
/** @brief Kind of non-asserting diagnostic notification. */
enum class DiagnosticKind { Scope,
                            Note,
                            Warning,
                            Value,
                            Elapsed };

/** @brief Diagnostic event emitted during case execution; it does not change the outcome. */
struct DiagnosticEvent {
    DiagnosticKind kind;
    std::string description;
    std::string value;
    std::vector<std::string> context;
    std::chrono::nanoseconds duration{};
    std::string file;
    std::uint_least32_t line = 0;
};

/** @brief Observed end state of a bounded wait, without diagnosing its cause. */
enum class WaitStatus { Ready,
                        Timeout,
                        Deferred,
                        InvalidTimeout,
                        Unexpected };

/** @brief Structured observation for a bounded wait assertion.
 *  @details `elapsed` is measured waiting time, not a benchmark.
 *  `available_budget` is the relative timeout or time remaining when
 *  an absolute-deadline check began. It is negative for invalid durations.
 *  A timeout does not establish deadlock, starvation or scheduler slowdown.
 */
struct WaitObservation {
    WaitStatus status;
    std::chrono::nanoseconds elapsed{};
    std::chrono::nanoseconds available_budget{};
    bool absolute_deadline = false;
};

/** @brief Captured assertion, including source location and nested diagnostic context. */
struct AssertionRecord {
    Outcome outcome;
    std::string description;
    std::string actual;
    std::string expected;
    std::string detail;
    std::string file;
    std::uint_least32_t line;
    std::size_t check_index = 0;
    std::vector<std::string> context;
    // Empty for ordinary assertions and nonasserting elapsed diagnostics.
    std::optional<WaitObservation> wait;
};

// The framework only records these references; adapters decide file formats.
/** @brief Role of a registered artifact, independent of its retention policy. */
enum class ArtifactKind { Output,
                          Reference,
                          Difference };
/** @brief Ownership and retention policy for an artifact's path. */
enum class ArtifactLifetime { External,
                              Temporary,
                              KeepOnFailure,
                              KeepAlways };

/** @brief Metadata reference for an output, reference or difference artifact. */
struct ArtifactRecord {
    std::filesystem::path path;
    std::string label;
    std::string media_type;
    ArtifactKind kind = ArtifactKind::Output;
    ArtifactLifetime lifetime = ArtifactLifetime::External;
    std::vector<std::pair<std::string, std::string>> metadata;
};

/** @brief Per-case execution result, assertions, diagnostics and artifacts. */
struct CaseResult {
    std::string name;
    Outcome outcome = Outcome::Passed;
    std::vector<AssertionRecord> assertions;
    std::chrono::nanoseconds duration{};
    std::string suite_name;
    std::size_t case_index = 0;
    std::size_t case_total = 0;
    std::vector<DiagnosticEvent> diagnostics;
    std::vector<ArtifactRecord> artifacts;
    // Labels are per-case metadata; they are not generated from case names.
    std::vector<std::string> tags;
    std::string implementation;
};

// Exit-gate policy only: assertion outcomes and summary counts never change.
/** @brief Whether an inconclusive case blocks overall acceptance. */
enum class InconclusivePolicy { Blocking,
                                NonBlocking };
/** @brief Whether a skipped case blocks overall acceptance. */
enum class SkippedPolicy { NonBlocking,
                           Blocking };

/** @brief Exit-gate policy; does not rewrite assertion classifications. */
struct AcceptancePolicy {
    InconclusivePolicy inconclusive = InconclusivePolicy::Blocking;
    SkippedPolicy skipped = SkippedPolicy::NonBlocking;
};

/** @brief Aggregated counts and the exit-gate acceptance decision. */
struct Summary {
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t inconclusive = 0;
    std::size_t skipped = 0;
    std::size_t errors = 0;

    // Even an opt-in permissive exit gate requires at least one actual pass.
    bool acceptable(AcceptancePolicy policy) const {
        return passed > 0 && failed == 0 && errors == 0 && (inconclusive == 0 || policy.inconclusive == InconclusivePolicy::NonBlocking) && (skipped == 0 || policy.skipped == SkippedPolicy::NonBlocking);
    }
    bool successful() const { return acceptable({}); }
};

/** @brief Registration metadata used for domain-specific grouping. */
struct CaseMetadata {
    std::vector<std::string> tags;
    // A variant's implementation name is its exact selection identity.
    std::string implementation;
};

/** @brief Opt-in selection. Blank fields mean no constraint.
 *  @details Multiple fields intersect; any_tags matches at least one tag.
 *  This type never modifies the set of registered cases.
 */
struct CaseFilter {
    std::string name_contains;
    std::vector<std::string> any_tags;
    std::string implementation;

    bool active() const noexcept {
        return !name_contains.empty() || !any_tags.empty() || !implementation.empty();
    }
};

/** @brief Exact numbers and predicate applied when running a filtered suite. */
struct SelectionInfo {
    std::size_t registered = 0;
    std::size_t selected = 0;
    CaseFilter filter;

    bool complete() const noexcept { return registered == selected; }
};

/** @brief Filtered results, separating selection success from full coverage. */
struct SelectionResult {
    Summary summary;
    SelectionInfo selection;
    // This is the only API that can claim a complete successful regression.
    bool successful() const { return selection.complete() && summary.successful(); }
    // Explicit opt-in for callers that intentionally only test a subset.
    bool selected_successful() const { return summary.successful(); }
};

} // namespace Ariadne::Testing

#endif // ARIADNE_TESTING_RESULTS_HPP
