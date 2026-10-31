#include "ariadne-testing.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Ariadne::Testing;

struct Capture : Reporter {
    std::vector<CaseResult> cases;
    void case_finished(CaseResult const& r) override { cases.push_back(r); }
    void suite_finished(std::string_view, Summary const&) override {}
};
struct Trace {
    std::vector<std::string> events;
    int instances=0;
};
struct Fixture : ContextFixture {
    static inline Trace* trace=nullptr;
    int local=0;
    explicit Fixture(Context& c):ContextFixture(c) {
        ++trace->instances; trace->events.push_back("construct");
    }
    ~Fixture(){trace->events.push_back("destroy");}
    void set_up(){trace->events.push_back("setup");}
    void tear_down(){trace->events.push_back("teardown");}
    void test_modify(){
        trace->events.push_back("body");
        ctx().expect_equal(local,0,"fresh local state");
        local=3;
    }
    void test_fresh(){
        trace->events.push_back("body");
        ctx().expect_equal(local,0,"no cross-case leakage");
    }
    void test_throw(){
        trace->events.push_back("body");
        throw std::runtime_error("body failed");
    }
    void test_abort(){
        trace->events.push_back("body");
        ctx().require([]{return false;},"fatal prerequisite");
    }
};
struct SetupFail : ContextFixture {
    static inline int setup=0, body=0, teardown=0;
    using ContextFixture::ContextFixture;
    void set_up(){++setup;throw std::runtime_error("setup failed");}
    void tear_down(){++teardown;}
    void test_case(){++body;ctx().prove(false,"should not run");}
};
struct TeardownFail : ContextFixture {
    static inline int teardown=0;
    using ContextFixture::ContextFixture;
    void tear_down(){++teardown;throw std::runtime_error("teardown failed");}
    void test_success(){ctx().prove(true,"body success");}
    void test_exception(){throw std::runtime_error("body failed");}
    void test_abort(){ctx().require([]{return false;},"fatal before teardown");}
};
struct NoThrow : ContextFixture {
    static inline int teardown=0;
    using ContextFixture::ContextFixture;
    void tear_down(){++teardown;}
    void test_exception(){throw std::runtime_error("caught body error");}
};
struct Shared {int instances=0, setups=0, teardowns=0, uses=0;};
struct Variant : ContextFixture {
    std::shared_ptr<Shared> shared;
    int local=0;
    Variant(Context& c,std::shared_ptr<Shared> value):
        ContextFixture(c),shared(std::move(value)){++shared->instances;}
    void set_up(){++shared->setups;}
    void tear_down(){++shared->teardowns;}
    void test_first(){++shared->uses;ctx().expect_equal(local,0,"fresh");local=8;}
    void test_second(){++shared->uses;ctx().expect_equal(local,0,"fresh again");}
};
struct VariantFail : ContextFixture {
    static inline int teardown=0;
    using ContextFixture::ContextFixture;
    void tear_down(){++teardown;}
    void test_exception(){throw std::runtime_error("variant error");}
};

int main(){
    Trace trace; Fixture::trace=&trace;
    Suite suite("fixture lifecycle");
    ARIADNE_TESTING_ADD_TEST_TO(suite,Fixture,test_modify);
    ARIADNE_TESTING_ADD_TEST_TO(suite,Fixture,test_fresh);
    ARIADNE_TESTING_ADD_TEST_TO(suite,Fixture,test_throw);
    ARIADNE_TESTING_ADD_TEST_TO(suite,Fixture,test_abort);
    ARIADNE_TESTING_ADD_TEST_TO(suite,SetupFail,test_case);
    ARIADNE_TESTING_ADD_TEST_TO(suite,TeardownFail,test_success);
    ARIADNE_TESTING_ADD_TEST_TO(suite,TeardownFail,test_exception);
    ARIADNE_TESTING_ADD_TEST_TO(suite,TeardownFail,test_abort);
    ARIADNE_TESTING_ADD_TEST_NO_THROW_TO(suite,NoThrow,test_exception);
    auto shared=std::make_shared<Shared>();
    auto group=fixture_variant<Variant>(suite,"shared resource",
        [shared](Context& c){return Variant(c,shared);});
    ARIADNE_TESTING_ADD_VARIANT_TEST(group,test_first);
    ARIADNE_TESTING_ADD_VARIANT_TEST(group,test_second);
    auto failing=fixture_variant<VariantFail>(suite,"variant failure");
    ARIADNE_TESTING_ADD_VARIANT_TEST(failing,test_exception);
    Capture capture;
    auto const summary=suite.run(capture);
    Fixture::trace=nullptr;
    bool ok=true;
    auto verify=[&](bool condition,std::string const& msg){
        if(!condition){std::cerr<<msg<<'\n';ok=false;}
    };
    verify(summary.passed==4 && summary.failed==2 && summary.errors==6 &&
           summary.skipped==0 && summary.inconclusive==0 && capture.cases.size()==12,
           "lifecycle outcomes: pass="+std::to_string(summary.passed)+
           ", fail="+std::to_string(summary.failed)+
           ", error="+std::to_string(summary.errors));
    verify(trace.instances==4 && trace.events.size()==20,
           "fixture count or number of events");
    if(trace.events.size()==20){
        std::vector<std::string> const cycle{"construct","setup","body","teardown","destroy"};
        for(std::size_t i=0;i<trace.events.size();++i)
            verify(trace.events[i]==cycle[i%5],"lifecycle order: "+std::to_string(i));
    }
    verify(SetupFail::setup==1 && SetupFail::body==0 && SetupFail::teardown==0,
           "setup failure incorrectly entered method or teardown");
    verify(TeardownFail::teardown==3 && NoThrow::teardown==1 &&
           VariantFail::teardown==1,"teardown not consistently called");
    verify(shared->instances==2 && shared->setups==2 &&
           shared->teardowns==2 && shared->uses==2,"variant state was leaked or skipped");
    if(capture.cases.size()==12){
        const std::vector<Outcome> expected{
            Outcome::Passed,Outcome::Passed,Outcome::Error,Outcome::Failed,
            Outcome::Error,Outcome::Error,Outcome::Error,Outcome::Error,
            Outcome::Failed,Outcome::Passed,Outcome::Passed,Outcome::Error};
        for(std::size_t i=0;i<expected.size();++i)
            verify(capture.cases[i].outcome==expected[i],"case outcome "+std::to_string(i));
        auto const& dual=capture.cases[6].assertions;
        verify(!dual.empty() && dual.back().detail.find("body failed")!=std::string::npos &&
               dual.back().detail.find("teardown failed")!=std::string::npos,
               "dual body/teardown error details lost");
        auto const& fatal=capture.cases[3].assertions;
        verify(fatal.size()==1 && fatal[0].outcome==Outcome::Failed,
               "fatal prerequisite not preserved");
        auto const& fatal_teardown=capture.cases[7].assertions;
        verify(fatal_teardown.size()==2 && fatal_teardown[0].outcome==Outcome::Failed &&
               fatal_teardown[1].outcome==Outcome::Error &&
               fatal_teardown[1].detail.find("teardown failed")!=std::string::npos,
               "fatal and teardown failures not both retained");
        auto const& no_throw=capture.cases[8].assertions;
        verify(no_throw.size()==1 && no_throw[0].outcome==Outcome::Failed &&
               no_throw[0].description=="operation completes",
               "NoThrow policy changed");
    }
    return ok?0:1;
}
