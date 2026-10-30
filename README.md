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
