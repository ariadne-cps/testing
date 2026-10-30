#include "testing/testing.hpp"

#include <exception>
#include <iostream>
#include <sstream>


namespace Ariadne::Testing {
namespace {

std::string_view outcome_name(Outcome result) {
    switch (result) {
        case Outcome::Passed: return "PASS";
        case Outcome::Failed: return "FAIL";
        case Outcome::Inconclusive: return "INCONCLUSIVE";
        case Outcome::Skipped: return "SKIP";
        case Outcome::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

int priority(Outcome result) {
    switch (result) {
        case Outcome::Passed: return 0;
        case Outcome::Skipped: return 1;
        case Outcome::Inconclusive: return 2;
        case Outcome::Failed: return 3;
        case Outcome::Error: return 4;
        default: return 4;
    }
}

std::string summary_text(std::string_view name, Summary const& summary) {
    std::ostringstream os;
    os << name << ": " << summary.passed << " passed, " << summary.failed << " failed, "
       << summary.inconclusive << " inconclusive, " << summary.skipped << " skipped, "
       << summary.errors << " errors";
    return os.str();
}

} // namespace

ConsoleReporter::ConsoleReporter(std::ostream& output) : _output(output) { }

void ConsoleReporter::case_finished(CaseResult const& result) {
    _output << '[' << outcome_name(result.outcome) << "] " << result.name << '\n';
    if (result.outcome == Outcome::Passed) return;
    for (auto const& assertion : result.assertions) {
        if (assertion.outcome == Outcome::Passed) continue;
        _output << "  " << outcome_name(assertion.outcome) << ": " << assertion.description << '\n';
        if (!assertion.actual.empty()) _output << "    actual: " << assertion.actual << '\n';
        if (!assertion.expected.empty()) _output << "    expected: " << assertion.expected << '\n';
        if (!assertion.detail.empty()) _output << "    detail: " << assertion.detail << '\n';
        if (!assertion.file.empty()) _output << "    at " << assertion.file << ':' << assertion.line << '\n';
    }
}

void ConsoleReporter::suite_finished(std::string_view name, Summary const& summary) {
    _output << summary_text(name,summary) << '\n';
}

Outcome Context::outcome(Evidence value) {
    switch (value) {
        case Evidence::True: return Outcome::Passed;
        case Evidence::False: return Outcome::Failed;
        case Evidence::Indeterminate: return Outcome::Inconclusive;
        default: return Outcome::Error;
    }
}

void Context::record(Outcome status, std::string_view description,
                     std::string actual, std::string expected, std::string detail,
                     std::source_location location) {
    _result.assertions.push_back({status, std::string(description), std::move(actual),
        std::move(expected), std::move(detail), location.file_name(), location.line()});
    if (priority(status) > priority(_result.outcome)) _result.outcome = status;
}

void Context::skip(std::string_view reason, std::source_location location) {
    record(Outcome::Skipped, reason, {}, {}, {}, location);
}

void Suite::add(std::string name, std::function<void(Context&)> test) {
    if (name.empty() || !test) throw std::invalid_argument("Test name and body must be nonempty");
    _tests.push_back({std::move(name),std::move(test)});
}

Summary Suite::run(Reporter& reporter) const {
    Summary summary;
    for (auto const& test : _tests) {
        CaseResult result;
        result.name = test.name;
        Context context(result);
        auto const started = std::chrono::steady_clock::now();
        try {
            test.run(context);
        } catch (std::exception const& exception) {
            context.record(Outcome::Error, "Unhandled std::exception", {}, {}, exception.what(),
                           std::source_location::current());
        } catch (...) {
            context.record(Outcome::Error, "Unhandled non-standard exception", {}, {}, {},
                           std::source_location::current());
        }
        result.duration = std::chrono::steady_clock::now() - started;
        if (result.assertions.empty()) {
            context.record(Outcome::Skipped, "No assertions recorded", {}, {}, {},
                           std::source_location::current());
        }
        switch (result.outcome) {
            case Outcome::Passed: ++summary.passed; break;
            case Outcome::Failed: ++summary.failed; break;
            case Outcome::Inconclusive: ++summary.inconclusive; break;
            case Outcome::Skipped: ++summary.skipped; break;
            case Outcome::Error: ++summary.errors; break;
            default:
                ++summary.errors;
                result.outcome = Outcome::Error;
                break;
        }
        reporter.case_finished(result);
    }
    reporter.suite_finished(_name, summary);
    return summary;
}

} // namespace Ariadne::Testing
