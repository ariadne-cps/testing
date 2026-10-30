# Ariadne Testing — design journal

This document is the evolving architectural contract and design journal for
`ariadne-cps/testing`. Update it whenever changing semantics, dependencies,
CI conventions, or migration decisions.

## Current architecture (2026-10-30)

`testing` depends only on `configuration` for CMake/build infrastructure,
not on `logging`, `utility`, `macro` or any numerical component.
Reporting is via the `Reporter` interface, with built-in
`ConsoleReporter(std::ostream&)`; result capture is independent of global
logging state, verbosity, threading and schedulers.

The dated entries below are historical; the original Logging dependency and
`LoggingReporter` design described there are superseded.

## 2026-10-30 — Initial bootstrap

### Goals

Build an in-house C++20 testing framework tailored to rigorous numerical software,
without Catch2, GoogleTest, doctest, or any external test framework. Replace the
monolithic testing macros with a model separating mathematical evidence, test outcomes,
test execution, and presentation. The original `macro/testing.hpp` combines these
responsibilities and, in some failure paths, evaluates operands again.

### Dependency and build policy

Production dependency direction: `testing -> logging -> utility -> macro`.
The repository pins `logging` and `configuration` as Git submodules. Its
`configuration` commit must match all the transitive routes through Logging,
Utility and Macro. These foundational repositories must not add a reverse
submodule dependency on Testing; their self-tests remain bootstrap tests.

`testing` must not depend on `numeric`, `interval`, `algebra` or other
high-level modules. Domain-specific numerical assertions should be implemented
using ADL-based customization or in test-only adapters owned by those domains.

### Rigorous semantics

The mathematical evidence lattice for the first implementation is:
- `Evidence::True`: definitively established.
- `Evidence::False`: definitively refuted.
- `Evidence::Indeterminate`: neither established nor refuted.

`Context::prove` maps these to `Passed`, `Failed`, and `Inconclusive`
respectively. `Skipped` and `Error` are operational outcomes, not mathematical
truth values. Uncaught exceptions result in `Error`; no recorded assertions
result in `Skipped`. An inconsistent ADL predicate reporting
`definitely(v) && !possibly(v)` is a logic error.

A suite is successful iff at least one test case passed, and no case failed,
was inconclusive, or errored. An intentionally skipped case does not by itself
fail an otherwise successful suite. Within one case, the severity ordering is:
`Error > Failed > Inconclusive > Skipped > Passed`. More explicit handling of
skip/inconclusive policies remains future work.

### Initial API

`Suite` manages named cases of type `void(Context&)`. `Context` provides:
- `prove` for three-valued mathematical propositions;
- `expect_equal` for `==`;
- `expect_same` for ADL `same`;
- `expect_within` for `mag(actual-expected) <= tolerance` (or `std::abs`);
- `skip` for explicitly omitted checks.

For evidence, `definitely` and `possibly` are found via ADL; other values use
boolean conversion. Diagnostic values are streamed where possible, or display
`<unprintable>`. `std::source_location` captures call-site file and line.
Assertion operands are passed once and not re-evaluated during diagnostics.
No guarantee of left-to-right evaluation of distinct C++ function arguments
is implied.

### Reporting and Logging

`ConsoleReporter` renders results without requiring logger state. The
optional `LoggingReporter` uses the Ariadne Logger, whose verbosity, muting
and scheduling may suppress output. It is **never** authoritative for verdicts:
those reside in structured `CaseResult` and `AssertionRecord` instances.
A future structured log capture interface must address thread safety,
lifetime/isolation, flushing, and global logger configuration.

### CI and bootstrap tests

Match the existing `ariadne-cps/logging` CI layout on `main`: Ubuntu and macOS
Debug/Release, Windows MSVC Debug/Release, sanitizers, coverage with GCC/LLVM,
install target, and CTest. The framework's own test executable checks its
collected outcomes using ordinary C++ conditionals, rather than trusting the
framework under test.

### Deferred design choices

1. Explicit policies for indeterminate and skipped cases.
2. Test fixture lifetime and environment isolation (precision, rounding mode).
3. Property-based testing, seeds, reproductions, shrinking.
4. Domain adapters for validated numbers, enclosures, intervals and sets.
5. Optional thin expression-stringification macros and compatibility migration.
6. Per-test scoped log capture, concurrent execution and output ordering.
7. JUnit/JSON reporters and per-case CTest discovery.
8. Fatal assertions, expected exception assertions, timeouts and parameterization.

### Verification log

- Local GCC 14 C++20 build and self-test run executed with a minimal Logger stub;
  it verifies the framework core, not integration against the actual dependency build.
- GitHub Actions platform builds and full CMake integration: pending CI runs.
- No migration of the old `macro/testing.hpp` or other repositories attempted.

Maintain this journal as a durable record of future decisions, not only a roadmap.

## 2026-10-30 — Resolve GCC 12 -Wswitch-default CI failure

### Observation

The Ubuntu Release job with GCC 12 and `WARNINGS_AS_ERRORS=ON` rejected
four exhaustive enum `switch` statements in `src/testing.cpp` because the
project enables `-Wswitch-default`. This warning requires an explicit
`default` branch even when all declared enumerators are covered.

### Resolution

Add a `default` arm to each of `outcome_name`, `priority`,
`Context::outcome`, and the outcome aggregation in `Suite::run`.
Retain the former fallback semantics: unsupported names print `UNKNOWN`,
unsupported priorities receive error severity, invalid mathematical evidence
becomes `Error`, and invalid case outcomes are counted as errors and normalized
to `Outcome::Error` before reporting. Do not suppress warnings or relax
`-Werror`.

### Verification

Recompiled `src/testing.cpp` and `tests/test_testing.cpp` in local Release
(`-O3 -DNDEBUG`) and Debug (`-O0 -g`) configurations with GCC 14 and
all project warning flags, including `-Wswitch-default -Werror`. Both
self-test executions passed. These local checks used a minimal logger stub;
they do **not** establish that the full GitHub Actions matrix, GCC 12, or
integration with the real Logging submodule has passed. Check CI for those.

## 2026-10-30 — Remove the Macro repository from the dependency chain

The intended dependency chain is now `testing -> logging -> utility`, with
general-purpose production macros internal to Utility as
`utility/macros.hpp`. Utility and Logging changes are staged on their
respective `testing` branches; Testing tracks the exact Logging
`testing` commit in its `main` gitlink, which transitively pins Utility's
`testing` commit. All repositories continue to use the identical
Configuration commit.

The dependency constraints in Testing CMake drop the obsolete
`logging|utility|macro|configuration` path. Utility and Logging
bootstrap tests no longer include `macro/testing.hpp`; each keeps
independent, standard-C++ assertion helpers without a reverse
dependency on Testing. The legacy dedicated test of Macro's test macros
is removed from Utility because the test suite no longer uses that API.

**Migration note:** This branch update does not modify `utility/main` or
`logging/main`. The above gitlinks intentionally point to commits
on their `testing` branches. Future commit propagation should keep
`testing/main` synchronized with those exact changes, and all CI
configurations should be run before considering promotion of the
upstream branches.

### CI correction — configure-time regression

After publishing the new dependency gitlink, `testing/main` CI failed at
CMake configuration because the old `logging|utility|macro|configuration`
constraint remained in the one-line `require_same_dependency_commit` call.
The obsolete chain was removed, while retaining the three valid checks:
`configuration`, `logging|configuration`, and
`logging|utility|configuration`. This was a migration regression, not an
issue with the submodule commit identities. CI confirmation is pending.

## 2026-10-30 — Remove Logging dependency entirely

### Decision

Testing is a standalone proof-oriented testing framework. Only the pinned
`ss/cfg` Configuration gitlink remains. Remove the `ss/log` Git submodule
and entry from `.gitmodules`, the unused `ss/CMakeLists.txt`, the
`add_subdirectory(ss)` call, obsolete cross-submodule commit consistency
checks, and `target_link_libraries(testing INTERFACE logging)`.
No logging library shall be required to build, test, or install Testing.

Remove `LoggingReporter` and `#include "logging/logging.hpp"` from the
public API and implementation. Keep the abstract `Reporter` interface and
the direct-to-`std::ostream` `ConsoleReporter`. This avoids dependence on
verbosity, muting, thread registration and scheduler state. Optional reporters
provided by higher-level consumers can adapt to external systems without
changing Testing's own dependencies.

### Testing and compatibility

Extend self-tests with a `ConsoleReporter` output regression checking
PASS/FAIL lines, actual/expected values, and summary formatting. Keep existing
tests of ternary truth and error classification unchanged. Removing the
`LoggingReporter` public type is an intentional API break.

Local GCC 14 Debug and Release builds, with strict project warnings including
`-Wswitch-default -Werror`, and a local Clang C++20 build passed the self-tests
for this change. The full Configuration-based CMake and CI matrix remain to
be verified on the new GitHub commit.

This does not change `utility/testing` or `logging/testing`: those branches
can evolve independently, while `testing/main` no longer tracks their commits.

## 2026-10-30 — Typed and untyped exception expectations

Utility migration requires test-side checks for expected and unexpected exceptions.
Added `expect_throws<E>`, `expect_exception` and `expect_no_throw`, with
single evaluation, caller source location and dedicated success/failure assertions.
Wrong exception types and missing exceptions are failures. Self-tests validate
all six paths. This keeps Utility free of a parallel testing harness.

## 2026-10-30 — Signed and unsigned integral equality

Integration with the Utility test suite revealed that the generic
`expect_equal(A,B)` implementation triggers `-Wsign-compare -Werror`
when an unsigned value is compared with a signed expectation in its
instantiated function template. For two non-bool built-in integral operands,
use C++20 `std::cmp_equal` instead of `operator==`: it preserves the correct
mathematical interpretation of negative signed values and avoids compiler
warnings. For numerical user-defined types, keep their `operator==` and ADL
three-valued evidence unchanged. The framework self-test now checks equality
across positive mixed-sign types and inequality of -1 versus max unsigned.
