/***************************************************************************
 *            compile.hpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/
/*
 *  This file is part of Ariadne Testing.
 *  Ariadne Testing is free software under the GNU General Public License,
 *  version 3 or (at your option) any later version.
 *  Ariadne Testing comes WITHOUT ANY WARRANTY.
 */
/** @file testing/compile.hpp
 *  @brief Compile-time checks backed directly by static_assert.
 *  @details The compiler's diagnostics remain authoritative. Parenthesize
 *  inline concept expressions with commas, or name the concept. For template
 *  types with commas, use aliases in the two-type macro.
 *  No ARIADNE_TEST_* aliases are provided: legacy Macro/testing.hpp
 *  already owns these names with different runtime semantics.
 */
#ifndef ARIADNE_TESTING_COMPILE_HPP
#define ARIADNE_TESTING_COMPILE_HPP
#include <type_traits>

/** @def ARIADNE_TESTING_COMPILE_CONCEPT
 *  @brief Require a C++20 concept expression at compile time.
 */
#define ARIADNE_TESTING_COMPILE_CONCEPT(condition) \
    static_assert((condition), "ARIADNE compile-time concept requirement failed: " #condition)

/** @def ARIADNE_TESTING_COMPILE_TRAIT
 *  @brief Require a compile-time boolean trait.
 */
#define ARIADNE_TESTING_COMPILE_TRAIT(condition) \
    static_assert((condition), "ARIADNE compile-time trait requirement failed: " #condition)

/** @def ARIADNE_TESTING_COMPILE_SAME_TYPE
 *  @brief Require exact type identity rather than convertibility.
 */
#define ARIADNE_TESTING_COMPILE_SAME_TYPE(first, second) \
    static_assert(std::is_same_v<first, second>, \
                  "ARIADNE compile-time type identity failed: " #first " vs " #second)

#endif // ARIADNE_TESTING_COMPILE_HPP
