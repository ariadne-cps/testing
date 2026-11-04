#include "ariadne-testing.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace Ariadne::Testing;
using namespace std::chrono_literals;

struct Capture final : Reporter {
    std::vector<CaseResult> results;
    int checks = 0;
    void check_started(CaseResult const&, std::size_t, std::string_view) override { ++checks; }
    void case_finished(CaseResult const& result) override { results.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override { }
};

struct DeadlineFixture : ContextFixture {
    using ContextFixture::ContextFixture;
    void test_future() {
        std::promise<int> promise;
        auto future = promise.get_future();
        promise.set_value(7);
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        ARIADNE_TESTING_EXPECT_READY_UNTIL(future, deadline);
    }
    void test_condition() {
        std::condition_variable_any condition;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL(condition, lock, [] { return true; }, deadline);
    }
    void test_atomic() {
        std::atomic<int> state{9};
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL(state, [](int value) { return value == 9; }, deadline);
    }
};

int main() {
    Suite suite("shared steady-clock deadline");
    Capture capture;
    // wait_until(lock, deadline, predicate) may invoke its predicate more
    // than once, including a final recheck after timeout. Count each
    // scenario separately rather than asserting a total invocation count.
    std::array<int, 3> condition_calls{0, 0, 0};
    int atomic_calls = 0;
    suite.add("expired deadline accepts already-satisfied state", [&](Context& c) {
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        std::promise<int> promise;
        auto future = promise.get_future();
        promise.set_value(5);
        c.expect_ready_until(future, deadline, "already-ready future");
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually_until(cv, lock, [&] { ++condition_calls[0]; return true; },
                                  deadline, "already-met condition");
        std::atomic<int> state{4};
        c.expect_atomic_until(state, [&](int value) {
            ++atomic_calls; return value == 4;
        }, deadline, "already-met atomic");
    });
    suite.add("expired deadline rejects unsatisfied state", [&](Context& c) {
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        std::promise<int> promise;
        auto future = promise.get_future();
        c.expect_ready_until(future, deadline, "unready future");
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually_until(cv, lock, [&] { ++condition_calls[1]; return false; },
                                  deadline, "unmet condition");
        std::atomic<int> state{2};
        c.expect_atomic_until(state, [&](int value) {
            ++atomic_calls; return value == 3;
        }, deadline, "unmet atomic");
    });
    suite.add("deferred future does not execute", [](Context& c) {
        auto deferred = std::async(std::launch::deferred, [] { return 9; });
        auto deadline = std::chrono::steady_clock::now() + 1s;
        c.expect_ready_until(deferred, deadline, "deferred is not ready");
    });
    suite.add("one deadline shared across synchronized worker updates", [&](Context& c) {
        std::atomic<int> state{0};
        bool completed = false;
        std::mutex mutex;
        std::condition_variable_any cv;
        std::promise<void> finished_promise;
        auto completed_future = finished_promise.get_future();
        std::jthread worker([&] {
            { std::lock_guard<std::mutex> guard(mutex);
              completed = true;
              state.store(11, std::memory_order_release);
            }
            cv.notify_all();
            finished_promise.set_value();
        });
        auto deadline = std::chrono::steady_clock::now() + 5s;
        c.expect_ready_until(completed_future, deadline, "worker finished");
        std::unique_lock<std::mutex> lock(mutex);
        c.expect_eventually_until(cv, lock, [&] { return completed; },
                                  deadline, "worker notified");
        lock.unlock();
        c.expect_atomic_until(state, [](int value) { return value == 11; },
                              deadline, "worker published");
        worker.join();
    });
    suite.add("future times out without resetting deadline", [](Context& c) {
        std::promise<void> promise;
        auto future = promise.get_future();
        auto deadline = std::chrono::steady_clock::now() + 2ms;
        c.expect_ready_until(future, deadline, "bounded future wait");
    });
    suite.add("predicate exception is Error", [](Context& c) {
        std::atomic<int> state{1};
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        c.expect_atomic_until(state, [](int) -> bool {
            throw std::runtime_error("absolute predicate failure");
        }, deadline, "broken predicate");
    });
    suite.add("explicit-context deadline macros", [&](Context& c) {
        auto deadline = std::chrono::steady_clock::now() - 1ms;
        std::promise<void> promise;
        auto future = promise.get_future();
        promise.set_value();
        ARIADNE_TESTING_EXPECT_READY_UNTIL_CTX(c, future, deadline);
        std::condition_variable_any cv;
        std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        ARIADNE_TESTING_EXPECT_EVENTUALLY_UNTIL_CTX(c, cv, lock,
            [&] { ++condition_calls[2]; return true; }, deadline);
        std::atomic<int> state{5};
        ARIADNE_TESTING_EXPECT_ATOMIC_UNTIL_CTX(c, state,
            [&](int value) { ++atomic_calls; return value == 5; }, deadline);
    });
    ARIADNE_TESTING_ADD_TEST_TO(suite, DeadlineFixture, test_future);
    ARIADNE_TESTING_ADD_TEST_TO(suite, DeadlineFixture, test_condition);
    ARIADNE_TESTING_ADD_TEST_TO(suite, DeadlineFixture, test_atomic);

    auto const summary = suite.run(capture);
    bool ok = true;
    auto verify = [&](bool result, std::string const& description) {
        if (!result) { std::cerr << description << '\n'; ok = false; }
    };
    verify(summary.passed == 6 && summary.failed == 3 && summary.errors == 1 &&
           summary.inconclusive == 0 && summary.skipped == 0 &&
           capture.results.size() == 10 && capture.checks == 18,
           "shared deadline summary or CHECK count: passed=" +
               std::to_string(summary.passed) + ", failed=" +
               std::to_string(summary.failed) + ", errors=" +
               std::to_string(summary.errors) + ", checks=" +
               std::to_string(capture.checks));
    // Predicate-based condition waits may recheck after a timeout, including
    // on a deadline already in the past. They promise no exact call count.
    // Every expected path must nevertheless exercise its predicate.
    verify(condition_calls[0] >= 1 &&
           condition_calls[1] >= 1 &&
           condition_calls[2] >= 1,
           "condition predicates not exercised: " +
               std::to_string(condition_calls[0]) + "/" +
               std::to_string(condition_calls[1]) + "/" +
               std::to_string(condition_calls[2]));
    // Each atomic observation uses an already-satisfied or expired deadline
    // and therefore must invoke its predicate once per call.
    verify(atomic_calls == 3,
           "atomic predicates unexpectedly evaluated " +
               std::to_string(atomic_calls) + " times");
    if (capture.results.size() == 10) {
        std::vector<Outcome> const expected{
            Outcome::Passed, Outcome::Failed, Outcome::Failed,
            Outcome::Passed, Outcome::Failed, Outcome::Error,
            Outcome::Passed, Outcome::Passed, Outcome::Passed, Outcome::Passed
        };
        for (std::size_t i = 0; i < expected.size(); ++i) {
            verify(capture.results[i].outcome == expected[i],
                   "deadline case outcome mismatch: " + std::to_string(i));
        }
        auto const& initial = capture.results[0].assertions;
        verify(initial.size() == 3, "missing initially-satisfied checks");
        if (initial.size() == 3)
            for (auto const& assertion : initial)
                verify(assertion.detail.find("remaining_at_start=0ms") != std::string::npos,
                       "expired deadline must not be restarted");

        auto const& expired = capture.results[1].assertions;
        verify(expired.size() == 3, "missing expired checks");
        if (expired.size() == 3) {
            verify(expired[0].actual == "timeout" &&
                   expired[1].actual == "timeout" &&
                   expired[2].actual == "2" &&
                   expired[2].detail.find("timeout; ") == 0,
                   "expired states not reported faithfully");
        }
        auto const& deferred = capture.results[2].assertions;
        if (deferred.size() == 1)
            verify(deferred[0].actual == "deferred",
                   "deferred future must not execute on wait");
        auto const& broken = capture.results[5].assertions;
        if (broken.size() == 1)
            verify(broken[0].detail == "absolute predicate failure",
                   "predicate exception detail lost");
    }

    // The state must change after an initial unsuccessful observation. A
    // promise synchronizes the producer with the first predicate evaluation,
    // so the test exercises real polling without assuming thread start order.
    Suite atomic_transition("atomic transition during absolute wait");
    atomic_transition.add("value published after waiting starts", [](Context& c) {
        std::atomic<int> state{0};
        std::promise<void> first_observed;
        auto observed = first_observed.get_future();
        bool notified = false;
        std::jthread producer([&] {
            if (observed.wait_for(2s) != std::future_status::ready) return;
            std::this_thread::sleep_for(2ms);
            state.store(1, std::memory_order_release);
        });
        int inspections = 0;
        c.expect_atomic_until(state, [&](int value) {
            ++inspections;
            if (value == 0 && !notified) {
                notified = true;
                first_observed.set_value();
            }
            return value == 1;
        }, std::chrono::steady_clock::now() + 3s,
        "updated atomic state becomes ready");
        producer.join();
        c.prove(inspections >= 2,
                "predicate observed an initial mismatch and the updated state");
    });
    Capture transition_capture;
    auto const transition_summary = atomic_transition.run(transition_capture);
    verify(transition_summary.passed == 1 && transition_summary.failed == 0 &&
           transition_summary.errors == 0 && transition_capture.checks == 2 &&
           transition_capture.results.size() == 1,
           "atomic transition must complete successfully within its deadline");
    if (transition_capture.results.size() == 1) {
        auto const& assertions = transition_capture.results[0].assertions;
        verify(assertions.size() == 2 &&
               assertions[0].outcome == Outcome::Passed &&
               assertions[0].actual == "1" &&
               assertions[0].wait.has_value() &&
               assertions[0].wait->status == WaitStatus::Ready &&
               assertions[0].wait->absolute_deadline,
               "atomic transition must preserve ready status and deadline");
    }

    // The same fixture methods must also take the NoThrow registration branch,
    // wrapping the body in expect_no_throw without changing these outcomes.
    Suite no_throw_suite("deadline fixture NoThrow policy");
    add_fixture_test<DeadlineFixture, &DeadlineFixture::test_future>(
        no_throw_suite, "future", FixtureCheck::NoThrow);
    add_fixture_test<DeadlineFixture, &DeadlineFixture::test_condition>(
        no_throw_suite, "condition", FixtureCheck::NoThrow);
    add_fixture_test<DeadlineFixture, &DeadlineFixture::test_atomic>(
        no_throw_suite, "atomic", FixtureCheck::NoThrow);
    Capture no_throw_capture;
    auto const no_throw_summary = no_throw_suite.run(no_throw_capture);
    verify(no_throw_summary.passed == 3 && no_throw_summary.failed == 0 &&
           no_throw_summary.errors == 0 && no_throw_capture.results.size() == 3,
           "NoThrow deadline fixture outcomes changed");
    return ok ? 0 : 1;
}
