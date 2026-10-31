# Ariadne Testing

[![macOS Status](https://github.com/ariadne-cps/testing/actions/workflows/macos.yml/badge.svg)](https://github.com/ariadne-cps/testing/actions/workflows/macos.yml)
[![Ubuntu Status](https://github.com/ariadne-cps/testing/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/ariadne-cps/testing/actions/workflows/ubuntu.yml)
[![Windows Status](https://github.com/ariadne-cps/testing/actions/workflows/win.yml/badge.svg)](https://github.com/ariadne-cps/testing/actions/workflows/win.yml)
[![Sanitizers Status](https://github.com/ariadne-cps/testing/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/ariadne-cps/testing/actions/workflows/sanitizers.yml)
[![Coverage Status](https://github.com/ariadne-cps/testing/actions/workflows/coverage.yml/badge.svg)](https://github.com/ariadne-cps/testing/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/ariadne-cps/testing/branch/main/graph/badge.svg)](https://codecov.io/gh/ariadne-cps/testing)

A C++20 testing framework designed for rigorous numerical software. It provides
proof-oriented three-valued assertions, distinct test outcomes, source locations
and structured test results, with no external test framework.

The **only repository dependency** is `ariadne-cps/configuration`, used for
CMake configuration and test builds. Testing has no runtime dependency on
Logging, Utility or Macro, nor on numerical domain libraries.

`ConsoleReporter` writes to a caller-provided `std::ostream`; the abstract
`Reporter` interface permits custom presentation without pulling other
modules into Testing. See [the design journal](doc/testing_design.md).

## Build and test

```bash
git clone https://github.com/ariadne-cps/testing.git
cd testing
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DWARNINGS_AS_ERRORS=ON
cmake --build build-debug --parallel --target tests
ctest --test-dir build-debug --output-on-failure
```

CMake initializes only the pinned Configuration submodule; recursive checkout
is not required. This framework is a **test-only dependency** and is not
installed: consumers compile/link it from source for their test targets.

## Compile-time and standalone CTest checks

The public `ARIADNE_TEST_CONCEPT(expr)`, `ARIADNE_TEST_TRAIT(expr)`
and `ARIADNE_TEST_SAME_TYPE(A, B)` macros expand directly to
`static_assert`; compiler diagnostics remain authoritative.
Parenthesize expressions containing commas or use named type aliases.

Include `cmake/TestingOrchestration.cmake` to register
`ariadne_add_compile_check(NAME ... SOURCE ... LINK_LIBRARIES ...)`.
An optional `EXPECT_FAILURE EXPECTED_DIAGNOSTIC "marker"` checks that
compilation fails for the intended reason, not an unrelated error.
Compile checks use OBJECT targets excluded from normal builds; CTest
builds them serially in the current configuration. The independent
`ariadne_add_cmake_script_test(NAME ... SCRIPT ...)` runs CMake scripts.
`ariadne_add_python_unittest_test(NAME ... DIRECTORY ...)` discovers
unittest cases only when an interpreter exists, without requiring Python.
CTest labels distinguish compile, cmake and python checks.

## Example

```cpp
#include "ariadne-testing.hpp"
#include <iostream>

int main() {
    Ariadne::Testing::Suite suite("arithmetic");
    suite.add("integer identity", [](Ariadne::Testing::Context& t) {
        t.expect_equal(1 + 1, 2, "one plus one");
    });
    Ariadne::Testing::ConsoleReporter reporter(std::cout);
    return suite.run(reporter).successful() ? 0 : 1;
}
```

## Self-describing assertions

Include `ariadne-testing.hpp` to access all public Testing APIs, including
fixture, checks and parameterized helpers. Within `ContextFixture` methods,
use `ARIADNE_PROVE(predicate)`, `ARIADNE_EXPECT_EQUAL(a, b)`,
`ARIADNE_EXPECT_SAME(a, b)`, `ARIADNE_EXPECT_WITHIN(a, b, tolerance)`,
`ARIADNE_EXPECT_THROWS(Exception, operation())`,
`ARIADNE_EXPECT_EXCEPTION(operation())` and
`ARIADNE_EXPECT_NO_THROW(operation())`. In free functions or Context&
lambdas use their `_CTX(context, ...)` equivalents. Expression names are
recorded automatically; operand evaluation is deferred until after the
numbered CHECK progress event and happens once. Explicit Context calls
remain available whenever a handwritten description is preferable.

## Domain relations remain separate

Use `ARIADNE_EXPECT_EQUAL(_CTX)` for ordinary equality,
`ARIADNE_EXPECT_SAME(_CTX)` for ADL-dispatched identity, and named
`ARIADNE_EXPECT_BINARY_PREDICATE(_CTX)` relations for mathematical
`equal`, `intersect`, `disjoint`, `contains` and `refines`.
The domain supplies the functions; Testing provides only generic
evaluation and evidence preservation. Tolerance via
`ARIADNE_EXPECT_WITHIN(_CTX)` is not exact equality, and an uncertain
mathematical relation remains Inconclusive. `test_relations.cpp` checks
these distinctions with a dependency-free stand-in domain.

## Relational and predicate checks

In addition to equality and tolerance, `ariadne-testing.hpp` exposes
`ARIADNE_EXPECT_NOT_EQUAL`, `ARIADNE_EXPECT_LESS`,
`ARIADNE_EXPECT_LESS_EQUAL`, `ARIADNE_EXPECT_GREATER`,
`ARIADNE_EXPECT_GREATER_EQUAL`, and generic
`ARIADNE_EXPECT_UNARY_PREDICATE`/`ARIADNE_EXPECT_BINARY_PREDICATE`
with corresponding `NOT_` forms. Each expression is evaluated once after
the numbered CHECK notification. Generic predicate negation preserves an
indeterminate verdict rather than coercing it to a boolean. Domain-specific
relations, such as `refines`, can be supplied by their owner via ADL.

## Explicit evidence expectations

Inside a fixture, use `ARIADNE_EXPECT_DEFINITELY(proposition)`,
`ARIADNE_EXPECT_POSSIBLY(proposition)`,
`ARIADNE_EXPECT_NOT_POSSIBLY(proposition)`, or
`ARIADNE_EXPECT_INDETERMINATE(proposition)`; outside a fixture use `_CTX`.
An indeterminate proposition *expected* to be indeterminate passes; an
indeterminate result under a requested definite proof remains inconclusive.
The possibly check only establishes non-refutation and must not be confused
with a definite mathematical proof. The report retains observed evidence.

## Exit-gate policy

`Summary::successful()` is strict; explicit `summary.acceptable(AcceptancePolicy{InconclusivePolicy::NonBlocking, SkippedPolicy::NonBlocking})` allows a caller to tolerate inconclusives only when choosing an exit status. It does not change stored outcomes, reporter labels or counters, does not allow failures/errors, and still requires at least one genuinely passing case. Solver UNKNOWN is an ordinary domain value, not automatically a testing Inconclusive.

## Domain status expectations

`Context::expect_status(operation, selector, {allowed...}, verifier, description)`
checks permitted typed domain statuses and evaluates a conditional result
postcondition. An expected UNKNOWN with the required reason is Passed;
uncertain postcondition evidence is Inconclusive. A shorter overload checks
only status. An unpermitted status fails without running the verifier.

## Alternative result/exception outcomes

`Context::expect_alternative(operation, verify_result, verify_exception, description)`
tests one of two completion paths: a normally returned result or an exception.
Each caller-provided validator decides whether its observed alternative is
allowed and checks any applicable witness, partial result or budget policy.
For exceptions, the callback receives `std::exception const&` so consumers
can test derived types and payloads with `dynamic_cast`. A void operation
passes `std::monostate const&` to its result validator. Validation returns
boolean or three-valued evidence; false is Failed, indeterminate is
Inconclusive, and validator exceptions produce Error. Nonstandard operation
exceptions are rejected. CHECK is announced before the operation and only
the selected validator executes.

### Domain, alternative and concurrency macros

The user-facing checks also have optional, documented macro wrappers.
Use the `_CTX` forms in free functions or `Suite::add` lambdas and the
short forms inside a `ContextFixture`:

```cpp
ARIADNE_EXPECT_STATUS_CTX(c, solve(), [](auto const& r) { return r.status; },
    (std::initializer_list<Status>{Status::SAT, Status::UNKNOWN}),
    [](auto const& r) { return valid_witness_or_reason(r); });

ARIADNE_EXPECT_STATUS_ONLY_CTX(c, solve(), [](auto const& r) { return r.status; },
    (std::initializer_list<Status>{Status::UNSAT}));

ARIADNE_EXPECT_ALTERNATIVE_CTX(c, compute(),
    [](auto const& result) { return verify_result(result); },
    [](std::exception const& error) { return permitted_exception(error); });

ARIADNE_EXPECT_ATOMIC_CTX(c, progress,
    [](int observed) { return observed >= 10; }, std::chrono::seconds(5));
ARIADNE_EXPECT_READY_CTX(c, future, std::chrono::seconds(5));
ARIADNE_EXPECT_EVENTUALLY_CTX(c, cv, lock,
    [&] { return finished; }, std::chrono::seconds(5));
```

The corresponding fixture macros omit `_CTX` and the context argument:
`ARIADNE_EXPECT_STATUS`, `ARIADNE_EXPECT_STATUS_ONLY`,
`ARIADNE_EXPECT_ALTERNATIVE`, `ARIADNE_EXPECT_ATOMIC`,
`ARIADNE_EXPECT_READY`, and `ARIADNE_EXPECT_EVENTUALLY`.
Wrap initializer-list expressions containing commas in parentheses, as
shown above; callbacks are invoked only when their selected check requires
them, and the operation expression is evaluated once after CHECK.
The pre-existing status-set, future/atomic object, lock, and timeout are
regular C++ arguments and are not deferred. These macros remain thin
wrappers for the corresponding `Context` methods; the C++ APIs are
equally supported and preferred when explicit descriptions are needed.

## Fixture lifecycle and shared resources

Every `ARIADNE_ADD_TEST_TO` and `ARIADNE_ADD_VARIANT_TEST` case constructs
a new fixture with its own `Context&`. Optional `void set_up()` and
`void tear_down()` hooks run around the test body. Teardown runs after
normal completion, an exception or a failed fatal requirement; setup
failure prevents the test body and teardown, but the destructor still runs.
Teardown failures are Errors; when both body and teardown fail, both
error descriptions are retained. `FixtureCheck::NoThrow` applies only
to the body, not the hooks. Hook return values must be void.

**Never share a fixture across cases:** its Context reference is case-local.
To preserve solver warm-start or similar reusable data, capture an
explicitly owned `std::shared_ptr` resource in the `fixture_variant`
factory and construct new fixtures that refer to the resource. The
consumer owns resource synchronization, reset and state restoration.
The lifecycle is a C++ extension of existing fixture registration, not
a new assertion: existing registration macros already activate the hooks.

## Parameterized suites and fixture factories

The umbrella header also provides implementation/precision variants.
For each case, `fixture_variant<Fixture>(suite, "implementation", factory)`
constructs a fresh fixture using the supplied `Context&`, with an
implementation owned or cloned by the factory. Register a subset of test
methods per implementation with `ARIADNE_ADD_VARIANT_TEST(group, test_method)`.
Variants are part of case names, not a third numeric progress counter.
`suite_variant(suite, "Validated").add_typed<ValidatedTag>("api", callback)`
supports compile-time paradigm variants. Factory errors are reported as
test Error, while normal fixture methods remain standard C++.

## Bounded atomic progress

`Context::expect_atomic(state, predicate, timeout, description)` waits
for a predicate on a `std::atomic<T>` loaded with acquire ordering. The
operation publishes CHECK before observing the state, reports the last
observed value and elapsed/limit durations, and classifies timeout as
Failed or negative timeout as Error. Use release stores in the producer;
only the case runner may access `Context` and `Reporter`. Workers must
not issue checks directly because worker-result synchronization remains
a separate D05 task. Atomic waits use a bounded poll instead of the
non-timed C++20 `atomic::wait`. This facility does not stop hung workers;
the repository's CTest self-tests have a separate 90-second watchdog.

## Structured wait observations

All bounded waits attach `AssertionRecord::wait` when applicable. The
optional `WaitObservation` contains a typed `WaitStatus`, elapsed waiting
time, available time budget and whether the deadline was absolute.
A timeout reports unmet progress within a time budget, not proof of deadlock
or scheduler slowdown. Ordinary assertions have no wait metadata;
`Context::elapsed` and `measure` remain separate nonasserting diagnostics.
Existing public wait macros automatically populate these observations.

## Shared monotonic deadlines

For multi-stage asynchronous tests, create one absolute deadline and reuse
it rather than restarting the full duration for every check:

```cpp
auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
ARIADNE_EXPECT_READY_UNTIL_CTX(c, future, deadline);
ARIADNE_EXPECT_EVENTUALLY_UNTIL_CTX(c, cv, lock, [&] { return finished; }, deadline);
ARIADNE_EXPECT_ATOMIC_UNTIL_CTX(c, progress,
    [](int value) { return value >= 10; }, deadline);
```

The fixture forms omit `_CTX` and the context argument. Equivalent explicit
methods are `expect_ready_until`, `expect_eventually_until`, and
`expect_atomic_until`. An expired deadline still permits a single check of
an already-satisfied condition; otherwise it produces a timeout. Recorded
diagnostics include elapsed waiting, remaining budget at each check start,
and observed overrun. A deadline failure establishes only that required
progress was not observed in time; it cannot distinguish a bug from
scheduler slowdown, and it is not a benchmark measurement. Worker ownership
and CTest's separate process watchdog remain unchanged.

## Cross-thread checks and deterministic reporting

`WorkerBatch batch(n)` allocates fixed numbered worker slots. Run
`batch.run(i, [](WorkerBatch::Worker& worker) { ... })` on a caller-owned
worker thread and use `ARIADNE_WORKER_CHECK(worker, expression)` to buffer
one evidence-preserving check without touching `Context` or `Reporter`.
After joining **all** threads, invoke `batch.flush(context)` on the owning
case thread. Assertions are emitted in slot order, then check order.
Source locations and Failed/Inconclusive/Error classifications are retained.

Uncaught worker exceptions and empty worker slots are Error; duplicate or
incomplete batches cannot silently pass. Worker references must not escape
the run callback. These checks only emit CHECK events at flush time, so
worker hangs remain subject to the CTest watchdog; this API does not create,
cancel, join or force-stop threads. See `testing/worker.hpp` for details.

## Structured diagnostic scopes

Use `auto file = ctx().scope("oracle", "floatmp_rounding.tsv");` and
`auto row = ctx().scope("row", 21);` for nested case/row labels. The console
prints and flushes SCOPE events before the next operation, and failures
include their full active context. Scope destruction restores the parent.
`ctx().note(text)`, `ctx().warning(text)`, `ctx().inspect("value", x)`,
`ctx().elapsed("phase", duration)`, and
`ctx().measure("phase", [&] { operation(); });` create diagnostic events
that do not increment assertion counters or count as successful checks.
`ConsoleReporter(output, ConsoleDetail::Concise)` hides successful
notes and measurements, but preserves warnings/scopes and displays
additional details if the case fails.

## Fatal prerequisites and exception messages

`ARIADNE_REQUIRE(condition)` records a prerequisite and stops the current
fixture test case if it is false or indeterminate; the equivalent for a
free function or Context& lambda is `ARIADNE_REQUIRE_CTX(context, condition)`.
The recorded outcome is `Failed` for false evidence and `Inconclusive`
for indeterminate evidence, not `Error`. Exceptions thrown while evaluating
prerequisites are errors. Other cases continue normally. Do not intercept
this internal control-flow signal in a test's user-defined catch-all handler.

`ARIADNE_EXPECT_THROWS_MESSAGE(std::runtime_error, action(), "keyword")`
checks both a `std::exception`-derived type and a substring of `what()`.
A `_CTX(context, ...)` form is available outside fixtures. The action is
executed only after the numbered CHECK notification.

## Exception payload verification

Use `ARIADNE_EXPECT_THROWS_PAYLOAD(Exception, statement, projector, expected)`
inside a fixture, or the `_CTX(context, ...)` form elsewhere. `projector`
is a callable applied to the caught typed exception; the result is checked
against `expected`. For example, a lambda can extract `error.code`.
Both operands are evaluated after the numbered CHECK, retain their rendered
values, and preserve three-valued equality evidence. A projector that throws
is reported as a test Error, not confused with the operation's exception.

## Range comparisons

Use `ARIADNE_EXPECT_RANGES(actual, expected)` within a fixture, or
`ARIADNE_EXPECT_RANGES_CTX(context, actual, expected)` outside one.
`ARIADNE_EXPECT_RANGES_BY(actual, expected, comparator)` accepts a
custom evidence-preserving comparison; `ARIADNE_EXPECT_RANGES_AT(actual,
expected, comparator, path)` additionally maps a zero-based index to a
semantic path such as `node/3`. All have explicit-context `_CTX` variants.
They accept C++20 input ranges, count both sequences, and report the first
difference with short value excerpts. Pairwise indeterminate evidence yields
an Inconclusive check unless another pair definitely differs or counts differ.
Parenthesize macro arguments containing commas, such as braced sequences.

## Artifact files and golden references

`Context::work_directory()` creates a temporary directory isolated to the running case. Use `Context::artifact(path, label, media_type, kind, lifetime)` to record artifacts and request retention; the default lifetime is `External`, which never changes an existing file. For managed outputs choose `Temporary`, `KeepOnFailure`, or `KeepAlways`; managed paths must be within the case directory.

`expect_file_exists(path, description)`, `expect_file_equal(actual, reference, description)`, and `expect_file_signature(path, signature, description)` perform filesystem and binary checks, reporting missing files or the first differing byte offset. Golden files are read only, never overwritten; PNG interpretation belongs to consumers such as Rendering.

## Restoring mutable test state

Use `auto guard = Ariadne::Testing::preserve_value(setting);` for copyable state with non-throwing assignment. For global or domain-specific state, use `preserve_state(getter, restorer)`: the getter snapshots the original value once, and the `noexcept` restorer runs at scope exit, including exception unwinding or a failed fatal prerequisite. Guards are non-copyable and non-movable, and nested guards restore in reverse order. No global state is modified automatically.
