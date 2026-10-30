# Ariadne Testing

A C++20 framework for testing rigorous numerical software, without third-party
test frameworks. It offers three-valued mathematical assertions, separate operational
test outcomes, source locations, and independent console and logging reporters.

See [the design journal](doc/testing_design.md) for semantics and future work.

## Build and test

```bash
git clone https://github.com/ariadne-cps/testing.git
cd testing
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel --target tests
ctest --test-dir build-debug --output-on-failure
```

CMake initializes only the pinned submodules it requires. Build target
`ariadne-testing` for release/install, following the Logging repository.

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
