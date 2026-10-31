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
negation, `Detail::negate(Evidence)` swaps True/False and leaves
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
`Detail::capture_callable` after the CHECK notification. It keeps lvalue
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

## 2026-10-30 — Revert public namespace flattening

After an integration review, the Testing C++ API remains in
`Ariadne::Testing`. The attempted flattening to `Ariadne` (commit
`3de42fa`) conflicted under GCC `-Wshadow -Werror` with Utility's
`Ariadne::True` and `Ariadne::False` type aliases. Renaming the
`Evidence` enumerators (`cb82137`) did not address the broader risk
of colliding with existing declarations such as Foundation's
`Ariadne::Indeterminate`. Rather than introduce a new dependency or
rename further public symbols, restore all library source, macro, test
and README contents from `ba944cf`, before both changes, preserving
all existing framework functionality and coverage self-tests.

The original `Evidence::{False,True,Indeterminate}` values remain
scoped inside `Ariadne::Testing`; `ContextFixture` retains its
constructor-inheritance requirement. File names, CMake targets and
test-only integration are unchanged. Utility restores the original
test-source namespace qualifications and updates its pinned `ss/tst`
gitlink to this restoration commit.

## 2026-10-31 — testing.cpp coverage: move ownership of a diagnostic scope

The user's LLVM 21 report from 2026-10-31 confirms `parameterized.hpp`
at 15/15 functions, 79/79 lines and 8/8 branches, so the earlier
parameterized coverage intervention is closed without further tests.
It identifies a single unexecuted function in `src/testing.cpp`, the
`Context::Scope` move constructor. Its valid behavior is transferring
ownership of scope closure without prematurely leaving the diagnostic
context or closing twice. A targeted case in `test_testing.cpp` now
constructs a scope, explicitly moves it, verifies the active scope on the
first assertion and verifies that the context has been closed at the
second assertion. The expected suite count and RUN/CHECK indices are
updated to reflect the additional case. The framework implementation is
untouched. A fresh coverage report for this commit is required before
claiming complete function coverage for `testing.cpp`.

## 2026-10-31 — testing.cpp coverage: registration contracts and nonstandard exceptions

The user-supplied 2026-10-31 LLVM 21 report for the post-scope-move
revision reports `src/testing.cpp` at 31/31 functions, 273/301 lines
and 161/206 branches, confirming the previous move-constructor
coverage improvement. The report's direct branch records show
`Suite::add` at line 351 has no execution for the empty-name or
empty-callable branch (both True counts 0), and `Suite::run` lines
374–377 have no execution for an unhandled non-`std::exception`
throw. These represent valid public API/error-policy paths.

Added `test_testing.cpp` cases verifying that both forms of invalid
registration throw `std::invalid_argument` without registering cases;
that a test throwing an integer is classified as `Error`, with an
explicit unhandled-exception assertion at index zero; and that a
following case still executes and passes. Both independent registration
guard outcomes and the exception handler should be exercised by this
change. No framework implementation or coverage object selection
changed. Verify the exact delta against the next official coverage
report before marking these gaps closed.

## 2026-10-31 — LLVM coverage playbook: D06 managed artifact semantics

The user-supplied macOS LLVM 21 archive dated 2026-10-31 13:51, after
commit `bf9d7ba`, reports 172/172 functions (100%), 898/1001 lines
(89.71%), and 265/370 branches (71.62%). Compared with the preceding
895/1001 and 263/370 baseline, this is +3 lines and +2 branches.
`testing.cpp` is 31/31 functions, 276/301 lines and 163/206 branches.
`fixture.hpp`, `parameterized.hpp` and `state.hpp` remain fully
covered for applicable metrics.

Inspection of `coverage/branches.txt` points to real, untested D06
behaviors in `testing.cpp`: empty artifact path and label (line 188),
managed path not inside the owned directory (lines 195-197), successful
byte-for-byte file equality (lines 241-245), missing golden reference
(lines 228-231), missing file signature (lines 253-257), KeepAlways
retention and conditional cleanup on success (lines 150-152).
The self-test adds corresponding end-to-end cases, verifying exact
outcomes, diagnostic strings, the absence of rejected artifact records,
metadata preservation, filesystem cleanup and retention. It uses
isolated per-case directories and explicitly removes retained test data.
No production code or coverage scope is modified.

The allocator's exhaustion and exceptional filesystem branches, invalid
internal enum defaults and other practically unreachable paths are not
targeted merely to increase branch counts. A new official LLVM report
is required before claiming a coverage increase from this test change.

## 2026-10-31 — LLVM 21 coverage update and real error-path checks

The user-supplied archive `Archivio 6(5).zip` contains a macOS LLVM 21 report
for the test-only D06 increment `0e7fc96`: 172/172 functions (100%),
906/1001 lines (90.51%), 524/616 regions (85.06%) and 276/370
branches (74.59%). Against the previous report for `bf9d7ba`,
this represents +8 covered lines and +11 covered branches without
adding production code. In `src/testing.cpp`, the report shows 31/31
functions, 284/301 lines and 174/206 branches. The D06 checks for
registration, retention and file comparison have materially improved
the corresponding branch counts.

Inspection of `coverage/branches.txt` now identifies actionable
`include/testing/testing.hpp` gaps in negative-duration handling for
`expect_ready` and `expect_eventually` (lines 765-767, 808-810),
typed exception absence and nonstandard exception classification
(lines 654, 680-682, 716-718, 728-730), and range diagnostics for
the first indeterminate element followed by a definite mismatch
(line 489) and truncated long values (lines 433-434).
The new self-tests exercise these through the public APIs, verifying
their observable outcomes, non-evaluation of rejected wait predicates,
diagnostic strings, preserved mismatch/indeterminate indices,
bounded output length, empty-range equality and continuation to
subsequent cases. No framework code, baseline expectations or
coverage selection has changed.

Other unexecuted branches include impossible enum switch defaults,
allocator exhaustion and toolchain-specific per-instantiation
branches. These are not targeted with artificial tests merely to
satisfy coverage metrics. A fresh official LLVM report is needed
to confirm the effect of the newly published tests.

## 2026-10-31 — Coverage playbook: checks.hpp macro-operand branches

The user-provided LLVM 21 report `Archivio 7(6).zip` shows
`include/testing/checks.hpp` with 58/58 functions, 58/58 lines,
72/75 regions and 9/14 branches. All five uncovered branches are
attributed to expressions expanded inside the one-evaluation macro
wrappers: one at line 42 (`ARIADNE_EXPECT_EQUAL_CTX`) and four at
lines 105-106 (`ARIADNE_EXPECT_LESS_CTX`). Their source is conditional
error handling in `test_testing.cpp`'s CHECK-before-evaluation and
operand-order tests, not independent conditions of the macros themselves.

The same lexical macro invocations are now exercised repeatedly under
both successful and failing conditions. When a console reporter does
not emit progress to the guarded output stream, or an operand detects
the wrong evaluation order, its exception must propagate to
`Suite::run` as a single Error record. The positive execution is
unchanged; separate test runs verify the first-operand, missing-CHECK
and second-operand failures with their actual error messages. Reusing
the same lambda bodies, rather than duplicating them, is essential to
exercise the instrumented branch instances in the uploaded report.
No assertion expectations were weakened and no framework source or
coverage selection changed. A fresh official LLVM report must
confirm the branch and region delta before declaring `checks.hpp`
fully covered.

## 2026-10-31 — checks.hpp coverage closed (LLVM 21)

The user-supplied `Archivio 8(3).zip` coverage report generated on
2026-10-31 confirms `include/testing/checks.hpp` at **58/58 functions,
58/58 lines, 75/75 regions, and 14/14 branches (100% each)**. The
preceding report showed 72/75 regions and 9/14 branches, so the targeted
regression tests in commit `0afabca` closed the remaining three regions
and five branches. The corresponding Ubuntu, macOS, Sanitizers and
Coverage GitHub Actions completed successfully; Windows validation
was still running when this checkpoint was recorded.

Repository-wide metrics in the same report are **173/173 functions
(100%), 917/1002 lines (91.52%), 536/623 regions (86.04%), and
286/374 branches (76.47%)**. `fixture.hpp`, `parameterized.hpp`,
`state.hpp`, and `checks.hpp` have complete coverage under their
applicable metrics. `testing.hpp` remains at 56/56 functions,
445/513 lines and 84/140 branches; `testing.cpp` remains at
31/31 functions, 284/301 lines and 174/206 branches.

`checks.hpp` is closed under the coverage playbook. Future coverage
work should focus on behaviorally meaningful remaining gaps in
`testing.hpp` and `testing.cpp`; do not introduce artificial scenarios
or weaken expectations merely to inflate the metrics.

## 2026-10-31 — Windows filesystem boundary self-tests

Previously, the managed-artifact escape and filesystem-error self-tests were
entirely excluded on `_WIN32`, leaving Windows CI without either check.
The artifact fixture now uses a directory junction on Windows (via `mklink /J`),
which does not require the symbolic-link privilege, and a symbolic directory
link on POSIX. Both must resolve to the external fixture, reject a managed
artifact through the link, and accept an ordinary nested output. An exit guard
removes the link before the case directory is cleaned, even on exceptions.
The filesystem-error fixture continues testing an ELOOP link on POSIX.
An initial Windows fixture using an invalid filename containing `*` did not
produce an OS error under MSVC: `is_regular_file` classified it as absent,
causing both Windows Debug configurations to fail in `test_boundary_matrix`
on commit `bb7f23e`. This was an incorrect test expectation, not evidence
that the library must change its error classification. The Windows fixture
now creates a directory junction back to its parent and traverses more than
the Win32 reparse-point limit; the cleanup guard removes the junction before
work-directory cleanup. Both platforms assert a genuine filesystem `Error`
and a diagnostic. Windows CI validation is required for this correction.

## 2026-10-31 — Filesystem status errors on Windows and POSIX

Windows CI for `252334e` still failed `test_boundary_matrix`: the
cyclic directory junction did not yield an `Outcome::Error` assertion.
The preceding invalid-`*` filename fixture failed for the same reason.
MSVC's `std::filesystem` maps several native Windows errors to
`file_type::not_found`; constructing either fixture without checking the
observable filesystem error was an unsupported expectation.

The regression now exercises a valid public-API boundary shared by all
platforms: a filename component of 1024 characters, exceeding filesystem
component limits. The fixture first establishes that `is_regular_file`
reports a non-absence `error_code` (rather than assuming the platform's
mapping), then requires `Context::expect_file_exists` to record one
`Error` assertion with a nonempty diagnostic. The existing POSIX `ELOOP`
fixture is retained as a separate, POSIX-specific link behavior; Windows
is never left without an actual assertion for the shared Error contract.
The two previous Windows-only guesses are removed, not turned into skipped
or vacuous tests. MSVC CI confirmation is still required.

## 2026-10-31 — D08 part 1: typed domain status expectations

The new `Context::expect_status` checks exact typed domain statuses after
announcing CHECK, and conditionally verifies the returned result only when its
status is permitted. The verification predicate returns bool or three-valued
mathematical evidence: its uncertainty produces Inconclusive, but a deliberately
expected domain UNKNOWN with the correct reason passes. `test_status.cpp`
exercises SAT, UNSAT, UNKNOWN/reason, missing witnesses, unsupported statuses,
uncertain postconditions, error propagation, evaluation counts and CHECK order.
No domain library dependencies or platform-specific test omissions were added.
D08's separate result/exception alternatives remain to be implemented.

## 2026-10-31 — D08 part 2: result/exception alternatives

`Context::expect_alternative(operation, verify_result, verify_exception, description)`
completes D08 with an explicit, deferred choice between a returned result
and a `std::exception` thrown by the operation. The operation executes once,
after the numbered CHECK event, and only the matching validator runs.
Each validator returns boolean or three-valued evidence, preserving
Passed/Failed/Inconclusive classification. An exception verifier can
`dynamic_cast` the `std::exception` reference and check domain-specific
types, partial results, deadlines and other payloads without dependencies
from Testing to consumers. Non-standard operation exceptions are always
Failed rather than implicitly accepted. Exceptions originating from either
validator are recorded as Error (not mistaken for an allowed operation
exception), while a fatal `require` preserves the AbortCase propagation
contract. A void operation presents `std::monostate` to its result validator;
reference-returning operations are outside this API.

`test_alternatives.cpp` exercises accepted complete/partial/deadline paths,
incorrect result/exception types and payloads, nonstandard operation errors,
exception-producing validators, uncertain postconditions, void operations,
CHECK-before-operation notification, exactly-once evaluation and continued
case execution. No platform/compiler-specific exclusions and no new
production dependencies were introduced. The final release gate is GitHub
Actions GCC/Clang/MSVC, sanitizers and coverage on the published commit.

## 2026-10-31 — D05 part 1: atomic progress, bounded waits and CTest watchdog

The generic `Context::expect_atomic(state, predicate, timeout, description)`
observes an externally updated `std::atomic<T>` using acquire loads and
waits only up to a `steady_clock` deadline, with a bounded 1 ms polling
backoff. A worker may publish progress using release stores without ever
touching the owning Context or Reporter. The predicate returns bool,
must be nonblocking and is evaluated on the owning case thread. The
assertion records the last observed value, elapsed time and the timeout
limit, distinguishing Passed/Failed from an invalid negative timeout
(Error). A zero timeout still permits an already-satisfied predicate.
This API does not use `atomic::wait`, because C++20 provides no timeout
for that operation, and does not cancel or join worker threads.

The new `test_progress.cpp` exercises already-met, externally published,
zero/positive timeout, negative timeout and throwing-predicate cases.
It synchronizes the worker through promises rather than requiring a
specific scheduling delay; its timeout is a generous upper bound, not
an assertion about scheduler timing. Context and Reporter stay owned by
the runner thread; concurrent worker-issued checks and deterministic
result aggregation are still open D05 tasks.

CTest now applies a 90-second *process-level* timeout to every framework
self-test target. This is the watchdog for real deadlocks that cannot
be safely cancelled inside a C++ test process. Downstream repositories
remain responsible for their CTest timeout policies. No compiler- or
OS-specific skip was added. CI validation is required before marking
this increment as fully verified.

## 2026-10-31 — Public macro parity for D05 and D08

An audit showed `checks.hpp` had not acquired shorthand macros for the
public `expect_status`, `expect_alternative`, `expect_atomic`,
`expect_ready`, and `expect_eventually` methods. This was a gap in the
public API conventions, not a missing implementation of the underlying
assertions. Added documented paired `ARIADNE_EXPECT_*_CTX` (explicit
Context) and fixture `ARIADNE_EXPECT_*` wrappers, including a status-only
overload. The expression-style operations and predicate callbacks are
deferred through lambdas; existing status lists, synchronization objects
and timeout values are ordinary C++ arguments. Because C preprocessor
arguments split at commas, inline `std::initializer_list` expressions
must be parenthesized. All semantic classifications remain within the
original `Context` APIs. `test_check_macros.cpp` covers the explicit
Context and fixture variants, result and exception alternatives, deferred
evaluation and CHECK notifications, positive/negative outcomes and
diagnostic descriptions. Added a maintenance rule to the development
backlog requiring macro parity or an explicit rationale for C++-only APIs.
The change does not special-case any OS or compiler. CI results on the
published commit are required before the new wrappers are called verified.

## 2026-10-31 — D05: synchronized worker assertions

`WorkerBatch` stages worker-local checks under a mutex without exposing
`Context` or `Reporter` to worker threads. Fixed numbered slots are run
once each, and `batch.flush(context)` is called by the case thread after
all worker threads have joined. This emits CHECK and assertions in slot
index and within-slot order, independently of scheduler interleaving.
The original worker source location, evidence outcome and error diagnostic
are preserved. Uncaught worker exceptions become Error; zero-check workers
also become Error rather than vacuous success. Missing, duplicate and
prematurely flushed slots are rejected. The documented
`ARIADNE_WORKER_CHECK` macro is the thin deferred-expression convenience
wrapper; `Worker::check` is also available directly.

`test_worker.cpp` exercises synchronized cross-thread interleaving,
ordered replay, contradictory outcomes, error details, slot lifecycle,
exactly-once evaluation and a following unaffected case. All worker
CHECK events are deliberately deferred to the owner-thread flush;
therefore this is not streaming progress from worker threads. CTest
retains responsibility for hung/deadlocked workers. No platform-specific
test omission has been introduced; CI verification is required.

## 2026-10-31 — D05: shared monotonic deadlines

Introduced three explicit `std::chrono::steady_clock::time_point` overloads
`Context::expect_ready_until`, `expect_eventually_until`, and
`expect_atomic_until`, without changing existing duration-based methods.
One caller-created timestamp is reusable across consecutive checks; none of
the checks restarts the original time budget. Each first observes the
synchronization primitive, including when the deadline is already in the
past. Already-satisfied futures, conditions and atomics therefore pass
even with an expired deadline; unmet conditions fail immediately.

The futures preserve `ready/timeout/deferred` results without consuming the
future. Condition variables use the standard predicate-based
`wait_until(lock, deadline, predicate)` to tolerate spurious wakeups. Atomic
progress uses acquire loads, a bounded 1 ms backoff, and calls the predicate
on the case thread. All checks report elapsed wait time, remaining budget
at check start, and observed deadline overrun in milliseconds; these are
timing observations, *not* proof of deadlock, scheduler culpability or an
operation's benchmark duration. External process-level CTest watchdogs
remain necessary for truly hung workers. The six public macros
`ARIADNE_EXPECT_{READY,EVENTUALLY,ATOMIC}_UNTIL[_CTX]` mirror the new APIs,
with Doxygen documentation and fixture shorthand. No OS/compiler-exclusive
test path was introduced.

`test_deadlines.cpp` checks expired-but-satisfied vs expired-unsatisfied
states, a deferred future, synchronized cross-thread publishing under one
reused deadline, a positive timeout, throwing predicates and both macro
variants. A green result across the new GitHub Actions matrix is required
before treating this increment as verified.

## 2026-10-31 — Correct D05 deadline predicate-count expectation

CI on `bdf380b` failed `test_deadlines` across the tested platforms with
"shared deadline summary, predicate calls or CHECK count". The test had
incorrectly required exactly three calls across three predicate-based
`condition_variable_any::wait_until` checks. The C++ condition-variable
predicate contract does not promise one evaluation per invocation; an
already-expired, unsatisfied wait may evaluate its predicate again at timeout.
An isolated C++20 reproduction on GCC and Clang confirmed two evaluations
for this exact case.

The corrected self-test tracks each of the three condition-variable
scenarios separately and verifies that every expected predicate is exercised
at least once, while independently retaining exact CHECK/event counts,
assertion outcomes, timing diagnostics and the atomic predicate count.
The test no longer asserts an unspecified property of the standard library
or makes an OS-specific exclusion. No runtime logic or expected progress
outcome has changed. The CI matrix on this follow-up commit is required
to establish full verification.

## 2026-10-31 — D05 structured waiting observations

All six bounded future, condition-variable and atomic wait checks (relative
and shared absolute deadlines) now attach optional `WaitObservation` data to
the `AssertionRecord`, preserving the previous outcome and textual diagnostic.
The recorded `WaitStatus` is one of Ready, Timeout, Deferred, InvalidTimeout
or Unexpected, with elapsed waiting time and the budget available at check
start (negative for invalid relative timeouts). An absolute-deadline flag
distinguishes original shared budgets from relative timeouts. No observation
assigns a cause to a timeout: scheduler slowdown, starvation, deadlock and
a functional failure remain distinct possibilities. `elapsed` and `measure`
remain nonasserting diagnostics rather than performance verdicts.

`test_timing.cpp` verifies ready, timeout, deferred and invalid observations,
both deadline modes, budget preservation and separation from elapsed notes.
Existing assertion construction remains source-compatible because the new
field is optional with a default. No platform-specific tests were excluded.
This increment requires CI verification.

## 2026-10-31 — D01: domain relation separation

The existing generic checks already distinguish structural equality,
ADL-based identity via `same`, named unary/binary relations, negated
relations and numerical tolerance. D01 does not warrant a domain-specific
library dependency or additional macros. The new standalone regression
`test_relations.cpp` verifies structural equality versus identity in both
directions, mathematical `equal` with indeterminate evidence, directional
containment, intersection and disjointness, refinement, consistency and
tolerance versus exact equality. It checks domain relation naming, rendered
operands and CHECK-before-operand/single-evaluation behavior. No domain
relation is silently translated into a different one. The self-test is
registered without platform guards. CI verification remains the release gate.
