#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "ariadne-testing.hpp"

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

using namespace Ariadne::Testing;

struct Capture final : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(const CaseResult& result) override { cases.push_back(result); }
    void suite_finished(std::string_view, const Summary&) override {}
};

struct ThreeValued {
    bool definite;
    bool possible;
};
bool definitely(ThreeValued value) { return value.definite; }
bool possibly(ThreeValued value) { return value.possible; }

struct PayloadError : std::runtime_error {
    int code;
    explicit PayloadError(int value) : std::runtime_error("payload"), code(value) {}
};

struct SimulatedFuture {
    std::future_status status;
    int calls = 0;
    std::future_status wait_for(std::chrono::milliseconds) {
        ++calls;
        return status;
    }
};

// A custom Future can violate the std::future_status contract. Both waiting
// APIs must report the unexpected value as Error without consuming the result.
struct UnexpectedFuture {
    int relative_calls = 0;
    int absolute_calls = 0;
    template<class Rep, class Period>
    std::future_status wait_for(std::chrono::duration<Rep, Period>) {
        ++relative_calls;
        return static_cast<std::future_status>(99);
    }
    std::future_status wait_until(std::chrono::steady_clock::time_point) {
        ++absolute_calls;
        return static_cast<std::future_status>(99);
    }
};

int main() {
    auto verify = [](bool value, const std::string& description) {
        if (!value)
            std::cerr << description << '\n';
        return value;
    };
    bool ok = true;

    // The same type-instantiation must preserve all evidence states, including
    // contradictory input, rather than silently deciding uncertainty.
    Suite evidence_suite("three-valued inputs");
    const std::vector<ThreeValued> inputs{
        { true,  true},
        {false, false},
        {false,  true},
        { true, false}
    };
    const std::vector<Outcome> evidence_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Inconclusive, Outcome::Error};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        evidence_suite.add("evidence " + std::to_string(i), [&, i](Context& c) {
            c.prove(inputs[i], "logical evidence");
        });
    }
    Capture evidence_report;
    const auto evidence_summary = evidence_suite.run(evidence_report);
    ok &= verify(evidence_summary.passed == 1 && evidence_summary.failed == 1 &&
                     evidence_summary.inconclusive == 1 && evidence_summary.errors == 1 &&
                     evidence_report.cases.size() == inputs.size(),
                 "evidence: wrong summary");
    if (evidence_report.cases.size() == inputs.size()) {
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            ok &= verify(evidence_report.cases[i].outcome == evidence_outcomes[i],
                         "evidence: unexpected outcome " + std::to_string(i));
        }
        ok &= verify(evidence_report.cases[3].assertions.size() == 1 &&
                         evidence_report.cases[3].assertions[0].detail.find(
                             "Inconsistent logical evidence") != std::string::npos,
                     "evidence: contradictory input must be rejected");
    }

    // One instantiation of the typed-message checker for success, mismatch,
    // absent exception, wrong types and a fatal prerequisite.
    Suite typed_suite("typed exception boundaries");
    const std::vector<Outcome> typed_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Failed,
        Outcome::Failed, Outcome::Failed, Outcome::Failed};
    for (int mode = 0; mode < 6; ++mode) {
        typed_suite.add("typed " + std::to_string(mode), [mode](Context& c) {
            std::function<void()> operation = [mode, &c] {
                switch (mode) {
                    case 0:
                        throw std::runtime_error("token present");
                    case 1:
                        throw std::runtime_error("different message");
                    case 2:
                        throw std::logic_error("wrong exception type");
                    case 3:
                        throw 42;
                    case 4:
                        return;
                    case 5:
                        c.require([] { return false; }, "fatal prerequisite");
                        return;
                    default:
                        throw std::logic_error("unexpected test mode");
                }
            };
            c.expect_throws_message<std::runtime_error>(operation, "token", "typed message");
        });
    }
    Capture typed_report;
    const auto typed_summary = typed_suite.run(typed_report);
    ok &= verify(typed_summary.passed == 1 && typed_summary.failed == 5 &&
                     typed_summary.errors == 0 && typed_report.cases.size() == 6,
                 "typed exception: wrong summary");
    if (typed_report.cases.size() == 6) {
        const std::vector<std::string> expected_details{
            "", "exception message mismatch", "unexpected exception type",
            "unexpected non-standard exception", "no exception thrown", ""};
        for (std::size_t i = 0; i < 6; ++i) {
            const auto& result = typed_report.cases[i];
            ok &= verify(result.outcome == typed_outcomes[i] &&
                             result.assertions.size() == 1,
                         "typed exception: wrong outcome " + std::to_string(i));
            if (result.assertions.size() == 1 && i != 5)
                ok &= verify(result.assertions[0].detail == expected_details[i],
                             "typed exception: wrong detail " + std::to_string(i));
            if (result.assertions.size() == 1 && i == 5)
                ok &= verify(result.assertions[0].description == "fatal prerequisite",
                             "typed exception: prerequisite must retain original identity");
        }
    }

    // Projector and expectation failures must be distinguished from a
    // payload mismatch and from a fatal prerequisite inside either step.
    Suite payload_suite("payload validation boundaries");
    const std::vector<Outcome> payload_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Error, Outcome::Error,
        Outcome::Error, Outcome::Error, Outcome::Failed, Outcome::Inconclusive};
    for (int mode = 0; mode < 8; ++mode) {
        payload_suite.add("payload " + std::to_string(mode), [mode](Context& c) {
            std::function<void()> action = [] { throw PayloadError(17); };
            auto projector_factory = [&]() {
                return std::function<int(const PayloadError&)>([&](const PayloadError& error) {
                    switch (mode) {
                        case 2:
                            throw std::runtime_error("projector failed");
                        case 3:
                            throw 42;
                        case 6:
                            c.require([] { return false; }, "projector prerequisite");
                            break;
                        default:
                            break;
                    }
                    return error.code;
                });
            };
            auto expected = [&]() -> int {
                switch (mode) {
                    case 1:
                        return 21;
                    case 4:
                        throw std::runtime_error("expected failed");
                    case 5:
                        throw 42;
                    case 7:
                        c.require([] { return ThreeValued{false, true}; },
                                  "expected prerequisite");
                        break;
                    default:
                        break;
                }
                return 17;
            };
            c.expect_throws_payload<PayloadError>(action, projector_factory,
                                                  expected, "payload check");
        });
    }
    Capture payload_report;
    const auto payload_summary = payload_suite.run(payload_report);
    ok &= verify(payload_summary.passed == 1 && payload_summary.failed == 2 &&
                     payload_summary.inconclusive == 1 && payload_summary.errors == 4 &&
                     payload_report.cases.size() == 8,
                 "payload: wrong summary");
    if (payload_report.cases.size() == 8) {
        for (std::size_t i = 0; i < 8; ++i) {
            const auto& result = payload_report.cases[i];
            ok &= verify(result.outcome == payload_outcomes[i] &&
                             result.assertions.size() == 1,
                         "payload: wrong outcome " + std::to_string(i));
        }
        ok &= verify(payload_report.cases[6].assertions[0].description ==
                             "projector prerequisite" &&
                         payload_report.cases[7].assertions[0].description ==
                             "expected prerequisite",
                     "payload: fatal prerequisite was swallowed");
    }

    // A simulated Future avoids real sleeps while exercising all specified
    // wait_for classifications using one template instantiation.
    Suite future_suite("future readiness boundaries");
    const std::vector<Outcome> future_outcomes{
        Outcome::Passed, Outcome::Failed, Outcome::Failed, Outcome::Error};
    for (int mode = 0; mode < 4; ++mode) {
        future_suite.add("future " + std::to_string(mode), [mode](Context& c) {
            SimulatedFuture future{
                mode == 0 ? std::future_status::ready : mode == 1 ? std::future_status::timeout
                                                                  : std::future_status::deferred};
            const auto timeout = mode == 3 ? std::chrono::milliseconds(-1) : std::chrono::milliseconds(0);
            c.expect_ready(future, timeout, "readiness");
            if (future.calls != (mode == 3 ? 0 : 1))
                throw std::runtime_error("invalid wait_for invocation count");
        });
    }
    Capture future_report;
    const auto future_summary = future_suite.run(future_report);
    ok &= verify(future_summary.passed == 1 && future_summary.failed == 2 &&
                     future_summary.errors == 1 && future_report.cases.size() == 4,
                 "future: wrong summary");
    if (future_report.cases.size() == 4) {
        for (std::size_t i = 0; i < 4; ++i)
            ok &= verify(future_report.cases[i].outcome == future_outcomes[i] &&
                             future_report.cases[i].assertions.size() == 1,
                         "future: wrong outcome " + std::to_string(i));
    }

    // Reproduce a stale directory and a stale regular file from an earlier
    // process. The allocator must skip both, preserve their contents, and
    // remove only the directory that it actually owns.
#if defined(_WIN32)
    const auto process_id = std::to_string(_getpid());
#else
    const auto process_id = std::to_string(getpid());
#endif
    const auto temp = std::filesystem::temp_directory_path();
    const auto occupied_directory = temp / ("ariadne-case-" + process_id + "-0");
    const auto occupied_file = temp / ("ariadne-case-" + process_id + "-1");
    std::error_code allocation_error;
    if (!std::filesystem::create_directory(occupied_directory, allocation_error) ||
        allocation_error) {
        std::cerr << "could not reserve a stale case directory: "
                  << allocation_error.message() << '\n';
        return 2;
    }
    {
        std::ofstream output(occupied_directory / "original.txt");
        output << "preserve this file";
    }
    {
        std::ofstream output(occupied_file);
        output << "preserve this ordinary file";
    }
    std::filesystem::path allocated;
    Suite allocation_suite("pre-existing case path ownership");
    allocation_suite.add("skip occupied paths", [&](Context& c) {
        allocated = c.work_directory();
        c.prove(allocated != occupied_directory && allocated != occupied_file,
                "allocation must not adopt an occupied path");
        c.prove(std::filesystem::is_regular_file(occupied_directory / "original.txt"),
                "stale directory contents must survive allocation");
        c.prove(std::filesystem::is_regular_file(occupied_file),
                "stale regular file must survive allocation");
    });
    Capture allocation_capture;
    const auto allocation_summary = allocation_suite.run(allocation_capture);
    ok &= verify(allocation_summary.passed == 1 && allocation_summary.errors == 0 &&
                     allocation_capture.cases.size() == 1,
                 "stale case-path collision affected test outcome");
    ok &= verify(!allocated.empty() && allocated != occupied_directory &&
                     allocated != occupied_file && !std::filesystem::exists(allocated),
                 "owned case directory was not cleaned");
    ok &= verify(std::filesystem::is_regular_file(occupied_directory / "original.txt") &&
                     std::filesystem::is_regular_file(occupied_file),
                 "allocator damaged a pre-existing entry");
    std::filesystem::remove_all(occupied_directory, allocation_error);
    ok &= verify(!allocation_error, "could not clean reserved directory");
    std::filesystem::remove(occupied_file, allocation_error);
    ok &= verify(!allocation_error, "could not clean reserved regular file");

    // std::cmp_* must be used for non-boolean integers; bool and floating
    // operands must take their ordinary C++20 relational path. Assert every
    // result, including the ordering of negative signed vs unsigned values.
    Suite comparison_suite("comparison operand type contracts");
    std::vector<std::array<bool, 5>> expected_comparisons;
    auto add_pair = [&]<class A, class B>(std::string label, A actual, B expected,
                                          std::array<bool, 5> outcomes) {
        expected_comparisons.push_back(outcomes);
        comparison_suite.add(std::move(label), [=](Context& c) {
            c.check_compare<Comparison::NotEqual>([&] { return actual; },
                                                  [&] { return expected; }, "not equal");
            c.check_compare<Comparison::Less>([&] { return actual; },
                                              [&] { return expected; }, "less");
            c.check_compare<Comparison::LessEqual>([&] { return actual; },
                                                   [&] { return expected; }, "less equal");
            c.check_compare<Comparison::Greater>([&] { return actual; },
                                                 [&] { return expected; }, "greater");
            c.check_compare<Comparison::GreaterEqual>([&] { return actual; },
                                                      [&] { return expected; }, "greater equal");
        });
    };
    add_pair("boolean operands", true, false, {true, false, false, true, true});
    add_pair("signed and boolean", -1, true, {true, true, true, false, false});
    add_pair("boolean and signed", true, 2, {true, true, true, false, false});
    add_pair("signed and unsigned", -1, 0u, {true, true, true, false, false});
    add_pair("integer and floating", -1, 0.5, {true, true, true, false, false});
    add_pair("floating and integer", 1.5, 2, {true, true, true, false, false});
    add_pair("nonintegral objects", std::string("a"), std::string("b"),
             {true, true, true, false, false});
    Capture comparison_capture;
    const auto comparison_summary = comparison_suite.run(comparison_capture);
    ok &= verify(comparison_summary.failed == expected_comparisons.size() &&
                     comparison_summary.errors == 0 &&
                     comparison_capture.cases.size() == expected_comparisons.size(),
                 "comparison type contract summary changed");
    if (comparison_capture.cases.size() == expected_comparisons.size()) {
        for (std::size_t i = 0; i < expected_comparisons.size(); ++i) {
            const auto& result = comparison_capture.cases[i];
            ok &= verify(result.assertions.size() == 5,
                         "missing comparison check in " + result.name);
            if (result.assertions.size() != 5)
                continue;
            for (std::size_t j = 0; j < 5; ++j) {
                const auto expected = expected_comparisons[i][j] ? Outcome::Passed : Outcome::Failed;
                ok &= verify(result.assertions[j].outcome == expected,
                             "incorrect relation " + std::to_string(j) + " in " + result.name);
            }
        }
    }

    // A moved Scope can outlive an enclosing Scope when the owners are
    // stored independently. Releasing the outer owner first must not
    // resurrect an already-closed context or grow the scope stack.
    Suite moved_scopes("moved diagnostic scope lifetimes");
    moved_scopes.add("out-of-order owner destruction", [](Context& c) {
        std::optional<Context::Scope> outer;
        std::optional<Context::Scope> inner;
        outer.emplace(c.scope("outer"));
        inner.emplace(c.scope("inner"));
        c.prove(true, "nested context is active");
        outer.reset();
        c.prove(true, "closed outer scope is not resurrected");
        inner.reset();
        c.prove(true, "late inner scope cleanup is harmless");
    });
    Capture scopes_report;
    const auto scopes_summary = moved_scopes.run(scopes_report);
    ok &= verify(scopes_summary.passed == 1 && scopes_summary.errors == 0 &&
                     scopes_report.cases.size() == 1,
                 "moved scopes: incorrect summary");
    if (scopes_report.cases.size() == 1) {
        const auto& assertions = scopes_report.cases[0].assertions;
        ok &= verify(assertions.size() == 3,
                     "moved scopes: unexpected assertion count");
        if (assertions.size() == 3) {
            ok &= verify(assertions[0].context.size() == 2 &&
                             assertions[0].context[0] == "outer" &&
                             assertions[0].context[1] == "inner" &&
                             assertions[1].context.empty() &&
                             assertions[2].context.empty(),
                         "moved scopes: stale or resurrected context");
        }
    }

    // An overlong path component is a real filesystem error on both POSIX
    // and Windows; missing files and invalid names can be classified as
    // not_found by MSVC, so neither proves the infrastructure-error path.
    Suite fs_suite("filesystem error boundaries");
    fs_suite.add("overlong filename", [](Context& c) {
        const auto too_long = c.work_directory() / std::string(1024, 'x');
        std::error_code status_error;
        const bool regular = std::filesystem::is_regular_file(too_long, status_error);
        if (regular || !status_error ||
            status_error == std::errc::no_such_file_or_directory ||
            status_error == std::errc::not_a_directory) {
            throw std::runtime_error(
                "overlong filename did not cause a non-absence filesystem error: " +
                status_error.message());
        }
        c.expect_file_exists(too_long, "overlong filename must be an error");
    });
#if !defined(_WIN32)
    // Additional POSIX-specific ELOOP regression. Windows junction cycles
    // do not reliably produce ELOOP, so the shared test above verifies the
    // same Error contract on Windows without a privileged symlink fixture.
    fs_suite.add("symlink loop", [](Context& c) {
        const auto loop = c.work_directory() / "self-loop";
        std::filesystem::create_symlink(loop.filename(), loop);
        c.expect_file_exists(loop, "cyclic symbolic link");
    });
#endif
    Capture fs_report;
    const auto fs_summary = fs_suite.run(fs_report);
#if defined(_WIN32)
    constexpr std::size_t expected_cases = 1;
#else
    constexpr std::size_t expected_cases = 2;
#endif
    ok &= verify(fs_summary.errors == expected_cases && fs_summary.failed == 0 &&
                     fs_report.cases.size() == expected_cases,
                 "filesystem errors must be classified as Error");
    for (const auto& result: fs_report.cases) {
        ok &= verify(result.outcome == Outcome::Error &&
                         result.assertions.size() == 1 &&
                         result.assertions[0].outcome == Outcome::Error &&
                         !result.assertions[0].detail.empty(),
                     "filesystem: missing Error assertion or underlying detail in " +
                         result.name);
    }

    // An adapter returning a nonstandard future_status must never masquerade
    // as ready, timed out or deferred. Cover relative and absolute deadlines.
    Suite unexpected_future("invalid future status classification");
    unexpected_future.add("relative invalid status", [](Context& c) {
        UnexpectedFuture future;
        c.expect_ready(future, std::chrono::milliseconds{1}, "relative readiness");
        c.prove(future.relative_calls == 1 && future.absolute_calls == 0,
                "relative wait must invoke only wait_for once");
    });
    unexpected_future.add("absolute invalid status", [](Context& c) {
        UnexpectedFuture future;
        c.expect_ready_until(future, std::chrono::steady_clock::now() + std::chrono::seconds{1}, "absolute readiness");
        c.prove(future.relative_calls == 0 && future.absolute_calls == 1,
                "absolute wait must invoke only wait_until once");
    });
    Capture unexpected_report;
    const auto unexpected_summary = unexpected_future.run(unexpected_report);
    ok &= verify(unexpected_summary.errors == 2 && unexpected_summary.passed == 0 &&
                     unexpected_report.cases.size() == 2,
                 "nonstandard future statuses must be errors");
    if (unexpected_report.cases.size() == 2) {
        for (std::size_t i = 0; i != 2; ++i) {
            const auto& entry = unexpected_report.cases[i];
            ok &= verify(entry.outcome == Outcome::Error &&
                             entry.assertions.size() == 2 &&
                             entry.assertions[0].outcome == Outcome::Error &&
                             entry.assertions[0].actual == "unexpected future status" &&
                             entry.assertions[0].wait.has_value() &&
                             entry.assertions[0].wait->status == WaitStatus::Unexpected &&
                             entry.assertions[0].wait->absolute_deadline == (i == 1) &&
                             entry.assertions[1].outcome == Outcome::Passed,
                         "invalid future status or deadline diagnostic changed at " +
                             std::to_string(i));
        }
    }

    // A Context can be constructed around a caller-owned CaseResult. An
    // unknown prior outcome must not be silently downgraded by later checks.
    // This also exercises the defensive Outcome priority contract.
    Suite context_boundaries("external result and temporary directory contracts");
    context_boundaries.add("unknown prior outcome retains blocking priority", [](Context& c) {
        CaseResult external;
        external.outcome = static_cast<Outcome>(99);
        {
            Context context(external);
            context.prove(true, "subsequent successful assertion");
            context.prove(false, "subsequent failed assertion");
        }
        c.prove(external.outcome == static_cast<Outcome>(99) &&
                    external.assertions.size() == 2 &&
                    external.assertions[0].outcome == Outcome::Passed &&
                    external.assertions[1].outcome == Outcome::Failed,
                "unknown input outcomes retain error-level priority");
    });
#if !defined(_WIN32)
    context_boundaries.add("unwritable temporary root rejects case allocation", [](Context& c) {
        if (::geteuid() == 0) {
            c.skip("permission restrictions are not enforced for root");
            return;
        }
        const auto restricted = c.work_directory() / "restricted";
        std::filesystem::create_directory(restricted);
        std::filesystem::permissions(restricted,
                                     std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);
        const char* current = std::getenv("TMPDIR");
        std::optional<std::string> original =
            current ? std::optional<std::string>{current} : std::nullopt;
        bool rejected = false;
        const bool configured = ::setenv("TMPDIR", restricted.c_str(), 1) == 0;
        if (configured) {
            try {
                CaseResult attempted;
                Context isolated(attempted);
                (void)isolated.work_directory();
            } catch (const std::filesystem::filesystem_error& error) {
                rejected = error.code() == std::errc::permission_denied &&
                           std::string(error.what()).find("create work directory") !=
                               std::string::npos;
            }
        }
        if (original)
            (void)::setenv("TMPDIR", original->c_str(), 1);
        else
            (void)::unsetenv("TMPDIR");
        std::filesystem::permissions(restricted, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        c.prove(configured && rejected,
                "permission denial must be reported as filesystem_error");
    });
#endif
    Capture context_boundary_capture;
    const auto context_boundary_summary = context_boundaries.run(context_boundary_capture);
#if defined(_WIN32)
    constexpr std::size_t context_boundary_count = 1;
#else
    constexpr std::size_t context_boundary_count = 2;
#endif
    ok &= verify(context_boundary_summary.passed + context_boundary_summary.skipped ==
                         context_boundary_count &&
                     context_boundary_summary.failed == 0 &&
                     context_boundary_summary.errors == 0 &&
                     context_boundary_capture.cases.size() == context_boundary_count,
                 "external result or filesystem boundary regression");
    return ok ? 0 : 1;
}

