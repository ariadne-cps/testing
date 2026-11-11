#include <type_traits>

#include "testing/results.hpp"
static_assert(std::is_aggregate_v<Ariadne::Testing::CaseResult>);
static_assert(Ariadne::Testing::Outcome::Passed != Ariadne::Testing::Outcome::Error);
