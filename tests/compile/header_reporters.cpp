#include <type_traits>

#include "testing/reporters.hpp"
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::ConsoleReporter>);
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::JsonReporter>);
static_assert(std::is_base_of_v<Ariadne::Testing::Reporter, Ariadne::Testing::JUnitReporter>);
