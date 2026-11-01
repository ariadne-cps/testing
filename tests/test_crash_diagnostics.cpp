#include "ariadne-testing.hpp"
#include <csignal>
#include <string>
#include <vector>
#include <iostream>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#endif

using namespace Ariadne::Testing;

int main() {
#ifndef _WIN32
    // Each handled POSIX signal must preserve the last announced CHECK and
    // re-raise the original signal for the operating system to terminate.
    // Validate from the parent: a crashing child cannot flush LLVM profiles.
    std::vector<int> const signals{
        SIGABRT, SIGSEGV, SIGILL, SIGFPE,
#ifdef SIGBUS
        SIGBUS
#endif
    };
    for (int const signal_number : signals) {
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
            suite.add("fatal case", [signal_number](Context& c) {
                c.prove(true,"finished check");
                c.check([signal_number] {
                    std::raise(signal_number);
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
        if (!WIFSIGNALED(status) || WTERMSIG(status) != signal_number) {
            std::cerr << "Child did not terminate by signal " << signal_number << ": status=" << status << '\n';
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
    }
#endif
    return 0;
}
