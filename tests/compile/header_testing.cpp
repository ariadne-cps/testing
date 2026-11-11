#include <type_traits>

#include "testing/testing.hpp"
static_assert(std::is_constructible_v<Ariadne::Testing::Suite, std::string>);
static_assert(std::is_constructible_v<Ariadne::Testing::Context,
                                      Ariadne::Testing::CaseResult&>);
