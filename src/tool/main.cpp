#include <iostream>
#include <sstream>
#include <regex>

#include <stdcorelib/system.h>
#include <stdcorelib/str.h>

#include <syscmdline/parser.h>

#include "os.h"

namespace SCL = SysCmdLine;

struct Options {
    bool verbose = false;
    std::string cmakePath = "cmake";
    std::string ninjaPath = "ninja";
};

static Options g_options;

// Check CMake version
static void check_cmake(const std::filesystem::path &programPath) {
    std::string output;
    int ret = os::CheckProcessOutput({stdc::to_string(programPath), "--version"}, output);
    if (ret != 0) {
        throw std::runtime_error("failed to execute cmake");
    }
    std::string line;
    if (std::getline(std::stringstream(output), line)) {
        static std::regex pattern(R"(cmake version (.+))");
        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            if (g_options.verbose) {
                std::cout << "cmake version: " << match[1] << std::endl;
            }
            return;
        }
    }
    throw std::runtime_error("failed to get cmake version");
}

// Check Ninja version
static void check_ninja(const std::filesystem::path &programPath) {
    std::string output;
    int ret = os::CheckProcessOutput({stdc::to_string(programPath), "--version"}, output);
    if (ret != 0) {
        throw std::runtime_error("failed to execute ninja");
    }
    std::string line;
    if (std::getline(std::stringstream(output), line)) {
        if (g_options.verbose) {
            std::cout << "ninja version: " << line << std::endl;
        }
        return;
    }
    throw std::runtime_error("failed to get ninja version");
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    auto args = stdc::system::command_line_arguments();
    g_options.verbose = true;

    check_cmake(g_options.cmakePath);
    check_ninja(g_options.ninjaPath);

    return 0;
}