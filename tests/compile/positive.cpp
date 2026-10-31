#include "ariadne-testing.hpp"
#include <concepts>
#include <type_traits>
ARIADNE_TESTING_COMPILE_CONCEPT(std::integral<int>);
ARIADNE_TESTING_COMPILE_CONCEPT((std::same_as<int, int>));
ARIADNE_TESTING_COMPILE_TRAIT(std::is_trivially_copyable_v<int>);
ARIADNE_TESTING_COMPILE_SAME_TYPE(int, int);
