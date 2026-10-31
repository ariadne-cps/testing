#include "testing/results.hpp"
#include <type_traits>
static_assert(std::is_aggregate_v<Ariadne::Testing::CaseResult>);
static_assert(Ariadne::Testing::Outcome::Passed != Ariadne::Testing::Outcome::Error);
