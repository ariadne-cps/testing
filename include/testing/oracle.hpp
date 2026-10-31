/***************************************************************************
 *            oracle.hpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/
/*
 *  This file is part of Ariadne Testing.
 *  Ariadne Testing is free software under the GNU General Public License,
 *  version 3 or (at your option) any later version.
 *  Ariadne Testing comes WITHOUT ANY WARRANTY.
 */
/** @file testing/oracle.hpp
 *  @brief Frozen-oracle provenance and format-independent row integrity.
 *  @details Parsers and independent golden generators belong to the consumer.
 *  No file is read, written, generated, or selected by these data structures.
 */
#ifndef ARIADNE_TESTING_ORACLE_HPP
#define ARIADNE_TESTING_ORACLE_HPP

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Ariadne::Testing {

/** @brief Optional, read-only reference metadata; not a data generator. */
struct OracleMetadata {
    std::filesystem::path reference;
    std::string label;
    std::string row_key_column;
    std::optional<std::uint64_t> seed;
    std::optional<std::uint32_t> precision_bits;
    std::vector<std::pair<std::string, std::string>> input_attributes;
};

/** @brief Composite identity of one frozen reference row.
 *  @details The same domain key may occur in separate iterations/repetitions.
 */
struct OracleRowId {
    std::string key;
    std::uint64_t iteration = 0;
    std::uint64_t repetition = 0;
    bool operator==(OracleRowId const&) const = default;
};

/** @brief Result of a consumer-owned reader; parsed=false marks a bad row.
 *  @note Invalid textual fields must be rejected by the reader, not coerced
 *  to an invented numeric key or a default iteration.
 */
struct OracleRow {
    OracleRowId id;
    bool parsed = true;
};

enum class OracleIssueKind {
    InvalidManifest, Malformed, Duplicate, Missing, Extra, OutOfOrder
};

/** @brief One specific integrity defect; index refers to the affected list. */
struct OracleIssue {
    OracleIssueKind kind;
    std::size_t index;
    OracleRowId id;
};

/** @brief Independent verification of expected and observed key sequences. */
struct OracleIntegrity {
    std::size_t expected_rows = 0;
    std::size_t observed_rows = 0;
    std::vector<OracleIssue> issues;

    bool valid() const noexcept { return issues.empty(); }
    bool invalid_manifest() const noexcept {
        for (auto const& issue : issues)
            if (issue.kind == OracleIssueKind::InvalidManifest) return true;
        return false;
    }
    /** @brief Human-readable first issue and counts for assertion diagnostics. */
    std::string description() const;
};

/** @brief Validate a complete domain-parsed dataset against a frozen manifest.
 *  @details Exact composite-key equality is required. A missing, duplicated,
 *  malformed, unexpected or reordered row is an integrity failure. An
 *  invalid/empty manifest is an infrastructure Error, never a vacuous pass.
 *  There is NO implicit row selection, iteration reduction or RNG mutation.
 *  A domain reader is responsible for faithfully reporting malformed rows.
 */
OracleIntegrity validate_oracle_rows(std::span<OracleRow const> manifest,
                                    std::span<OracleRow const> observed);

/** @brief Explicit replay selection, never an assertion of full coverage.
 *  @details Empty requested list means full replay. Nonempty requests are
 *  matched by full composite row identity, never by substring or prefix.
 *  Indices retain the frozen manifest's row order, even if requests are
 *  supplied in a different order. Requests do not invoke any generators,
 *  random engines, callbacks or readers.
 */
struct OracleReplayPlan {
    std::size_t total_rows = 0;
    std::vector<std::size_t> indices;
    bool valid = true;
    bool complete = false;
    std::string reason;

    /** @brief Reject fabricated, empty, repeated, unordered or incomplete plans. */
    bool structurally_valid() const noexcept {
        if (!valid || total_rows == 0 || indices.empty() ||
            indices.size() > total_rows) return false;
        for (std::size_t i = 0; i < indices.size(); ++i)
            if (indices[i] >= total_rows ||
                (i != 0 && indices[i - 1] >= indices[i])) return false;
        return complete == (indices.size() == total_rows);
    }

    /** @brief Check membership without reading or advancing a random stream.
     *  @note If the consumer uses one shared RNG, generate all rows in the
     *  original order BEFORE deciding whether to assert a selected row.
     */
    bool includes(std::size_t index) const noexcept {
        if (!structurally_valid()) return false;
        for (auto const selected : indices)
            if (selected == index) return true;
        return false;
    }

    std::string description() const;
};

/** @brief Resolve full or selected replay with no RNG side effects.
 *  @param manifest Independently frozen complete reference rows.
 *  @param requested Empty: all rows. Otherwise explicitly requested IDs.
 *  @return Invalid plan for empty/invalid manifest, duplicate or unknown
 *  requests. Valid partial replay still explicitly reports incomplete
 *  coverage; by default it must NOT count as a passing full-suite check.
 */
OracleReplayPlan plan_oracle_replay(std::span<OracleRow const> manifest,
                                   std::span<OracleRowId const> requested = {});

} // namespace Ariadne::Testing
#endif // ARIADNE_TESTING_ORACLE_HPP
