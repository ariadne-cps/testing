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
