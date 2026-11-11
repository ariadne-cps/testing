#include <concepts>
#include <type_traits>

#include "testing/macros.hpp"
ARIADNE_TESTING_COMPILE_CONCEPT(std::integral<int>);
ARIADNE_TESTING_COMPILE_TRAIT(std::is_trivially_copyable_v<int>);
ARIADNE_TESTING_COMPILE_SAME_TYPE(int, int);
#ifndef ARIADNE_TESTING_EXPECT_EQUAL_CTX
#error "macros.hpp must export runtime assertion macros"
#endif
#ifndef ARIADNE_TESTING_ADD_VARIANT_TEST
#error "macros.hpp must export variant macros"
#endif
