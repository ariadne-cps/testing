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
