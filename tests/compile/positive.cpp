#include "ariadne-testing.hpp"
#include <concepts>
#include <type_traits>
ARIADNE_TEST_CONCEPT(std::integral<int>);
ARIADNE_TEST_CONCEPT((std::same_as<int, int>));
ARIADNE_TEST_TRAIT(std::is_trivially_copyable_v<int>);
ARIADNE_TEST_SAME_TYPE(int, int);
