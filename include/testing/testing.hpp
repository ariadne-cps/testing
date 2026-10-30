#ifndef ARIADNE_TESTING_TESTING_HPP
#define ARIADNE_TESTING_TESTING_HPP

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Ariadne::Testing {

enum class Evidence { False, True, Indeterminate };
enum class Outcome { Passed, Failed, Inconclusive, Skipped, Error };

struct AssertionRecord {
    Outcome outcome;
    std::string description;
    std::string actual;
    std::string expected;
    std::string detail;
    std::string file;
    std::uint_least32_t line;
    std::size_t check_index = 0;
};

struct CaseResult {
    std::string name;
    Outcome outcome = Outcome::Passed;
    std::vector<AssertionRecord> assertions;
    std::chrono::nanoseconds duration{};
    std::string suite_name;
    std::size_t case_index = 0;
    std::size_t case_total = 0;
};

struct Summary {
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t inconclusive = 0;
    std::size_t skipped = 0;
    std::size_t errors = 0;
    bool successful() const { return passed > 0 && failed == 0 && inconclusive == 0 && errors == 0; }
};

class Reporter {
  public:
    virtual ~Reporter() = default;
    // Optional lifecycle notifications, compatible with existing reporters.
    virtual void case_started(CaseResult const&) { }
    virtual void check_started(CaseResult const&, std::size_t, std::string_view) { }
    virtual void case_finished(CaseResult const& result) = 0;
    virtual void suite_finished(std::string_view name, Summary const& summary) = 0;
};

class ConsoleReporter final : public Reporter {
  public:
    explicit ConsoleReporter(std::ostream& output);
    void case_started(CaseResult const& result) override;
    void check_started(CaseResult const& result, std::size_t index,
                       std::string_view description) override;
    void case_finished(CaseResult const& result) override;
    void suite_finished(std::string_view name, Summary const& summary) override;
  private:
    std::ostream& _output;
};

namespace detail {

template<class T>
Evidence evidence(T const& value) {
    if constexpr (requires { definitely(value); possibly(value); }) {
        bool const definite = static_cast<bool>(definitely(value));
        bool const possible = static_cast<bool>(possibly(value));
        if (definite && !possible) {
            throw std::logic_error("Inconsistent logical evidence: definitely true but not possibly true");
        }
        if (definite) return Evidence::True;
        if (!possible) return Evidence::False;
        return Evidence::Indeterminate;
    } else {
        return static_cast<bool>(value) ? Evidence::True : Evidence::False;
    }
}

template<class T>
std::string render(T const& value) {
    if constexpr (requires(std::ostream& os) { os << value; }) {
        std::ostringstream os;
        os << std::boolalpha << value;
        return os.str();
    } else {
        return "<unprintable>";
    }
}

template<class T>
auto magnitude(T const& value) {
    if constexpr (requires { mag(value); }) {
        return mag(value);
    } else {
        using std::abs;
        return abs(value);
    }
}

} // namespace detail

class Context {
  public:
    explicit Context(CaseResult& result, Reporter* reporter = nullptr)
        : _result(result), _reporter(reporter) { }

    template<class T>
    void prove(T const& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        record(outcome(detail::evidence(predicate)), description, {}, {}, {}, location, index);
    }

    template<class A, class B>
    void expect_equal(A const& actual, B const& expected,
                      std::string_view description = "equality",
                      std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const verdict = [&]() {
            using Actual = std::remove_cvref_t<A>;
            using Expected = std::remove_cvref_t<B>;
            if constexpr (std::is_integral_v<Actual> && std::is_integral_v<Expected>
                          && !std::is_same_v<Actual,bool> && !std::is_same_v<Expected,bool>) {
                return detail::evidence(std::cmp_equal(actual,expected));
            } else {
                return detail::evidence(actual == expected);
            }
        }();
        record(outcome(verdict), description,
               detail::render(actual), detail::render(expected), {}, location, index);
    }

    template<class A, class B>
    void expect_same(A const& actual, B const& expected,
                     std::string_view description = "identity",
                     std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const relation = same(actual, expected); // ADL: supplied by numerical domain
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected), {}, location, index);
    }

    template<class A, class B, class Tolerance>
    void expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description = "bounded error",
                       std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const error = detail::magnitude(actual - expected);
        auto const relation = (error <= tolerance);
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected),
               "error=" + detail::render(error) + ", tolerance=" + detail::render(tolerance), location, index);
    }

    template<class Exception, class Callable>
    void expect_throws(Callable&& action, std::string_view description = "expected exception",
                       std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Exception const&) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, std::string("unexpected exception: ") + e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception type", location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }
    template<class Callable>
    void expect_exception(Callable&& action, std::string_view description = "exception expected",
                          std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (...) { record(Outcome::Passed, description, {}, {}, {}, location, index); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location, index);
    }
    template<class Callable>
    void expect_no_throw(Callable&& action, std::string_view description = "no exception expected",
                         std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        try { std::invoke(std::forward<Callable>(action)); }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, e.what(), location, index); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception", location, index); return; }
        record(Outcome::Passed, description, {}, {}, {}, location, index);
    }

    // Deferred checker: emit progress before evaluating its potentially costly predicate.
    template<class Callable>
    void check(Callable&& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        auto const index = begin_check(description);
        auto const value = std::invoke(std::forward<Callable>(predicate));
        record(outcome(detail::evidence(value)), description, {}, {}, {}, location, index);
    }

    void skip(std::string_view reason,
              std::source_location location = std::source_location::current());

  private:
    friend class Suite;
    static Outcome outcome(Evidence value);
    std::size_t begin_check(std::string_view description);
    void record(Outcome outcome, std::string_view description, std::string actual,
                std::string expected, std::string detail, std::source_location location,
                std::size_t index);
    CaseResult& _result;
    Reporter* _reporter;
    std::size_t _check_count = 0;
};

class Suite {
  public:
    explicit Suite(std::string name) : _name(std::move(name)) { }
    void add(std::string name, std::function<void(Context&)> test);
    Summary run(Reporter& reporter) const;
  private:
    struct TestCase {
        std::string name;
        std::function<void(Context&)> run;
    };
    std::string _name;
    std::vector<TestCase> _tests;
};

} // namespace Ariadne::Testing

#endif // ARIADNE_TESTING_TESTING_HPP
