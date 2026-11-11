/***************************************************************************
 *            test_directory_exhaustion.cpp
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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "testing/testing.hpp"
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

using Ariadne::Testing::CaseResult;
using Ariadne::Testing::Context;

int main() {
    // A single test executable starts with the per-process work-directory
    // counter at zero; the public API must reject an exhausted namespace.
#if defined(_WIN32)
    const auto pid = std::to_string(_getpid());
#else
    const auto pid = std::to_string(getpid());
#endif
    const auto root = std::filesystem::temp_directory_path();
    struct CreatedDirectories {
        std::vector<std::filesystem::path> paths;
        ~CreatedDirectories() {
            for (auto it = paths.rbegin(); it != paths.rend(); ++it) {
                std::error_code error;
                std::filesystem::remove(*it, error);
            }
        }
    } created;
    for (unsigned i = 0; i != 1000; ++i) {
        const auto candidate = root / ("ariadne-case-" + pid + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) {
            created.paths.push_back(candidate);
        } else if (error) {
            std::cerr << "Could not arrange occupied namespace: " << error.message() << '\n';
            return 1;
        }
    }
    CaseResult result;
    Context context(result);
    bool rejected = false;
    try {
        (void)context.work_directory();
    } catch (const std::runtime_error& e) {
        rejected = std::string(e.what()) == "Cannot allocate isolated test directory";
    }
    if (!rejected) {
        std::cerr << "Exhausted case directory namespace was not rejected\n";
        return 2;
    }
    // Exhaustion is confined to one allocation attempt, not a permanent
    // poison of the process-wide allocator. The next Context must recover.
    std::filesystem::path recovered;
    {
        CaseResult later_result;
        Context later(later_result);
        recovered = later.work_directory();
        if (!std::filesystem::is_directory(recovered)) {
            std::cerr << "A later Context could not recover after exhaustion\n";
            return 3;
        }
    }
    if (std::filesystem::exists(recovered)) {
        std::cerr << "A recovered Context left an owned directory behind\n";
        return 4;
    }
    return 0;
}
