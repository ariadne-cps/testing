/***************************************************************************
 *            test_parameterized.cpp
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
        ARIADNE_TESTING_EXPECT_EQUAL(implementation->value(), 42);
    }

    void test_other() {
        ++*executions;
        ARIADNE_TESTING_EXPECT_GREATER(implementation->value(), 41);
    }
    void test_demonstration() { ++*executions; }
};

struct PrecisionFixture : ContextFixture {
    int bits;
    PrecisionFixture(Context& context, int value) : ContextFixture(context), bits(value) {}
    void test_precision() { ARIADNE_TESTING_EXPECT_GREATER(bits, 32); }
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
    auto full_factory = [&](Context& context) {
        ++first_factories;
        return InterfaceFixture(context, std::make_unique<FirstImplementation>(), executions);
    };
    auto full = fixture_variant<InterfaceFixture>(suite, "TaylorPicard", full_factory);
    auto full_value = [](InterfaceFixture& f) { f.test_value(); };
    auto full_other = [](InterfaceFixture& f) { f.test_other(); };
    auto full_demo = [](InterfaceFixture& f) { f.test_demonstration(); };
    full.add("test_value", full_value);
    full.add("test_other", full_other);
    full.add("test_demonstration", full_demo, FixtureCheck::NoThrow);

    auto subset_factory = [&](Context& context) {
        ++second_factories;
        return InterfaceFixture(context, std::make_unique<SecondImplementation>(), executions);
    };
    auto subset = fixture_variant<InterfaceFixture>(suite, "Affine", subset_factory);
    auto subset_value = [](InterfaceFixture& f) { f.test_value(); };
    subset.add("test_value", subset_value);
    if (first_factories != 0 || second_factories != 0 || executions != 0) return 1;

    // A move-only factory is supported, because Suite stores a copyable
    // wrapper around its owned factory rather than copying the factory itself.
    auto state = std::make_shared<int>(64);
    auto precision_factory = [state](Context& context) {
        return PrecisionFixture(context, *state);
    };
    auto precision = fixture_variant<PrecisionFixture>(suite, "FloatMP/64", precision_factory);
    auto precision_check = [](PrecisionFixture& f) { f.test_precision(); };
    precision.add("test_precision", precision_check);

    auto nonmovable = fixture_variant<NotMovableFixture>(suite, "NonMovable");
    auto nonmovable_exists = [](NotMovableFixture& f) { f.test_exists(); };
    nonmovable.add("test_exists", nonmovable_exists);

    // A typed generic test is instantiated separately per compile-time tag.
    auto paradigm_check = []<class Tag>(Context& context, std::type_identity<Tag>) {
        ARIADNE_TESTING_EXPECT_GREATER_CTX(context, Tag::value, 0);
    };
    suite_variant(suite, "Approximate").add_typed<ApproximateTag>("api", paradigm_check);
    suite_variant(suite, "Validated").add_typed<ValidatedTag>("api", paradigm_check);
    suite_variant(suite, "Effective").add_typed<EffectiveTag>("api", paradigm_check);

    // A failing factory must produce an Error and must not suppress later cases.
    // The factory fails on its first invocation only, so the same variant
    // registration group later runs the method body successfully.
    auto broken_factory = [](Context& c) -> PrecisionFixture {
        static int calls = 0;
        if (calls++ == 0) throw std::runtime_error("factory failure");
        return PrecisionFixture(c, 96);
    };
    auto broken = fixture_variant<PrecisionFixture>(suite, "Broken", broken_factory);
    auto precision_method = [](PrecisionFixture& fixture) { fixture.test_precision(); };
    broken.add("test_precision", precision_method);
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
    auto live_factory = [&](Context& context) {
        if (streamed.str().find("[RUN 1/1] live / FloatMP/192 / precision") ==
            std::string::npos) throw std::runtime_error("RUN was not flushed");
        return PrecisionFixture(context, 192);
    };
    auto live_variant = fixture_variant<PrecisionFixture>(live, "FloatMP/192", live_factory);
    live_variant.add("test_precision", precision_check);
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

    // Reuse the identical callable specialization for both accepted and
    // rejected names. The invalid call must not register another case.
    Suite names("variant name validation");
    auto named_case = [](Context& c) {
        c.prove(true, "valid variant callback");
    };
    auto group = suite_variant(names, "Named");
    group.add("accepted", named_case);
    bool rejected_empty_name = false;
    try {
        group.add("", named_case);
    } catch (std::invalid_argument const&) {
        rejected_empty_name = true;
    }
    CaptureReporter names_capture;
    auto const names_summary = names.run(names_capture);
    if (!rejected_empty_name || names_summary.passed != 1 ||
        names_summary.failed != 0 || names_summary.errors != 0 ||
        names_capture.results.size() != 1 ||
        names_capture.results[0].name != "Named / accepted" ||
        names_capture.results[0].assertions.size() != 1)
        return 13;

    // Two uses of the SAME method callable type must cover prefix removal
    // and preservation, and both Direct and NoThrow fixture policies.
    Suite method_policies("variant method policies");
    auto policies = fixture_variant<PrecisionFixture>(
        method_policies, "Precision", [](Context& c) {
            return PrecisionFixture(c, 96);
        });
    auto check_precision = [](PrecisionFixture& fixture) {
        fixture.test_precision();
    };
    policies.add("test_prefixed", check_precision, FixtureCheck::Direct);
    policies.add("manual_name", check_precision, FixtureCheck::NoThrow);
    CaptureReporter policies_capture;
    auto const policies_summary = method_policies.run(policies_capture);
    if (policies_summary.passed != 2 || policies_summary.failed != 0 ||
        policies_summary.errors != 0 || policies_capture.results.size() != 2 ||
        policies_capture.results[0].name != "Precision / prefixed" ||
        policies_capture.results[1].name != "Precision / manual_name" ||
        policies_capture.results[0].assertions.size() != 1 ||
        policies_capture.results[1].assertions.size() != 2 ||
        policies_capture.results[1].assertions[0].outcome != Outcome::Passed ||
        policies_capture.results[1].assertions[1].description != "operation completes" ||
        policies_capture.results[1].assertions[1].outcome != Outcome::Passed)
        return 14;

    // Re-run the same variant group with a succeeding factory invocation.
    Suite recovered("factory failure recovery");
    auto broken2 = fixture_variant<PrecisionFixture>(recovered, "Broken again", broken_factory);
    broken2.add("recovered precision", precision_method);
    CaptureReporter recovered_capture;
    auto const recovered_summary = recovered.run(recovered_capture);
    if (recovered_summary.passed != 1 || recovered_summary.errors != 0 ||
        recovered_capture.results.size() != 1 ||
        recovered_capture.results[0].name != "Broken again / recovered precision")
        return 15;

    // The same variant registration groups must also take the NoThrow branch.
    Suite variant_no_throw("variant NoThrow twins");
    auto nt_full = fixture_variant<InterfaceFixture>(variant_no_throw, "TaylorPicard NT", full_factory);
    nt_full.add("test_value", full_value, FixtureCheck::NoThrow);
    nt_full.add("test_other", full_other, FixtureCheck::NoThrow);
    auto nt_subset = fixture_variant<InterfaceFixture>(variant_no_throw, "Affine NT", subset_factory);
    nt_subset.add("test_value", subset_value, FixtureCheck::NoThrow);
    auto nt_precision = fixture_variant<PrecisionFixture>(variant_no_throw, "FloatMP/64 NT", precision_factory);
    nt_precision.add("test_precision", precision_check, FixtureCheck::NoThrow);
    auto nt_nonmovable = fixture_variant<NotMovableFixture>(variant_no_throw, "NonMovable NT");
    nt_nonmovable.add("test_exists", nonmovable_exists, FixtureCheck::NoThrow);
    CaptureReporter variant_nt_capture;
    auto const variant_nt_summary = variant_no_throw.run(variant_nt_capture);
    if (variant_nt_summary.passed != 5 || variant_nt_summary.errors != 0 ||
        variant_nt_capture.results.size() != 5) return 16;

    // A move-only factory remains supported via the copyable wrapper.
    {
        auto move_only_state = std::make_unique<int>(64);
        Suite move_only_suite("move-only factory");
        auto mo = fixture_variant<PrecisionFixture>(move_only_suite, "MoveOnly",
            [value = std::move(move_only_state)](Context& context) {
                return PrecisionFixture(context, *value);
            });
        mo.add("test_precision", precision_check);
        CaptureReporter mo_capture;
        if (!move_only_suite.run(mo_capture).successful() ||
            mo_capture.results.size() != 1) return 17;
    }

    return 0;
}
