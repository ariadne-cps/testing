/***************************************************************************
 *            test_relations.cpp
 *
 *  Copyright  2026  Luca Geretti
 *
 ****************************************************************************/

/*
 *  This file is part of Ariadne Testing.
 *
 *  Ariadne Testing is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Ariadne Testing is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ariadne Testing. If not, see <https://www.gnu.org/licenses/>.
 */

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ariadne-testing.hpp"

using namespace Ariadne::Testing;

namespace Demo {
struct Value {
    int identity;
    int center;
    int radius;
};

struct TernaryNumber {
    int value;
    bool known;
};
std::ostream& operator<<(std::ostream& output, const TernaryNumber& number) {
    return output << "ternary(" << number.value << ','
                  << (number.known ? "known" : "unknown") << ')';
}
Evidence ternary_relation(bool operands_known, bool truth) {
    return !operands_known ? Evidence::Indeterminate
         : truth           ? Evidence::True
                           : Evidence::False;
}
Evidence operator!=(const TernaryNumber& lhs, const TernaryNumber& rhs) {
    return ternary_relation(lhs.known && rhs.known, lhs.value != rhs.value);
}
Evidence operator<(const TernaryNumber& lhs, const TernaryNumber& rhs) {
    return ternary_relation(lhs.known && rhs.known, lhs.value < rhs.value);
}
Evidence operator<=(const TernaryNumber& lhs, const TernaryNumber& rhs) {
    return ternary_relation(lhs.known && rhs.known, lhs.value <= rhs.value);
}
Evidence operator>(const TernaryNumber& lhs, const TernaryNumber& rhs) {
    return ternary_relation(lhs.known && rhs.known, lhs.value > rhs.value);
}
Evidence operator>=(const TernaryNumber& lhs, const TernaryNumber& rhs) {
    return ternary_relation(lhs.known && rhs.known, lhs.value >= rhs.value);
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
    return os << "value(" << v.identity << ',' << v.center << ',' << v.radius << ')';
}
bool operator==(const Value& a, const Value& b) {
    return a.center == b.center && a.radius == b.radius;
}
bool same(const Value& a, const Value& b) {
    return a.identity == b.identity;
}
Evidence equal(const Value& a, const Value& b) {
    if (a.radius == 0 && b.radius == 0 && a.center == b.center)
        return Evidence::True;
    if (std::abs(a.center - b.center) > a.radius + b.radius)
        return Evidence::False;
    return Evidence::Indeterminate;
}
bool intersect(const Value& a, const Value& b) {
    return std::abs(a.center - b.center) <= a.radius + b.radius;
}
bool disjoint(const Value& a, const Value& b) { return !intersect(a, b); }
bool contains(const Value& a, const Value& b) {
    return std::abs(a.center - b.center) + b.radius <= a.radius;
}
Evidence refines(const Value& a, const Value& b) {
    if (contains(b, a))
        return Evidence::True;
    if (intersect(a, b))
        return Evidence::Indeterminate;
    return Evidence::False;
}
bool consistent(const Value& v) { return v.radius >= 0; }
} // namespace Demo

struct TransientReference {
    int* target;
    ~TransientReference() { *target = -99; }
    const int& raw() const { return *target; }
};

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    std::string last_started;
    void check_started(const CaseResult&, std::size_t, std::string_view name) override {
        last_started = name;
    }
    void case_finished(const CaseResult& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

int main() {
    using Demo::Value;
    Suite suite("independent domain relation semantics");
    Capture capture;

    suite.add("exact equality need not imply identity", [](Context& c) {
        Value a{1, 10, 0}, b{2, 10, 0};
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(c, a, b);
    });
    suite.add("identity does not imply exact equality", [](Context& c) {
        Value a{7, 10, 0}, b{7, 11, 0};
        ARIADNE_TESTING_EXPECT_SAME_CTX(c, a, b);
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(c, a, b);
    });
    suite.add("exact equality does not imply identity", [](Context& c) {
        Value a{1, 10, 0}, b{2, 10, 0};
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(c, a, b);
        ARIADNE_TESTING_EXPECT_SAME_CTX(c, a, b);
    });
    suite.add("mathematical equality can be uncertain", [](Context& c) {
        Value a{1, 10, 1}, b{2, 11, 1};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, equal, a, b);
    });
    suite.add("mathematical inequality can be proven", [](Context& c) {
        Value a{1, 10, 0}, b{2, 20, 0};
        ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(c, equal, a, b);
    });
    suite.add("intersection and disjointness", [](Context& c) {
        Value a{1, 10, 1}, near{2, 12, 1}, far{3, 20, 1};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, intersect, a, near);
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, disjoint, a, far);
    });
    suite.add("directional containment", [](Context& c) {
        Value outer{1, 10, 5}, inner{2, 11, 1};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, contains, outer, inner);
        ARIADNE_TESTING_EXPECT_NOT_BINARY_PREDICATE_CTX(c, contains, inner, outer);
    });
    suite.add("proven refinement", [](Context& c) {
        Value inner{1, 11, 1}, outer{2, 10, 5};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, refines, inner, outer);
    });
    suite.add("uncertain refinement", [](Context& c) {
        Value a{1, 10, 2}, b{2, 12, 2};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, refines, a, b);
    });
    suite.add("consistency and its negation", [](Context& c) {
        Value good{1, 10, 0}, bad{2, 10, -1};
        ARIADNE_TESTING_EXPECT_UNARY_PREDICATE_CTX(c, consistent, good);
        ARIADNE_TESTING_EXPECT_NOT_UNARY_PREDICATE_CTX(c, consistent, bad);
    });
    suite.add("numerical tolerance is separate", [](Context& c) {
        ARIADNE_TESTING_EXPECT_WITHIN_CTX(c, 11, 10, 1);
    });
    suite.add("exact numerical equality rejects discrepancy", [](Context& c) {
        ARIADNE_TESTING_EXPECT_EQUAL_CTX(c, 11, 10);
    });
    int left_calls = 0, right_calls = 0;
    suite.add("ADL relations evaluate once after CHECK", [&](Context& c) {
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, intersect,
                                                    ([&] {
                                                        if (capture.last_started.find("intersect(") == std::string::npos)
                                                            throw std::logic_error("CHECK event not published");
                                                        ++left_calls;
                                                        return Value{1, 10, 1};
                                                    })(),
                                                    ([&] {
                                                        if (left_calls != 1)
                                                            throw std::logic_error("operand order changed");
                                                        ++right_calls;
                                                        return Value{2, 11, 1};
                                                    })());
    });
    suite.add("disjoint refinement must be rejected", [](Context& c) {
        Value a{1, 10, 1}, b{2, 30, 1};
        ARIADNE_TESTING_EXPECT_BINARY_PREDICATE_CTX(c, refines, a, b);
    });

    const auto summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << message << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 8 && summary.failed == 4 &&
               summary.inconclusive == 2 && summary.errors == 0 &&
               summary.skipped == 0 && capture.cases.size() == 14,
           "relation summary mismatch: passed=" + std::to_string(summary.passed) +
               " failed=" + std::to_string(summary.failed) +
               " inconclusive=" + std::to_string(summary.inconclusive) +
               " errors=" + std::to_string(summary.errors));
    verify(left_calls == 1 && right_calls == 1, "operands not evaluated once");
    if (capture.cases.size() == 14) {
        const std::vector<Outcome> outcomes{
            Outcome::Passed, Outcome::Failed, Outcome::Failed,
            Outcome::Inconclusive, Outcome::Passed, Outcome::Passed,
            Outcome::Passed, Outcome::Passed, Outcome::Inconclusive,
            Outcome::Passed, Outcome::Passed, Outcome::Failed,
            Outcome::Passed, Outcome::Failed};
        for (std::size_t i = 0; i < outcomes.size(); ++i)
            verify(capture.cases[i].outcome == outcomes[i],
                   "unexpected relation outcome " + std::to_string(i));
        const auto& identity = capture.cases[1].assertions;
        verify(identity.size() == 2 &&
                   identity[0].outcome == Outcome::Passed &&
                   identity[1].outcome == Outcome::Failed,
               "identity must not be conflated with equality");
        const auto& equality = capture.cases[2].assertions;
        verify(equality.size() == 2 &&
                   equality[0].outcome == Outcome::Passed &&
                   equality[1].outcome == Outcome::Failed,
               "equality must not be conflated with identity");
        const auto& uncertain = capture.cases[3].assertions;
        verify(uncertain.size() == 1 &&
                   uncertain[0].outcome == Outcome::Inconclusive &&
                   uncertain[0].description.find("equal(") != std::string::npos,
               "mathematical equality lost evidence or relation name");
        const auto& tolerance = capture.cases[10].assertions;
        verify(tolerance.size() == 1 &&
                   tolerance[0].outcome == Outcome::Passed &&
                   tolerance[0].detail.find("tolerance=") != std::string::npos,
               "tolerance diagnostic changed");
        const auto& operands = capture.cases[12].assertions;
        verify(operands.size() == 1 &&
                   operands[0].outcome == Outcome::Passed &&
                   operands[0].actual.find("value(") != std::string::npos &&
                   operands[0].expected.find("value(") != std::string::npos,
               "rendered operands or relation check missing");
    }
    // The .raw() method returns a reference to a member-style value owned
    // by a temporary. Its destructor deliberately poisons that value.
    // Each relation must compare AND render before the temporary dies.
    Suite temporaries("relational temporary lifetime");
    Capture observed;
    int comparisons = 0;
    int lhs_evaluations = 0;
    int rhs_evaluations = 0;
    temporaries.add("greater equal from temporary", [&](Context& c) {
        int state = 7;
        ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(c,
                                                 TransientReference{&state}.raw(), 7);
        if (state == -99)
            ++comparisons;
    });
    temporaries.add("less equal from temporary", [&](Context& c) {
        int state = 4;
        ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(c,
                                              TransientReference{&state}.raw(), 4);
        if (state == -99)
            ++comparisons;
    });
    temporaries.add("less with deferred operand order", [&](Context& c) {
        int state = 3;
        ARIADNE_TESTING_EXPECT_LESS_CTX(c,
                                        ([&] { ++lhs_evaluations; return TransientReference{&state}; }()).raw(),
                                        ([&] { ++rhs_evaluations; return 6; }()));
        if (state == -99)
            ++comparisons;
    });
    temporaries.add("greater from temporary", [&](Context& c) {
        int state = 9;
        ARIADNE_TESTING_EXPECT_GREATER_CTX(c,
                                           TransientReference{&state}.raw(), 8);
        if (state == -99)
            ++comparisons;
    });
    temporaries.add("not equal from temporary", [&](Context& c) {
        int state = 9;
        ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(c,
                                             TransientReference{&state}.raw(), 8);
        if (state == -99)
            ++comparisons;
    });
    const auto temporary_summary = temporaries.run(observed);
    verify(temporary_summary.passed == 5 && temporary_summary.errors == 0 &&
               temporary_summary.failed == 0 && observed.cases.size() == 5 &&
               comparisons == 5 && lhs_evaluations == 1 && rhs_evaluations == 1,
           "relational macro temporary lifetime or operand evaluation changed");
    if (observed.cases.size() == 5) {
        const std::string expected_actuals[] = {"7", "4", "3", "9", "9"};
        for (std::size_t i = 0; i < 5; ++i)
            verify(observed.cases[i].assertions.size() == 1 &&
                       observed.cases[i].assertions[0].outcome == Outcome::Passed &&
                       observed.cases[i].assertions[0].actual == expected_actuals[i],
                   "relational temporary observation was poisoned at case " +
                       std::to_string(i));
    }

    // A single test matrix checks the five built-in relations through both
    // the deferred Context API and the snapshot macros. Each pair exercises
    // exactly the same mathematical ordering, including signed/unsigned
    // boundaries and explicit boolean arithmetic promotion.
    Suite arithmetic_matrix("built-in relational type combinations");
    std::vector<std::array<bool, 5>> expected_cases;
    auto add_type_pair = [&](std::string name, auto lhs, auto rhs,
                             std::array<bool, 5> expected) {
        expected_cases.push_back(expected);
        arithmetic_matrix.add(std::move(name), [=](Context& c) {
            c.check_compare<Comparison::NotEqual>([&] { return lhs; }, [&] { return rhs; }, "direct !=");
            c.check_compare<Comparison::Less>([&] { return lhs; }, [&] { return rhs; }, "direct <");
            c.check_compare<Comparison::LessEqual>([&] { return lhs; }, [&] { return rhs; }, "direct <=");
            c.check_compare<Comparison::Greater>([&] { return lhs; }, [&] { return rhs; }, "direct >");
            c.check_compare<Comparison::GreaterEqual>([&] { return lhs; }, [&] { return rhs; }, "direct >=");
            ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(c, lhs, rhs);
            ARIADNE_TESTING_EXPECT_LESS_CTX(c, lhs, rhs);
            ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(c, lhs, rhs);
            ARIADNE_TESTING_EXPECT_GREATER_CTX(c, lhs, rhs);
            ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(c, lhs, rhs);
        });
    };
    add_type_pair("bool/int", false, 1, {true, true, true, false, false});
    add_type_pair("int/bool", 2, true, {true, false, false, true, true});
    add_type_pair("bool/double equal", true, 1.0, {false, false, true, false, true});
    add_type_pair("double/bool", 0.5, true, {true, true, true, false, false});
    add_type_pair("bool/bool", false, true, {true, true, true, false, false});
    add_type_pair("signed/unsigned", -1, 1u, {true, true, true, false, false});
    add_type_pair("unsigned/signed", 1u, -1, {true, false, false, true, true});
    add_type_pair("int/int equal", 1, 1, {false, false, true, false, true});
    add_type_pair("double/double equal", 1.5, 1.5, {false, false, true, false, true});
    add_type_pair("wide signed/unsigned", std::numeric_limits<std::int64_t>::min(),
                  std::numeric_limits<std::uint64_t>::max(),
                  {true, true, true, false, false});
    Capture arithmetic_capture;
    const auto arithmetic_summary = arithmetic_matrix.run(arithmetic_capture);
    verify(arithmetic_summary.failed == expected_cases.size() &&
               arithmetic_summary.passed == 0 && arithmetic_summary.errors == 0 &&
               arithmetic_summary.inconclusive == 0 &&
               arithmetic_capture.cases.size() == expected_cases.size(),
           "mixed arithmetic relations changed the suite summary");
    if (arithmetic_capture.cases.size() == expected_cases.size()) {
        for (std::size_t i = 0; i < expected_cases.size(); ++i) {
            const auto& assertions = arithmetic_capture.cases[i].assertions;
            verify(assertions.size() == 10,
                   "missing mixed relational assertions at case " + std::to_string(i));
            if (assertions.size() != 10)
                continue;
            for (std::size_t j = 0; j < assertions.size(); ++j) {
                const auto expected = expected_cases[i][j % 5]
                                        ? Outcome::Passed
                                        : Outcome::Failed;
                verify(assertions[j].outcome == expected &&
                           !assertions[j].actual.empty() &&
                           !assertions[j].expected.empty(),
                       "mixed relation mismatch at case " + std::to_string(i) +
                           " check " + std::to_string(j));
            }
        }
    }

    // The deferred Context API and the snapshot macros must agree for
    // domain-defined three-valued comparisons, not only built-in integers.
    // Exercise uncertainty, equality and both strict ordering directions.
    Suite ternary_suite("shared domain comparison semantics");
    const std::array<std::array<Outcome, 5>, 4> ternary_expected{
        {{Outcome::Inconclusive, Outcome::Inconclusive, Outcome::Inconclusive,
          Outcome::Inconclusive, Outcome::Inconclusive},
         {Outcome::Failed, Outcome::Failed, Outcome::Passed,
          Outcome::Failed, Outcome::Passed},
         {Outcome::Passed, Outcome::Passed, Outcome::Passed,
          Outcome::Failed, Outcome::Failed},
         {Outcome::Passed, Outcome::Failed, Outcome::Failed,
          Outcome::Passed, Outcome::Passed}}
    };
    for (int mode = 0; mode < 4; ++mode) {
        ternary_suite.add("ternary relation " + std::to_string(mode),
                          [mode](Context& c) {
                              Demo::TernaryNumber lhs{mode == 3 ? 2 : 1, mode != 0};
                              Demo::TernaryNumber rhs{mode == 2 ? 2 : 1, true};
                              c.check_compare<Comparison::NotEqual>([&] { return lhs; },
                                                                    [&] { return rhs; }, "direct !=");
                              c.check_compare<Comparison::Less>([&] { return lhs; },
                                                                [&] { return rhs; }, "direct <");
                              c.check_compare<Comparison::LessEqual>([&] { return lhs; },
                                                                     [&] { return rhs; }, "direct <=");
                              c.check_compare<Comparison::Greater>([&] { return lhs; },
                                                                   [&] { return rhs; }, "direct >");
                              c.check_compare<Comparison::GreaterEqual>([&] { return lhs; },
                                                                        [&] { return rhs; }, "direct >=");
                              ARIADNE_TESTING_EXPECT_NOT_EQUAL_CTX(c, lhs, rhs);
                              ARIADNE_TESTING_EXPECT_LESS_CTX(c, lhs, rhs);
                              ARIADNE_TESTING_EXPECT_LESS_EQUAL_CTX(c, lhs, rhs);
                              ARIADNE_TESTING_EXPECT_GREATER_CTX(c, lhs, rhs);
                              ARIADNE_TESTING_EXPECT_GREATER_EQUAL_CTX(c, lhs, rhs);
                          });
    }
    Capture ternary_capture;
    const auto ternary_summary = ternary_suite.run(ternary_capture);
    verify(ternary_summary.failed == 3 && ternary_summary.inconclusive == 1 &&
               ternary_summary.passed == 0 && ternary_summary.errors == 0 &&
               ternary_capture.cases.size() == ternary_expected.size(),
           "domain comparison matrix summary changed");
    if (ternary_capture.cases.size() == ternary_expected.size()) {
        for (std::size_t i = 0; i < ternary_capture.cases.size(); ++i) {
            const auto& assertions = ternary_capture.cases[i].assertions;
            verify(assertions.size() == 10,
                   "missing domain comparisons at case " + std::to_string(i));
            if (assertions.size() != 10)
                continue;
            for (std::size_t j = 0; j < assertions.size(); ++j) {
                verify(assertions[j].outcome == ternary_expected[i][j % 5] &&
                           assertions[j].actual.find("ternary(") != std::string::npos &&
                           assertions[j].expected.find("ternary(") != std::string::npos,
                       "direct and macro domain relations differ at case " +
                           std::to_string(i) + " check " + std::to_string(j));
            }
        }
    }
    return ok ? 0 : 1;
}
