# Ariadne Testing

A C++20 testing framework designed for rigorous numerical software. It provides
proof-oriented three-valued assertions, distinct test outcomes, source locations
and structured test results, with no external test framework.

The **only repository dependency** is `ariadne-cps/configuration`, used for
CMake configuration and packaging. Testing has no runtime dependency on
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
is not required. For installation, use a Release build, target
`ariadne-testing` and `cmake --install`.

## Example

```cpp
#include "testing/testing.hpp"
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

Include `testing/checks.hpp` to use optional macros
`ARIADNE_PROVE(ctx(), predicate)`, `ARIADNE_EXPECT_EQUAL(ctx(), a, b)`,
`ARIADNE_EXPECT_SAME(ctx(), a, b)`,
`ARIADNE_EXPECT_WITHIN(ctx(), a, b, tolerance)`,
`ARIADNE_EXPECT_THROWS(ctx(), Exception, operation())`,
`ARIADNE_EXPECT_EXCEPTION(ctx(), operation())` and
`ARIADNE_EXPECT_NO_THROW(ctx(), operation())`. Expression names are
recorded automatically; operand evaluation is deferred until after the
numbered CHECK progress event and happens once. Explicit Context calls
remain available whenever a handwritten description is preferable.

## Relational and predicate checks

In addition to equality and tolerance, `testing/checks.hpp` exposes
`ARIADNE_EXPECT_NOT_EQUAL`, `ARIADNE_EXPECT_LESS`,
`ARIADNE_EXPECT_LESS_EQUAL`, `ARIADNE_EXPECT_GREATER`,
`ARIADNE_EXPECT_GREATER_EQUAL`, and generic
`ARIADNE_EXPECT_UNARY_PREDICATE`/`ARIADNE_EXPECT_BINARY_PREDICATE`
with corresponding `NOT_` forms. Each expression is evaluated once after
the numbered CHECK notification. Generic predicate negation preserves an
indeterminate verdict rather than coercing it to a boolean. Domain-specific
relations, such as `refines`, can be supplied by their owner via ADL.
