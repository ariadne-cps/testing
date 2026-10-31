#include "testing/testing.hpp"
#include <type_traits>
static_assert(std::is_constructible_v<Ariadne::Testing::Suite, std::string>);
static_assert(std::is_constructible_v<Ariadne::Testing::Context,
                                      Ariadne::Testing::CaseResult&>);
