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
};

struct CaseResult {
    std::string name;
    Outcome outcome = Outcome::Passed;
    std::vector<AssertionRecord> assertions;
    std::chrono::nanoseconds duration{};
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
    virtual void case_finished(CaseResult const& result) = 0;
    virtual void suite_finished(std::string_view name, Summary const& summary) = 0;
};

class ConsoleReporter final : public Reporter {
  public:
    explicit ConsoleReporter(std::ostream& output);
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
    explicit Context(CaseResult& result) : _result(result) { }

    template<class T>
    void prove(T const& predicate, std::string_view description,
               std::source_location location = std::source_location::current()) {
        record(outcome(detail::evidence(predicate)), description, {}, {}, {}, location);
    }

    template<class A, class B>
    void expect_equal(A const& actual, B const& expected,
                      std::string_view description = "equality",
                      std::source_location location = std::source_location::current()) {
        auto const relation = (actual == expected);
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected), {}, location);
    }

    template<class A, class B>
    void expect_same(A const& actual, B const& expected,
                     std::string_view description = "identity",
                     std::source_location location = std::source_location::current()) {
        auto const relation = same(actual, expected); // ADL: supplied by numerical domain
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected), {}, location);
    }

    template<class A, class B, class Tolerance>
    void expect_within(A const& actual, B const& expected, Tolerance const& tolerance,
                       std::string_view description = "bounded error",
                       std::source_location location = std::source_location::current()) {
        auto const error = detail::magnitude(actual - expected);
        auto const relation = (error <= tolerance);
        record(outcome(detail::evidence(relation)), description,
               detail::render(actual), detail::render(expected),
               "error=" + detail::render(error) + ", tolerance=" + detail::render(tolerance), location);
    }

    template<class Exception, class Callable>
    void expect_throws(Callable&& action, std::string_view description = "expected exception",
                       std::source_location location = std::source_location::current()) {
        try { std::invoke(std::forward<Callable>(action)); }
        catch (Exception const&) { record(Outcome::Passed, description, {}, {}, {}, location); return; }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, std::string("unexpected exception: ") + e.what(), location); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception type", location); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location);
    }
    template<class Callable>
    void expect_exception(Callable&& action, std::string_view description = "exception expected",
                          std::source_location location = std::source_location::current()) {
        try { std::invoke(std::forward<Callable>(action)); }
        catch (...) { record(Outcome::Passed, description, {}, {}, {}, location); return; }
        record(Outcome::Failed, description, {}, {}, "no exception thrown", location);
    }
    template<class Callable>
    void expect_no_throw(Callable&& action, std::string_view description = "no exception expected",
                         std::source_location location = std::source_location::current()) {
        try { std::invoke(std::forward<Callable>(action)); }
        catch (std::exception const& e) {
            record(Outcome::Failed, description, {}, {}, e.what(), location); return;
        }
        catch (...) { record(Outcome::Failed, description, {}, {}, "unexpected exception", location); return; }
        record(Outcome::Passed, description, {}, {}, {}, location);
    }

    void skip(std::string_view reason,
              std::source_location location = std::source_location::current());

  private:
    friend class Suite;
    static Outcome outcome(Evidence value);
    void record(Outcome outcome, std::string_view description, std::string actual,
                std::string expected, std::string detail, std::source_location location);
    CaseResult& _result;
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
