#include "ariadne-testing.hpp"
#include <csignal>
#include <string>
#include <iostream>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#endif

using namespace Ariadne::Testing;

int main() {
#ifndef _WIN32
    int fds[2];
    if (pipe(fds) != 0) return 1;
    pid_t child = fork();
    if (child < 0) return 2;
    if (child == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDERR_FILENO) < 0) _exit(3);
        close(fds[1]);
        Suite suite("crash probe");
        suite.add("before failure", [](Context& c) {
            c.prove(true,"first check");
        });
        suite.add("fatal case", [](Context& c) {
            c.prove(true,"finished check");
            c.check([] {
                std::raise(SIGABRT);
                return true;
            }, "fatal check");
        });
        char const* const args[] = {"test_crash_diagnostics","--concise"};
        (void)run_suite(suite, 2, args);
        _exit(4);
    }
    close(fds[1]);
    std::string output;
    char buffer[1024];
    for (;;) {
        ssize_t n = read(fds[0], buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        output.append(buffer, static_cast<std::size_t>(n));
    }
    close(fds[0]);
    int status = 0;
    if (waitpid(child, &status, 0) != child) return 5;
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
        std::cerr << "Child did not terminate by SIGABRT: status=" << status << '\n';
        return 6;
    }
    if (output != "[CRASH CONTEXT]\n"
                  "  suite: crash probe\n"
                  "  case: fatal case\n"
                  "  position: 2/2\n"
                  "  last check: [2.2] fatal check\n" ||
        output.find("before failure") != std::string::npos ||
        output.find("[CHECK ") != std::string::npos) {
        std::cerr << "Missing concise crash breadcrumb, or noisy progress: " << output << '\n';
        return 7;
    }
#endif
    return 0;
}
