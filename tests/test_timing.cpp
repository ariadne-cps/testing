/***************************************************************************
 *            test_timing.cpp
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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "ariadne-testing.hpp"
using namespace Ariadne::Testing;
using namespace std::chrono_literals;

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(const CaseResult& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, const Summary&) override {}
};

int main() {
    Suite suite("wait observations");
    suite.add("relative success", [](Context& c) {
        std::promise<int> p;
        auto future = p.get_future();
        p.set_value(1);
        c.expect_ready(future, 0ms, "ready future");
        std::condition_variable_any cv;
        std::mutex m;
        std::unique_lock<std::mutex> l(m);
        c.expect_eventually(cv, l, [] { return true; }, 0ms, "ready condition");
        std::atomic<int> a{1};
        c.expect_atomic(a, [](int v) { return v == 1; }, 0ms, "ready atomic");
    });
    suite.add("expired absolute waits", [](Context& c) {
        const auto deadline = std::chrono::steady_clock::now() - 1ms;
        std::promise<int> p;
        auto future = p.get_future();
        c.expect_ready_until(future, deadline, "timeout future");
        std::condition_variable_any cv;
        std::mutex m;
        std::unique_lock<std::mutex> l(m);
        c.expect_eventually_until(cv, l, [] { return false; }, deadline, "timeout condition");
        std::atomic<int> a{1};
        c.expect_atomic_until(a, [](int v) { return v == 2; }, deadline, "timeout atomic");
    });
    suite.add("invalid relative waits", [](Context& c) {
        std::promise<int> p;
        auto future = p.get_future();
        c.expect_ready(future, -1ms, "invalid future");
        std::condition_variable_any cv;
        std::mutex m;
        std::unique_lock<std::mutex> l(m);
        c.expect_eventually(cv, l, [] { return true; }, -1ms, "invalid condition");
        std::atomic<int> a{1};
        c.expect_atomic(a, [](int) { return true; }, -1ms, "invalid atomic");
    });
    suite.add("deferred future and timing note", [](Context& c) {
        auto future = std::async(std::launch::deferred, [] { return 1; });
        c.expect_ready(future, 0ms, "deferred future");
        c.elapsed("measurement is not a wait", 10ns);
    });
    suite.add("absolute successes even after deadline", [](Context& c) {
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        std::promise<int> p;
        auto future = p.get_future();
        p.set_value(2);
        c.expect_ready_until(future, deadline, "ready future");
        std::condition_variable_any cv;
        std::mutex m;
        std::unique_lock<std::mutex> l(m);
        c.expect_eventually_until(cv, l, [] { return true; }, deadline, "ready condition");
        std::atomic<int> a{1};
        c.expect_atomic_until(a, [](int v) { return v == 1; }, deadline, "ready atomic");
    });
    Capture capture;
    const auto summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << message << '\n';
            ok = false;
        }
    };
    verify(summary.passed == 2 && summary.failed == 2 && summary.errors == 1 &&
               capture.cases.size() == 5,
           "unexpected timing suite summary");
    if (capture.cases.size() == 5) {
        auto inspect = [&](std::size_t index, WaitStatus status, bool absolute) {
            const auto& checks = capture.cases[index].assertions;
            verify(checks.size() == 3, "expected three timed assertions");
            for (const auto& a: checks) {
                verify(a.wait.has_value(), "missing wait observation");
                if (a.wait)
                    verify(a.wait->status == status &&
                               a.wait->absolute_deadline == absolute &&
                               a.wait->elapsed >= 0ns &&
                               a.wait->available_budget >= 0ns &&
                               (!absolute || a.wait->available_budget == 0ns),
                           "incorrect wait status or budget");
            }
        };
        inspect(0, WaitStatus::Ready, false);
        inspect(1, WaitStatus::Timeout, true);
        inspect(4, WaitStatus::Ready, true);
        const auto& errors = capture.cases[2].assertions;
        verify(errors.size() == 3, "missing invalid wait checks");
        for (const auto& a: errors)
            verify(a.outcome == Outcome::Error && a.wait &&
                       a.wait->status == WaitStatus::InvalidTimeout &&
                       a.wait->available_budget < 0ns,
                   "invalid limit must remain observable");
        const auto& deferred = capture.cases[3];
        verify(deferred.assertions.size() == 1 && deferred.assertions[0].wait &&
                   deferred.assertions[0].wait->status == WaitStatus::Deferred &&
                   deferred.diagnostics.size() == 1 &&
                   deferred.diagnostics[0].kind == DiagnosticKind::Elapsed &&
                   deferred.diagnostics[0].duration == 10ns,
               "deferred wait and elapsed note must remain separate");
    }

    // The same measure() callable specialization must emit an elapsed event
    // on normal return, std::exception, and non-standard exception. Exceptions
    // propagate to Suite, and the following test remains runnable.
    Suite measured("measurement exception propagation");
    for (int mode = 0; mode < 3; ++mode) {
        measured.add("measurement " + std::to_string(mode), [mode](Context& c) {
            std::function<void()> operation = [mode] {
                if (mode == 1)
                    throw std::runtime_error("measured standard failure");
                if (mode == 2)
                    throw 17;
            };
            c.measure("measure including exceptional exits", operation);
            c.prove(true, "normal measured operation continues");
        });
    }
    Capture measured_capture;
    const auto measured_summary = measured.run(measured_capture);
    verify(measured_summary.passed == 1 && measured_summary.errors == 2 &&
               measured_summary.failed == 0 && measured_capture.cases.size() == 3,
           "measure must preserve normal and exceptional outcomes");
    if (measured_capture.cases.size() == 3) {
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& result = measured_capture.cases[i];
            verify(result.diagnostics.size() == 1 &&
                       result.diagnostics[0].kind == DiagnosticKind::Elapsed &&
                       result.diagnostics[0].description ==
                           "measure including exceptional exits" &&
                       result.diagnostics[0].duration.count() >= 0 &&
                       result.assertions.size() == 1 &&
                       result.outcome == (i == 0 ? Outcome::Passed : Outcome::Error),
                   "measure lost a diagnostic or failed to propagate an exception");
        }
        verify(measured_capture.cases[1].assertions[0].detail ==
                       "measured standard failure" &&
                   measured_capture.cases[2].assertions[0].description ==
                       "Unhandled non-standard exception",
               "measure changed the exception classification or detail");
    }
    return ok ? 0 : 1;
}
