#include "testing/reporters.hpp"
#include <type_traits>
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::ConsoleReporter>);
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::JsonReporter>);
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::JUnitReporter>);
