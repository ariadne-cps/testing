#include "ariadne-testing.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <vector>

using namespace Ariadne;

struct Capture : Reporter {
    std::vector<CaseResult> results;
    void case_finished(CaseResult const& result) override { results.push_back(result); }
    void suite_finished(std::string_view, Summary const&) override {}
};

int main() {
    int global_setting=11;
    int saves=0, restores=0;
    Suite suite("state isolation");
    suite.add("value restored", [&](Context& ctx) {
        {
            auto guard=preserve_value(global_setting);
            global_setting=37;
            ctx.expect_equal(global_setting,37,"modified in scope");
        }
        ctx.expect_equal(global_setting,11,"restored on normal exit");
    });
    suite.add("nested restoration", [&](Context& ctx) {
        auto outer=preserve_value(global_setting);
        global_setting=22;
        {
            auto inner=preserve_value(global_setting);
            global_setting=33;
        }
        ctx.expect_equal(global_setting,22,"inner snapshot restored");
    });
    suite.add("after nested case", [&](Context& ctx) {
        ctx.expect_equal(global_setting,11,"no leakage between cases");
    });
    suite.add("unwinding restores", [&](Context& ctx) {
        try {
            auto guard=preserve_value(global_setting);
            global_setting=44;
            throw std::runtime_error("intentional");
        } catch (std::runtime_error const&) {
            ctx.expect_equal(global_setting,11,"restored during unwinding");
        }
    });
    suite.add("custom getter/restorer", [&](Context& ctx) {
        auto guard=preserve_state([&] { ++saves; return global_setting; },
            [&](int const& original) noexcept { ++restores; global_setting=original; });
        global_setting=55;
        ctx.expect_equal(global_setting,55,"custom state changed");
    });
    suite.add("after custom case", [&](Context& ctx) {
        ctx.expect_equal(global_setting,11,"custom restore across cases");
        ctx.expect_equal(saves,1,"getter called once");
        ctx.expect_equal(restores,1,"restorer called once");
    });
    suite.add("failed requirement unwinds", [&](Context& ctx) {
        auto guard=preserve_value(global_setting);
        global_setting=66;
        ctx.require([] { return false; },"fatal prerequisite");
    });
    suite.add("after aborted case", [&](Context& ctx) {
        ctx.expect_equal(global_setting,11,"aborted case restored state");
    });
    Capture reporter;
    auto result=suite.run(reporter);
    bool const ok=result.passed==7 && result.failed==1 &&
        result.errors==0 && result.inconclusive==0 &&
        reporter.results.size()==8 && global_setting==11;
    if(!ok) {
        std::cerr<<"test_state: passed="<<result.passed<<", failed="<<result.failed
                 <<", errors="<<result.errors<<", inconclusive="<<result.inconclusive
                 <<", cases="<<reporter.results.size()<<", value="<<global_setting<<"\n";
        for(auto const& test:reporter.results) {
            std::cerr<<"  "<<test.name<<": "<<static_cast<int>(test.outcome)<<"\n";
        }
        return 1;
    }
    return 0;
}
