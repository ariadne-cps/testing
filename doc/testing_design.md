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
- `Evidence::Proven`: definitively established.
- `Evidence::Refuted`: definitively refuted.
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


## 2026-10-30 — Two-level test-function/checker progress

Each registered suite case corresponds to one test function (not a group of
functions). A case has a one-based index in the execution order; each
`Context` has its own one-based checker count. Thus `4.21` means
the 21st checker of the 4th function in the current executable's suite.
The counter resets per case, and suite case total is known at runtime.
No third nesting level is introduced; each file defines its own suite.

The `Reporter` lifecycle has two optional no-op virtual hooks,
`case_started` and `check_started`. The console implementation emits
`[RUN 4/10] suite / function` and `[CHECK 4.21] property` and flushes
before execution of the associated function or checker body.

For eager C++ calls such as `expect_equal(f(), expected)`, evaluation
of `f()` happens before entering the checker; a hang there cannot be
announced. `check([&] { return condition(); }, description)` provides
deferred evaluation after its CHECK event, as do exception checkers.
A caught unexpected exception from a checker is an operational error
and does not create a phantom extra numbered checker.

## 2026-10-30 — Automatic assertion descriptions

Optional `testing/checks.hpp` macros stringify user expressions and delegate
all logic to typed Context functions. The wrappers cover proof, equality,
ADL identity, bounded error, typed/any exceptions and no-throw expectations.
Deferred `check_equal`, `check_same`, and `check_within` announce CHECK
before evaluating operands, once, left to right. Existing eager methods
delegate to the same implementation for identical semantics. There is no
separate assertion engine or Logging dependency. Explicit Context methods
remain available for custom diagnostic descriptions. Preprocessor commas in
template arguments and braced initializers may require parentheses.

## 2026-10-30 — Source file headers and attribution

Adopt the C++ file header convention shared by the other Ariadne
repositories for every `testing` source (`.hpp`/`.cpp`): filename banner,
Copyright 2026 and Author Luca Geretti, and the GPLv3-or-later license notice
naming Ariadne Testing. The repository's LICENSE is GPLv3. This commit changes
comments only; public APIs, execution behavior and dependency graph are
unchanged. Future source files should use the same format.

## 2026-10-30 — D01: deferred comparisons and ADL predicate checks

Added a compile-time `Comparison` selector and `Context::check_compare` for
`!=`, `<`, `<=`, `>` and `>=`. As in `check_equal`, built-in non-bool
integer pairs use C++20 `std::cmp_*` to preserve negative signed vs unsigned
ordering and avoid `-Wsign-compare`, while user-defined types use their own
relational operators and evidence. Two new functions, `check_unary` and
`check_binary`, invoke arbitrary C++ predicates (including ADL calls),
render the arguments and preserve evidence: True/False/Indeterminate. For
negation, `detail::negate(Evidence)` swaps True/False and leaves
Indeterminate unchanged, instead of using boolean negation of a validated
predicate. All checkers announce and flush the check event before evaluating
operands, which are evaluated exactly once, left to right.

Added optional thin expression macros `ARIADNE_EXPECT_NOT_EQUAL`,
`ARIADNE_EXPECT_LESS`, `ARIADNE_EXPECT_LESS_EQUAL`,
`ARIADNE_EXPECT_GREATER`, `ARIADNE_EXPECT_GREATER_EQUAL`,
`ARIADNE_EXPECT_UNARY_PREDICATE`, `ARIADNE_EXPECT_NOT_UNARY_PREDICATE`,
`ARIADNE_EXPECT_BINARY_PREDICATE` and
`ARIADNE_EXPECT_NOT_BINARY_PREDICATE` in `testing/checks.hpp`.
The macros stringify the source expression but leave test classification,
operand evaluation and diagnostics in the C++ framework. No domain-specific
relation or production dependency was added. Self-tests exercise all five
operators, integer signedness, ADL, positive/negative predicates, uncertainty,
failed-value diagnostics and progress before evaluation. GitHub CI on the
first implementation commit was initiated; full matrix status is verified
separately.

## 2026-10-30 — D02 (partial): explicit mathematical evidence expectations

Added `EvidenceExpectation` with `DefinitelyTrue`, `PossiblyTrue`,
`NotPossibly` and `Indeterminate`, and deferred
`Context::expect_evidence(callable, expectation, description)`.
The helper records the **observed evidence** and **expected evidence
intention** separately. In particular, `Indeterminate` is a passing
assertion only when explicitly expected; it remains `Inconclusive`
for a demand for definite proof or refutation. `PossiblyTrue` passes
on True or Indeterminate and fails on False: this is a non-refutation
check, **not** a proof of the proposition. Domain statuses such as SAT,
UNSAT and UNKNOWN are ordinary domain values, not automatically mapped to
Testing's `Inconclusive` outcome.

Optional `ARIADNE_EXPECT_DEFINITELY`, `ARIADNE_EXPECT_POSSIBLY`,
`ARIADNE_EXPECT_NOT_POSSIBLY` and `ARIADNE_EXPECT_INDETERMINATE`
macros use deferred lambdas and expression stringification. Existing
`Context::prove` and `ARIADNE_PROVE` retain strict proof semantics.
Self-tests verify passing expectations, unsuccessful possibility,
inconclusive proof/refutation, failed mismatched expectation and the
recorded evidence labels. Policy configuration, legacy equivalence
mapping and domain-status adapters remain on the development backlog.
The new implementation was published for the GitHub Actions CI matrix.

## 2026-10-30 — Rename definitive falsity to NotPossibly

The fourth logical expectation is now expressed using the dual of
`possibly(proposition)`: `EvidenceExpectation::NotPossibly` and
`ARIADNE_EXPECT_NOT_POSSIBLY(context, proposition)`. For a proposition
whose evidence is False, this expectation passes; when the evidence is
Indeterminate it remains Inconclusive, and when it is True it fails.
This is a terminology-only change to the public API; the previous
falsity-oriented name was removed rather than retained as a second alias.
Self-tests assert the observed/expected labels and all three outcomes.

## 2026-10-30 — D02: explicit CI acceptance and legacy migration equivalence

Implemented `AcceptancePolicy`, `InconclusivePolicy` and `SkippedPolicy` for `Summary::acceptable(policy)`. The existing `successful()` is unchanged in effect: an inconclusive proof blocks CI. Explicit NonBlocking inconclusives can be tolerated only at the exit gate; every assertion outcome, case outcome, count and reporter entry remains Inconclusive. A genuine pass is still required, while Failed and Error always block. Self-tests exercise strict/permissive gates, skipped blocking, passless suites and failure/error rejection. An expected solver UNKNOWN + ResourceExhausted is tested as ordinary domain enum equality, and passes without turning mathematical uncertainty into a proof.

### Legacy macro migration mapping (verified source semantics)

Source: `ariadne-cps/macro/include/macro/testing.hpp` on main; `function/tests/test_taylor_model.cpp` on main for local REFINES. Old `AriadneTesting::decision(x)` invokes ADL `decide(x)` or converts to bool, distinct from the `definitely/possibly` mapping used by `truth(x)` and Testing's evidence checks. **Do not mechanically substitute when a domain's decision semantics differ.**

| Legacy | Candidate replacement | Non-equivalence or required verification |
|---|---|---|
| `ASSERT(x)` | `ARIADNE_PROVE(ctx(),x)` | Old Indeterminate merely warns and does not increment failures; new Inconclusive blocks default CI. |
| `EQUAL(a,b)`, `EQUALS(a,b)` | `ARIADNE_EXPECT_EQUAL` | Old uses `decision(a==b)` and can re-evaluate operands for diagnostic printing; new captures once and preserves evidence. |
| `NOT_EQUAL(a,b)` | `ARIADNE_EXPECT_NOT_EQUAL` | Old negates the bool decision of `a==b`; new uses `a!=b` and preserves its evidence. |
| `SAME(a,b)`, `SAME_AS(a,b)` | `ARIADNE_EXPECT_SAME` | Old uses `decision(same(a,b))`; new uses the evidence classification of ADL same. |
| `LESS(a,b)`, `COMPARE(a,op,b)` | Relational/check_equal macros according to operator | Old decision semantics and possible operand re-evaluation differ. |
| `WITHIN(a,b,t)` | `ARIADNE_EXPECT_WITHIN` | Magnitude and tolerance relation remain, but old uses decision and may re-evaluate failing operands. |
| `CHECK(a,b)`, `CHECK_WARN(a,b)` | Equality or warning adapter | CHECK uses testing_check + decision; CHECK_WARN only warns and does not fail. |
| `UNARY_PREDICATE`, `BINARY_PREDICATE` | Generic predicate macros | Old decision/operand re-evaluation may differ; new evaluates once and preserves evidence. |
| `CONCEPT`, `STATIC_ASSERT`, `SAME_TYPE` | C++ compile-time checks or ordinary checks | Legacy macro assertions happen at runtime even for compile-time expressions. |
| `FAIL(statement)` | `ARIADNE_EXPECT_EXCEPTION` | Both expect an exception of any type. |
| `THROWS(statement,E)` | `ARIADNE_EXPECT_THROWS` | Both require type E; new also handles unexpected non-standard exception types as a failed expectation. |
| local `REFINES(a,b)` | Generic binary predicate + domain-local diagnostic adapter | Old additionally prints model differences, error and norm on failure, requiring a domain adapter to retain coverage. |
| `EXECUTE`, `TRY`, `CONSTRUCT`, `ASSIGN`, `PRINT`, `WARN` | Explicit C++ with check/notes | These have differing scope, side-effect and exception semantics; do not automatically replace. |

The table is guidance, **not a proof that all consumer call sites are equivalent**. Preserve test behavior during migration and review any intentional increase in proof strictness.

## 2026-10-30 — D03: suite variants and fixture factories

Added `testing/parameterized.hpp` with `SuiteVariant` (named cases
under a variant prefix), `fixture_variant<Fixture>(suite, name, factory)`
and the Context-only factory overload. Each `FixtureVariant::add`
registers a separate normal `Suite` case; a factory is invoked freshly at
**execution** rather than registration and supplies a full `Fixture` by
value, which can own a cloned or newly constructed implementation, a
runtime precision or other parameters. The factory is shared by owned
pointer for copyability of `std::function` case records, so move-only
factory captures are allowed. Non-movable fixture prvalues use guaranteed
copy elision in C++20. Methods are invoked exactly once; unexpected
factory exceptions are classified `Error` by the existing runner. No
fixture state is shared between cases unless the user deliberately shares
external state in the factory.

`SuiteVariant::add_typed<Tag>` instantiates a generic callback separately
for each compile-time tag (such as Approximate, Validated, Effective).
`ARIADNE_ADD_VARIANT_TEST(group, method)` and its `NO_THROW` form
stringify the method token and delegate to C++ member invocation.
The optional prefix `test_` is stripped from display names, preserving
current fixture conventions. Variant names appear in full case paths;
the existing one-based RUN index and per-case CHECK index are unchanged.

A dedicated `test_parameterized.cpp` executable exercises multiple
implementations with an Affine-style subset, runtime precision, move-only
factory, non-movable fixture, compile-time paradigms, empty-name rejection,
factory errors, counter reset and pre-factory RUN flush. Added to CTest
on every supported platform. No dependencies on Integration, Numeric or
Function were introduced. Explicit shared-fixture lifetime and migration of
actual consumer tests remain outside this implemented API.

## 2026-10-30 — D04: scoped diagnostic events (implementation)

Added `Context::Scope` (RAII, move-only) and `Context::scope(label)` /
`Context::scope(key,value)`, which record and flush a scoped progress event
before executing user code. Nesting is captured in each `AssertionRecord`
without changing the two-level case.check numbering. Scope destruction
restores the parent context; when stack unwinding reaches `Suite::run`,
its error assertion receives the captured deepest active scope.

`DiagnosticKind` and `DiagnosticEvent` separate non-asserting Scope,
Note, Warning, Value and Elapsed events from evidence checking. The
`Context::note`, `warning`, `inspect`, `elapsed` and `measure` APIs
store provenance and produce events through an optional reporter callback.
`measure` records elapsed time even when its callable throws, but cannot
bound or cancel a hanging operation. Warning and Scope events are always
flushed by ConsoleReporter; `ConsoleDetail::Concise` defers Note/Value/
Elapsed details until a case fails and never suppresses failed assertions.
There is no dependency on Logging or domain-specific TSV data readers.

Self-tests cover nested scopes, pre-work flush, warning visibility,
notes and elapsed diagnostics on failure, captured context on failure
and exception, and independence across cases. This change does not
alter mathematical assertion outcomes or the Suite summary semantics.

## 2026-10-30 — D05 (partial): bounded future and condition waits

Implemented `Context::expect_ready(future, timeout, description)` and
`Context::expect_eventually(condition_variable_any, unique_lock, predicate,
timeout, description)`. Both emit and flush CHECK before waiting, evaluate
future status or a condition under the supplied lock, and retain elapsed
milliseconds and deadline in failure details. Ready and satisfied states
pass; timeout and deferred futures fail explicitly. Negative timeouts are
classified as Error. The code never attempts to cancel or detach workers.
For a truly deadlocked worker, an independent process-level CTest timeout
remains necessary. Added synchronous, deterministic self-tests for ready,
timeout, deferred, satisfied and unsatisfied statuses. No claims of
cross-thread Context safety or worker exception aggregation are made;
these remain in development.md.

## 2026-10-30 — Fixture shorthand and conventional suite entry point

The canonical expression-checking macros take an explicit context and have a `_CTX` suffix (e.g. `ARIADNE_EXPECT_EQUAL_CTX(context, actual, expected)`). The unsuffixed macros are fixture-only shorthand using `this->ctx()`, and preserve expression stringification and single evaluation by forwarding to `_CTX`. They are not valid in free functions or ordinary Context& lambdas; no global or thread-local context is introduced.

For the conventional single-suite main, `ARIADNE_TEST_SUITE(name)` declares a local variable `ariadne_test_suite`; `ARIADNE_ADD_TEST(Fixture, method)` and `ARIADNE_ADD_TEST_NO_THROW(Fixture, method)` use it, and `ARIADNE_RUN_TESTS()` evaluates the standard console exit gate. The explicit `ARIADNE_ADD_TEST_TO(suite, Fixture, method)` and `ARIADNE_ADD_TEST_NO_THROW_TO` variants continue to support independent suites in one executable. `run_suite(suite, output)` is the underlying helper; callers may still instantiate reporters directly. These naming changes require downstream migrations from old explicit-context and explicit-suite macro signatures. No assertion or suite outcome semantics change.

## 2026-10-30 — Public umbrella header

Added `include/ariadne-testing.hpp` as the recommended single include for
clients. It exposes the core `Suite`/`Context` API, `ContextFixture`
and suite macros, expression assertion macros (including explicit `_CTX`
forms), and parameterized suite/fixture variants. It does not alter any of
the underlying component headers, their independence, or test semantics.

Both framework self-test executables now include the umbrella header to
exercise it. The CMake public-header registry includes the new root-level
header via `FILES`, alongside the existing `include/testing` directory,
so standalone install bundles include both the new entry point and the
individual headers. Downstream `utility/testing` may replace its Testing
component includes with the umbrella header without modifying test bodies.

## 2026-10-30 — Test-only distribution (correction to umbrella entry)

The public umbrella header `include/ariadne-testing.hpp` is used from the
build tree, not an installed SDK. The earlier note about registering it for
installation is superseded: the Testing framework must **not be installed**.
Removed the install-interface include directory, public header registration,
standalone `setup_project_library` packaging and install bundle. The
`ariadne-testing` target remains available as a build-tree static library
composed from `TESTING_SRC` and linked with the `testing` include interface;
self-tests and downstream consumers use it only for test executables. Utility
continues using the nested `TESTING_SRC` archive, without any production
link dependency on Testing. No assertion behavior changes.

## 2026-10-30 — Adopt centralized test-only CMake integration

Pinned Configuration at `a3367703c0f95af09b823560f19c6830aaa760fc`.
Its optional `setup_standalone_project_tests(USE_ARIADNE_TESTING)`
initializes Testing solely for standalone test builds. The framework's
build-tree static target `ariadne-testing` is reused directly by downstream
tests rather than repackaging `TESTING_SRC` into a project-specific archive.
No installation rules or production-library dependencies are introduced.

## 2026-10-30 — Automatic Testing discovery in Configuration

Configuration commit `d57304ec9d37ba27f99a9f28e23b9e0272d15b0a` removes the explicit
`USE_ARIADNE_TESTING` option. `setup_standalone_project_tests`
automatically reuses an existing `ariadne-testing` target, initializes
Testing when declared as a Git submodule, or leaves ordinary CTest alone
if absent. Testing's own standalone self-tests do not require a self-submodule.
Testing stays build-tree-only, uninstalled and absent from production targets.

## 2026-10-30 — D09: fatal prerequisites and typed exception messages

Added `Context::require` and the `ARIADNE_REQUIRE` / `ARIADNE_REQUIRE_CTX`
macros. Checks are announced before their predicates are evaluated once.
Failed or indeterminate requirements record their original evidence-derived
outcomes, then stop only the current case using an internal `AbortCase`
marker. `Suite::run` recognizes that marker without synthesizing an Error.
The preexisting exception checkers rethrow it rather than incorrectly
counting a failed prerequisite as an expected exception. Exceptions thrown
while evaluating a requirement remain uncaught test Errors. A caller's
own catch-all around a fatal assertion can swallow the internal marker;
test authors must not do this. Threaded case abortion is unsupported.

Added `expect_throws_message<Exception>` and the corresponding fixture and
explicit-context macros. Only `std::exception`-derived exception types are
supported, matching a substring of `what()` and preserving actual and
expected strings in assertion records; missing/wrong types and messages
fail rather than passing. Arbitrary exception payload checks remain in D09.
Dedicated `test_requirements.cpp` covers fatal aborts, preserved evidence,
exception interception, source-order evaluation, message mismatch, wrong
exception type, and subsequent case execution. No production dependencies,
new installation rules, or changes to existing tests are introduced.

The README now exposes the same six status/coverage badges as other Ariadne
repositories, pointing to Testing's own workflows and Codecov page.

## 2026-10-30 — D09: generic typed exception payload checks

Added `Context::expect_throws_payload<Exception>(action, projector, expected, description)`
with fixture and `_CTX` macros. The action is evaluated after CHECK; on a
matching `std::exception`-derived type a caller-supplied projection extracts
the payload, then the expected expression is evaluated. Each expression is
invoked once, and both are formatted into the normal assertion record.
Built-in mixed-sign integral equality uses the same safe comparison rules as
ordinary equality checks; domain comparisons preserve indeterminate evidence.
Wrong exception type or no exception is Failed; errors evaluating the
projection/expectation are Error, retaining the exception diagnostic. Internal
fatal-requirement aborts propagate rather than being counted as an exception.

Added dedicated `test_payload.cpp` self-tests for matching, mismatching,
indeterminate, wrong and missing exception, throwing projector/expected
expression, mixed signedness and fixture shorthand. No new dependency,
production linkage or install rule is introduced. The remaining D09 work
concerns normal C++ replacements for legacy construction/evaluation macros.

## 2026-10-30 — D07: range equality and structured mismatch paths

Added `Context::check_ranges` with default evidence-preserving equality,
user-supplied binary comparator and optional index-to-path projector. Its
`ARIADNE_EXPECT_RANGES`, `_BY`, `_AT` macros and explicit `_CTX` forms
defer range/comparator evaluation until after the indexed CHECK event.
Input ranges, including single-pass iterator ranges, are visited once and
compared without copying elements. Diagnostics record complete actual and
expected element counts, the first mismatch index and optional structured
path plus rendered snippets truncated to 96 characters. If an earlier
comparison is indeterminate and no definite mismatch occurs, the case stays
Inconclusive; a later false comparison or size mismatch makes it Failed.
Comparer exceptions are ordinary case Errors and are not concealed. No
unbounded artifact dumps, domain-specific tree dependencies, or source
installation requirements are introduced. A separate `test_ranges.cpp`
exercises equal/mismatched lengths, singleton and missing elements,
heterogeneous integral equality, indeterminate comparator, path selection,
input iterators and fixture shorthand. Concrete tree adapters are delegated
to downstream repositories rather than included in the framework.

## 2026-10-30 — D07: MSVC callable capture portability fix

Windows/MSVC compilation of the D07 self-test failed when a free function
(`same_value`) was passed to `ARIADNE_EXPECT_RANGES_BY_CTX`: the macro's
`decltype(auto)` factory returned a function designator, which MSVC rejected
with C3556/C3169. This was a macro portability bug, not an sccache failure,
and the existing semantic test remains in place.

The comparator and optional path factory macros now call
`detail::capture_callable` after the CHECK notification. It keeps lvalue
callables (including named free functions and non-copyable functors) through
`std::reference_wrapper` and owns rvalue callable temporaries by move,
without repeating expression evaluation. The underlying range comparison,
evidence classification, mismatch reporting, and test case inventory are
unchanged, except for additional regressions covering the capture modes.
Verification by the new Windows CI run remains required.

## 2026-10-30 — D06: generic artifacts and golden-regression files

Added `ArtifactRecord` with path, label, media type, kind, lifetime and metadata. A Context can create a per-case isolated temporary directory with `work_directory()`, register references with `artifact()`, and check file existence, byte-for-byte content and binary signatures. Each file assertion announces CHECK before filesystem inspection. Byte comparison reports the offset of the first mismatch; the framework does not parse PNGs or regenerate golden references.

Managed outputs must reside beneath the owned work directory. It is removed after the reporter has consumed the result unless a registered output specifies `KeepAlways` or `KeepOnFailure` on a non-passing case. External files are never deleted or written by cleanup. The framework cannot preserve outputs when the process terminates abnormally, and only the output-directory ownership is managed; arbitrary external files remain the caller's responsibility.

Added `test_artifacts.cpp` covering a passing case cleanup, retained failure output, unchanged golden reference, missing file, unequal bytes and mismatched signature. Testing remains independent of rendering libraries and keeps no install rules.

## 2026-10-30 — D06 regression correction: directory identity and diagnostics

The initial D06 commit failed all Debug/CTest workflows because its artifact self-test
returned a bare nonzero code. Consecutive Context instances reuse a stack address;
the work-directory naming scheme based on that address also reused the same path,
so an existing retention directory could hide the expected successful-case cleanup.
The allocator now uses an atomic monotonically increasing candidate counter and
atomic create_directory for cross-process collision detection. Each successful
allocation receives a distinct path. The golden fixture's external file name is
also derived from the isolated case directory instead of a constant global name.
The self-test emits descriptive stderr on every failed postcondition (directory
identity, retention, cleanup, expected results, artifact records, byte offset),
so CTest --output-on-failure gives actionable evidence rather than a bare exit code.
The expected test outcomes and D06 semantics are unchanged. This is a regression
fix; platform validation must be checked on its own commit.

## 2026-10-30 — D06 correction: missing files are failures, not errors

On macOS and GCC/libstdc++, `std::filesystem::is_regular_file(path, error)`
may report `no_such_file_or_directory` for an absent file. The original
`expect_file_exists` implementation classified *any* nonempty error code
as `Outcome::Error`, causing the missing-file self-test to produce a
wrong suite summary. Treat `no_such_file_or_directory` and
`not_a_directory` as `Failed` (an unmet file expectation); retain
`Error` for unrelated filesystem errors. The test explicitly covers
missing leaf and nested paths, non-regular directories, each assertion's
classification, and prints exact summary counters on mismatch.
The correction preserves test semantics: it fixes an incorrect assertion
outcome rather than altering the expected summary to pass CI.

## 2026-10-30 — D10: scoped test-state preservation

Added header-only `testing/state.hpp` to the umbrella API. `preserve_value(value)` snapshots and restores a copyable, noexcept-assignable value; `preserve_state(getter, restorer)` supports domain-specific state with a single getter call and a statically required noexcept restorer. The returned `ScopedState` guard cannot be copied or moved. Restoration occurs on normal scope exit, exception unwinding, and fatal assertion abort without changing the assertion outcome. Nesting restores the immediately preceding value. Non-noexcept restorers are rejected at compile time, preventing a second exception from escaping a destructor during unwinding. Domain libraries implement their own adapters without depending on Logging or Graphics. The existing fixture policy remains fresh construction per case; shared lifecycle is not implicitly enabled.

`test_state.cpp` covers normal restoration, nested scopes, per-case isolation, exception unwinding, custom getter/restorer evaluation counts, and fatal prerequisite aborts, with explicit diagnostics on failures. D10 setup/teardown and shared-fixture ownership remain open.

## 2026-10-30 — Coverage playbook: fixture.hpp targeted missing paths

The user's LLVM 21 coverage attachment for `testing/main` at commit
`de203692596b1da97adba4e9aaca322faa385368` reported `fixture.hpp`
at 6/7 functions, 28/29 lines, and 4/6 branches. Inspection of the
per-instantiation branch records identified the unexecuted const `ctx()`
accessor (line 45), the false branch of `starts_with("test_")` (line 60)
for the existing `Fixture`, `FixtureExample`, and `CompactFixture`
instantiations, the `NoThrow` path in `Fixture` (line 63), and the
unsuccessful `run_suite` return (line 74).

Added behavioral self-tests in the existing `test_testing.cpp` and
`test_requirements.cpp`, without manufacturing new fixture types or changing
production semantics. Each verifies a concrete public behavior: a const
fixture sees the same context, explicit unprefixed case names are preserved,
`NoThrow` wraps a normally successful fixture and records its completion,
and an actual failed assertion makes `run_suite` return 1 with a FAIL
diagnostic. The existing prefixed and direct-path cases remain unchanged.

The baseline and the proposed changes are distinct: a fresh coverage report
for the new commit is needed to verify any actual metric improvement. The
official tool summary, including template-instantiation effects, controls
whether the file can be called fully covered.

## 2026-10-30 — Coverage playbook: parameterized.hpp branch outcomes

The user's latest LLVM 21 coverage attachment (after commit
`22fc00fb1f892dd524e42d1f6a185f50210a511d`) confirms that
`fixture.hpp` reached 7/7 functions, 29/29 lines and 6/6 branches.
`parameterized.hpp` remained at 15/15 functions, 77/79 lines and
5/8 branches. The per-instantiation records identify the name guard in
`SuiteVariant::add` (line 52), the `test_` prefix guard in
`FixtureVariant::add` (line 104), and the `FixtureCheck::NoThrow`
dispatch (line 111). Different template instantiations individually
exercise only one of the two outcomes.

The new cases in `test_parameterized.cpp` deliberately reuse the same
`SuiteVariant::add` callable type with an accepted and a rejected name,
verifying that an invalid name is rejected without registering a case.
Another fixture variant uses the identical method callable with a prefixed
and unprefixed name, choosing Direct and NoThrow respectively, then
checks case names, assertion counts, outcomes and the completion check.
No framework source, fixture semantics or CMake coverage scope changed.

The expected effect is improved source-level line/branch coverage in
`parameterized.hpp`, but the numerical delta must be confirmed from
a new official coverage report for the published commit.

## 2026-10-30 — Public namespace flattened to Ariadne

The public and implementation C++ namespace is now `Ariadne`, rather than
`Ariadne::Testing`. All check macros, fixture/parameterized helpers, state
guards, built-in reporters, library self-tests and README examples use the
new namespace. No compatibility alias to the removed namespace is provided;
this is an intentional source-level API break. The `ariadne-testing` CMake
target, file paths and the build-tree-only/no-install boundary stay unchanged.
Utility updates its nine test executables and pinned Testing gitlink separately.

## 2026-10-30 — Public Evidence enumerator collision with Utility

Moving the Testing API from `Ariadne::Testing` to `Ariadne` caused
GCC `-Wshadow -Werror` failures when test clients included both the
framework and Utility. Utility already defines `True` and `False` as
`std::true_type` and `std::false_type` aliases; these are compile-time
types, whereas the Testing `Evidence` enumerators are runtime three-valued
logical classifications. The two are not interchangeable, and Testing must
remain independent of Utility.

Renamed `Evidence::True` to `Evidence::Proven` and `Evidence::False`
to `Evidence::Refuted`, retaining `Evidence::Indeterminate`. Updated all
comparisons, negation and outcome conversion without changing behavior.
The self-test includes an explicit compile-time check for the new enumerators.
No warning flags are suppressed, and the namespace remains `Ariadne`.
