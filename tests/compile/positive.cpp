#include "ariadne-testing.hpp"
// Prevent accidental reintroduction of pre-namespace compatibility macros.
// Canonical ARIADNE_TESTING_* names are exercised by this translation unit.
#ifdef ARIADNE_PROVE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_PROVE_CTX"
#endif
#ifdef ARIADNE_REQUIRE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_REQUIRE_CTX"
#endif
#ifdef ARIADNE_EXPECT_EQUAL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EQUAL_CTX"
#endif
#ifdef ARIADNE_EXPECT_RANGES_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES_CTX"
#endif
#ifdef ARIADNE_EXPECT_RANGES_BY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES_BY_CTX"
#endif
#ifdef ARIADNE_EXPECT_RANGES_AT_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES_AT_CTX"
#endif
#ifdef ARIADNE_EXPECT_SAME_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_SAME_CTX"
#endif
#ifdef ARIADNE_EXPECT_WITHIN_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_WITHIN_CTX"
#endif
#ifdef ARIADNE_EXPECT_THROWS_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS_CTX"
#endif
#ifdef ARIADNE_EXPECT_THROWS_MESSAGE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS_MESSAGE_CTX"
#endif
#ifdef ARIADNE_EXPECT_THROWS_PAYLOAD_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS_PAYLOAD_CTX"
#endif
#ifdef ARIADNE_EXPECT_EXCEPTION_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EXCEPTION_CTX"
#endif
#ifdef ARIADNE_EXPECT_NO_THROW_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NO_THROW_CTX"
#endif
#ifdef ARIADNE_EXPECT_STATUS_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_STATUS_CTX"
#endif
#ifdef ARIADNE_EXPECT_STATUS_ONLY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_STATUS_ONLY_CTX"
#endif
#ifdef ARIADNE_EXPECT_ALTERNATIVE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ALTERNATIVE_CTX"
#endif
#ifdef ARIADNE_EXPECT_ATOMIC_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ATOMIC_CTX"
#endif
#ifdef ARIADNE_EXPECT_READY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_READY_CTX"
#endif
#ifdef ARIADNE_EXPECT_EVENTUALLY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EVENTUALLY_CTX"
#endif
#ifdef ARIADNE_EXPECT_READY_UNTIL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_READY_UNTIL_CTX"
#endif
#ifdef ARIADNE_EXPECT_EVENTUALLY_UNTIL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EVENTUALLY_UNTIL_CTX"
#endif
#ifdef ARIADNE_EXPECT_ATOMIC_UNTIL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ATOMIC_UNTIL_CTX"
#endif
#ifdef ARIADNE_EXPECT_NOT_EQUAL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_EQUAL_CTX"
#endif
#ifdef ARIADNE_EXPECT_LESS_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_LESS_CTX"
#endif
#ifdef ARIADNE_EXPECT_LESS_EQUAL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_LESS_EQUAL_CTX"
#endif
#ifdef ARIADNE_EXPECT_GREATER_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_GREATER_CTX"
#endif
#ifdef ARIADNE_EXPECT_GREATER_EQUAL_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_GREATER_EQUAL_CTX"
#endif
#ifdef ARIADNE_EXPECT_UNARY_PREDICATE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_UNARY_PREDICATE_CTX"
#endif
#ifdef ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_UNARY_PREDICATE_CTX"
#endif
#ifdef ARIADNE_EXPECT_BINARY_PREDICATE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_BINARY_PREDICATE_CTX"
#endif
#ifdef ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_BINARY_PREDICATE_CTX"
#endif
#ifdef ARIADNE_EXPECT_DEFINITELY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_DEFINITELY_CTX"
#endif
#ifdef ARIADNE_EXPECT_POSSIBLY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_POSSIBLY_CTX"
#endif
#ifdef ARIADNE_EXPECT_NOT_POSSIBLY_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_POSSIBLY_CTX"
#endif
#ifdef ARIADNE_EXPECT_INDETERMINATE_CTX
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_INDETERMINATE_CTX"
#endif
#ifdef ARIADNE_WORKER_CHECK
#error "Obsolete testing alias must not be exposed: ARIADNE_WORKER_CHECK"
#endif
#ifdef ARIADNE_PROVE
#error "Obsolete testing alias must not be exposed: ARIADNE_PROVE"
#endif
#ifdef ARIADNE_REQUIRE
#error "Obsolete testing alias must not be exposed: ARIADNE_REQUIRE"
#endif
#ifdef ARIADNE_EXPECT_EQUAL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EQUAL"
#endif
#ifdef ARIADNE_EXPECT_RANGES
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES"
#endif
#ifdef ARIADNE_EXPECT_RANGES_BY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES_BY"
#endif
#ifdef ARIADNE_EXPECT_RANGES_AT
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_RANGES_AT"
#endif
#ifdef ARIADNE_EXPECT_SAME
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_SAME"
#endif
#ifdef ARIADNE_EXPECT_WITHIN
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_WITHIN"
#endif
#ifdef ARIADNE_EXPECT_THROWS
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS"
#endif
#ifdef ARIADNE_EXPECT_THROWS_MESSAGE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS_MESSAGE"
#endif
#ifdef ARIADNE_EXPECT_THROWS_PAYLOAD
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_THROWS_PAYLOAD"
#endif
#ifdef ARIADNE_EXPECT_EXCEPTION
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EXCEPTION"
#endif
#ifdef ARIADNE_EXPECT_NO_THROW
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NO_THROW"
#endif
#ifdef ARIADNE_EXPECT_NOT_EQUAL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_EQUAL"
#endif
#ifdef ARIADNE_EXPECT_LESS
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_LESS"
#endif
#ifdef ARIADNE_EXPECT_LESS_EQUAL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_LESS_EQUAL"
#endif
#ifdef ARIADNE_EXPECT_GREATER
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_GREATER"
#endif
#ifdef ARIADNE_EXPECT_GREATER_EQUAL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_GREATER_EQUAL"
#endif
#ifdef ARIADNE_EXPECT_UNARY_PREDICATE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_UNARY_PREDICATE"
#endif
#ifdef ARIADNE_EXPECT_NOT_UNARY_PREDICATE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_UNARY_PREDICATE"
#endif
#ifdef ARIADNE_EXPECT_BINARY_PREDICATE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_BINARY_PREDICATE"
#endif
#ifdef ARIADNE_EXPECT_NOT_BINARY_PREDICATE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_BINARY_PREDICATE"
#endif
#ifdef ARIADNE_EXPECT_DEFINITELY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_DEFINITELY"
#endif
#ifdef ARIADNE_EXPECT_POSSIBLY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_POSSIBLY"
#endif
#ifdef ARIADNE_EXPECT_NOT_POSSIBLY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_NOT_POSSIBLY"
#endif
#ifdef ARIADNE_EXPECT_INDETERMINATE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_INDETERMINATE"
#endif
#ifdef ARIADNE_EXPECT_STATUS
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_STATUS"
#endif
#ifdef ARIADNE_EXPECT_STATUS_ONLY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_STATUS_ONLY"
#endif
#ifdef ARIADNE_EXPECT_ALTERNATIVE
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ALTERNATIVE"
#endif
#ifdef ARIADNE_EXPECT_ATOMIC
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ATOMIC"
#endif
#ifdef ARIADNE_EXPECT_READY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_READY"
#endif
#ifdef ARIADNE_EXPECT_EVENTUALLY
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EVENTUALLY"
#endif
#ifdef ARIADNE_EXPECT_READY_UNTIL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_READY_UNTIL"
#endif
#ifdef ARIADNE_EXPECT_EVENTUALLY_UNTIL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_EVENTUALLY_UNTIL"
#endif
#ifdef ARIADNE_EXPECT_ATOMIC_UNTIL
#error "Obsolete testing alias must not be exposed: ARIADNE_EXPECT_ATOMIC_UNTIL"
#endif
#ifdef ARIADNE_ADD_TEST_TO
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_TEST_TO"
#endif
#ifdef ARIADNE_ADD_TEST_NO_THROW_TO
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_TEST_NO_THROW_TO"
#endif
#ifdef ARIADNE_TEST_SUITE
#error "Obsolete testing alias must not be exposed: ARIADNE_TEST_SUITE"
#endif
#ifdef ARIADNE_ADD_TEST
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_TEST"
#endif
#ifdef ARIADNE_ADD_TEST_NO_THROW
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_TEST_NO_THROW"
#endif
#ifdef ARIADNE_RUN_TESTS
#error "Obsolete testing alias must not be exposed: ARIADNE_RUN_TESTS"
#endif
#ifdef ARIADNE_ADD_VARIANT_TEST
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_VARIANT_TEST"
#endif
#ifdef ARIADNE_ADD_VARIANT_TEST_NO_THROW
#error "Obsolete testing alias must not be exposed: ARIADNE_ADD_VARIANT_TEST_NO_THROW"
#endif

#include <concepts>
#include <type_traits>
ARIADNE_TESTING_COMPILE_CONCEPT(std::integral<int>);
ARIADNE_TESTING_COMPILE_CONCEPT((std::same_as<int, int>));
ARIADNE_TESTING_COMPILE_TRAIT(std::is_trivially_copyable_v<int>);
ARIADNE_TESTING_COMPILE_SAME_TYPE(int, int);
