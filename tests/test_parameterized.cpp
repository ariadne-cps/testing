/***************************************************************************
 *            test_parameterized.cpp
 *
 *  Copyright  2026  Luca Geretti
 *  Author     Luca Geretti
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

#include "ariadne-testing.hpp"

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace Ariadne::Testing;

struct CaptureReporter final : Reporter {
    std::vector<CaseResult> results;
    Summary summary;
    void case_finished(CaseResult const& r) override { results.push_back(r); }
    void suite_finished(std::string_view, Summary const& s) override { summary = s; }
};

struct Interface {
    virtual ~Interface() = default;
    virtual int value() const = 0;
};

struct FirstImplementation final : Interface {
    int value() const override { return 42; }
};

struct SecondImplementation final : Interface {
    int value() const override { return 42; }
};

struct InterfaceFixture final : ContextFixture {
    std::unique_ptr<Interface> implementation;
    int* executions;

    InterfaceFixture(Context& context, std::unique_ptr<Interface> object, int& counter)
        : ContextFixture(context), implementation(std::move(object)), executions(&counter) {}

    void test_value() {
        ++*executions;
        ARIADNE_EXPECT_EQUAL(implementation->value(), 42);
    }

    void test_other() {
        ++*executions;
        ARIADNE_EXPECT_GREATER(implementation->value(), 41);
    }
    void test_demonstration() { ++*executions; }
};

struct PrecisionFixture : ContextFixture {
    int bits;
    PrecisionFixture(Context& context, int value) : ContextFixture(context), bits(value) {}
    void test_precision() { ARIADNE_EXPECT_GREATER(bits, 32); }
};

struct ApproximateTag { static constexpr int value = 1; };
struct ValidatedTag { static constexpr int value = 2; };
struct EffectiveTag { static constexpr int value = 3; };

struct NotMovableFixture : ContextFixture {
    using ContextFixture::ContextFixture;
    NotMovableFixture(NotMovableFixture const&) = delete;
    NotMovableFixture(NotMovableFixture&&) = delete;
    void test_exists() { ctx().prove(true, "non-movable construction"); }
};

int main() {
    Suite suite("parameterized");
    int first_factories = 0;
    int second_factories = 0;
    int executions = 0;
    auto full = fixture_variant<InterfaceFixture>(suite, "TaylorPicard", [&](Context& context) {
        ++first_factories;
        return InterfaceFixture(context, std::make_unique<FirstImplementation>(), executions);
    });
    ARIADNE_ADD_VARIANT_TEST(full, test_value);
    ARIADNE_ADD_VARIANT_TEST(full, test_other);
    ARIADNE_ADD_VARIANT_TEST_NO_THROW(full, test_demonstration);

    auto subset = fixture_variant<InterfaceFixture>(suite, "Affine", [&](Context& context) {
        ++second_factories;
        return InterfaceFixture(context, std::make_unique<SecondImplementation>(), executions);
    });
    ARIADNE_ADD_VARIANT_TEST(subset, test_value);
    if (first_factories != 0 || second_factories != 0 || executions != 0) return 1;

    // A move-only factory is supported, because Suite stores a copyable
    // wrapper around its owned factory rather than copying the factory itself.
    auto state = std::make_unique<int>(64);
    auto precision = fixture_variant<PrecisionFixture>(
        suite, "FloatMP/64", [value = std::move(state)](Context& context) {
            return PrecisionFixture(context, *value);
        });
    ARIADNE_ADD_VARIANT_TEST(precision, test_precision);

    auto nonmovable = fixture_variant<NotMovableFixture>(suite, "NonMovable");
    ARIADNE_ADD_VARIANT_TEST(nonmovable, test_exists);

    // A typed generic test is instantiated separately per compile-time tag.
    auto paradigm_check = []<class Tag>(Context& context, std::type_identity<Tag>) {
        ARIADNE_EXPECT_GREATER_CTX(context, Tag::value, 0);
    };
    suite_variant(suite, "Approximate").add_typed<ApproximateTag>("api", paradigm_check);
    suite_variant(suite, "Validated").add_typed<ValidatedTag>("api", paradigm_check);
    suite_variant(suite, "Effective").add_typed<EffectiveTag>("api", paradigm_check);

    // A failing factory must produce an Error and must not suppress later cases.
    auto broken = fixture_variant<PrecisionFixture>(suite, "Broken", [](Context&) -> PrecisionFixture {
        throw std::runtime_error("factory failure");
    });
    ARIADNE_ADD_VARIANT_TEST(broken, test_precision);
    suite_variant(suite, "AfterError").add("still runs", [](Context& context) {
        context.prove(true, "runner continues");
    });

    CaptureReporter capture;
    auto const sum = suite.run(capture);
    if (sum.passed != 10 || sum.errors != 1 || sum.failed != 0 ||
        sum.skipped != 0 || sum.inconclusive != 0) return 2;
    if (capture.results.size() != 11 ||
        first_factories != 3 || second_factories != 1 || executions != 4) return 3;
    if (capture.results[0].name != "TaylorPicard / value" ||
        capture.results[1].name != "TaylorPicard / other" ||
        capture.results[2].name != "TaylorPicard / demonstration" ||
        capture.results[3].name != "Affine / value" ||
        capture.results[4].name != "FloatMP/64 / precision" ||
        capture.results[6].name != "Approximate / api" ||
        capture.results[8].name != "Effective / api") return 4;
    if (capture.results[0].assertions[0].check_index != 1 ||
        capture.results[1].assertions[0].check_index != 1 ||
        capture.results[3].case_index != 4 ||
        capture.results[3].case_total != 11) return 5;
    if (capture.results[9].outcome != Outcome::Error ||
        capture.results[9].assertions.empty() ||
        capture.results[9].assertions[0].detail != "factory failure") return 6;
    if (capture.results[10].outcome != Outcome::Passed) return 7;
    if (capture.results[2].assertions.size() != 1 ||
        capture.results[2].assertions[0].description != "operation completes") return 8;

    // Reports of variant cases must be flushed before the factory starts.
    Suite live("live");
    std::ostringstream streamed;
    auto live_variant = fixture_variant<PrecisionFixture>(live, "FloatMP/192",
        [&](Context& context) {
            if (streamed.str().find("[RUN 1/1] live / FloatMP/192 / precision") ==
                std::string::npos) throw std::runtime_error("RUN was not flushed");
            return PrecisionFixture(context, 192);
        });
    ARIADNE_ADD_VARIANT_TEST(live_variant, test_precision);
    ConsoleReporter live_reporter(streamed);
    if (!live.run(live_reporter).successful()) return 9;
    if (streamed.str().find("[CHECK 1.1]") == std::string::npos) return 10;

    // Reject unqualified variant and method names, avoiding ambiguous logs.
    try {
        (void)suite_variant(suite, "");
        return 11;
    } catch (std::invalid_argument const&) {}
    try {
        suite_variant(suite, "Valid").add("", [](Context&) {});
        return 12;
    } catch (std::invalid_argument const&) {}

    return 0;
}
