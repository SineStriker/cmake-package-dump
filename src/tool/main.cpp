#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <filesystem>

#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/str.h>
#include <stdcorelib/path.h>

#include <syscmdline/parser.h>
#include <syscmdline/parseresult.h>

#include "os.h"
#include "resources.h"

namespace SCL = SysCmdLine;

namespace fs = std::filesystem;

struct GlobalContext {
    fs::path cwd;

    bool verbose = false;
    fs::path cmakePath = _TSTR("cmake");
    fs::path ninjaPath = _TSTR("ninja");

    fs::path dir;
    fs::path output;

    fs::path script;

    std::vector<std::string> extraArgs;
};

static GlobalContext g_ctx;

static inline std::string exception_message(const std::exception &e) {
    std::string msg = e.what();
#ifdef _WIN32
    if (typeid(e) == typeid(fs::filesystem_error)) {
        auto &err = static_cast<const fs::filesystem_error &>(e);
        msg = stdc::wstring_conv::to_utf8(stdc::wstring_conv::from_ansi(err.what()));
    }
#endif
    return msg;
}

static inline void report_subprocess_args(const fs::path &command,
                                          const std::vector<std::string> &args) {
    std::string cmdLine = stdc::system::join_command_line({stdc::to_string(command)});
    if (!args.empty()) {
        cmdLine += " " + stdc::system::join_command_line(args);
    }
    stdc::console::println(stdc::console::nostyle, stdc::console::blue | stdc::console::intensified,
                           stdc::console::nocolor, cmdLine);
}

static void check_cmake() {
    int ret;
    std::string output;

    // execute: cmake --version
    try {
        std::vector<std::string> cmakeArgs = {
            "--version",
        };
        if (g_ctx.verbose) {
            report_subprocess_args(g_ctx.cmakePath, cmakeArgs);
        }
        ret = os::CheckProcessOutput(g_ctx.cmakePath, cmakeArgs, {}, output);
    } catch (const std::exception &e) {
        throw std::runtime_error(stdc::formatN("check cmake failed: %1", exception_message(e)));
    }
    if (ret != 0) {
        throw std::runtime_error(
            stdc::formatN("check cmake failed: process exits with code %1", ret));
    }

    // expected output:
    // ```
    // cmake version X.X.X
    //
    // CMake suite maintained and supported by Kitware (kitware.com/cmake).
    // ```
    std::string line;
    if (std::getline(std::stringstream(output), line)) {
        static std::regex pattern(R"(cmake version (.+))");
        std::smatch match;
        if (std::regex_search(line, match, pattern)) {
            if (g_ctx.verbose) {
                stdc::u8print("cmake version: %1\n", match[1].str());
            }
            return;
        }
    }
    throw std::runtime_error("check cmake failed: failed to get version");
}

static void check_ninja() {
    int ret;
    std::string output;

    // execute: ninja --version
    try {
        std::vector<std::string> ninjaArgs = {
            "--version",
        };
        if (g_ctx.verbose) {
            report_subprocess_args(g_ctx.ninjaPath, ninjaArgs);
        }
        ret = os::CheckProcessOutput(g_ctx.ninjaPath, ninjaArgs, {}, output);
    } catch (const std::exception &e) {
        throw std::runtime_error(stdc::formatN("check ninja failed: %1", exception_message(e)));
    }
    if (ret != 0) {
        throw std::runtime_error(
            stdc::formatN("check ninja failed: process exits with code %1", ret));
    }

    // expected output:
    // ```
    // X.X.X
    // ```
    std::string line;
    if (std::getline(std::stringstream(output), line)) {
        if (g_ctx.verbose) {
            stdc::u8print("ninja version: %1\n", line);
        }
        return;
    }
    throw std::runtime_error("check ninja failed: failed to get version");
}

static void run_cmake_configure() {
    int ret;
    try {
        std::vector<std::string> cmakeArgs = {
            "-S",
            ".",
            "-B",
            "build",
            "-G",
            "Ninja",
            "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + stdc::to_string(g_ctx.ninjaPath),
            "-DXMAKE_FIND_SCRIPT:FILEPATH=" + stdc::to_string(g_ctx.script),
        };
        cmakeArgs.insert(cmakeArgs.end(), g_ctx.extraArgs.begin(), g_ctx.extraArgs.end());
        if (g_ctx.verbose) {
            report_subprocess_args(g_ctx.cmakePath, cmakeArgs);
        }
        ret = os::ExecuteProcess(g_ctx.cmakePath, cmakeArgs, g_ctx.dir, g_ctx.verbose ? "-" : "",
                                 g_ctx.verbose ? "-" : "");
    } catch (const std::exception &e) {
        throw std::runtime_error(stdc::formatN("execute cmake failed: %1", exception_message(e)));
    }
    if (ret != 0) {
        throw std::runtime_error(
            stdc::formatN("execute cmake failed: process exits with code %1", ret));
    }
}

static int cmd_handler(const SCL::ParseResult &result) {
    if (result.isRoleSet(SCL::Option::Verbose)) {
        g_ctx.verbose = true;
    }

    {
        auto cmakePath = result.valueForOption("--cmake").toString();
        auto ninjaPath = result.valueForOption("--ninja").toString();

        auto extraArgs = result.option("--").values();
        auto output = result.valueForOption("-o").toString();
        auto dir = result.valueForOption("--dir").toString();
        auto script = result.value("script").toString();

        if (!cmakePath.empty()) {
            g_ctx.cmakePath = stdc::path::from_utf8(cmakePath);
        }
        if (!ninjaPath.empty()) {
            g_ctx.ninjaPath = stdc::path::from_utf8(ninjaPath);
        }
        g_ctx.dir =
            dir.empty() ? g_ctx.cwd / _TSTR("build") : fs::absolute(stdc::path::from_utf8(dir));
        if (!output.empty()) {
            g_ctx.output = stdc::path::from_utf8(output);
        }

        g_ctx.script = fs::absolute(stdc::path::from_utf8(script));

        if (!extraArgs.empty()) {
            g_ctx.extraArgs.reserve(extraArgs.size());
            for (const auto &arg : extraArgs) {
                g_ctx.extraArgs.push_back(arg.toString());
            }
        }
    }

    // initialize
    g_ctx.cwd = fs::current_path();

    // check tools
    check_cmake();
    check_ninja();

    // prepare temporary path
    if (fs::exists(g_ctx.dir)) {
        fs::remove_all(g_ctx.dir);
    }
    fs::create_directory(g_ctx.dir);

    // check script file
    if (!fs::exists(g_ctx.script)) {
        throw std::runtime_error(stdc::formatN("failed to read file: %1", g_ctx.script));
    }

    // create CMakeLists.txt
    fs::path cmakeListsPath = g_ctx.dir / _TSTR("CMakeLists.txt");
    {
        std::ofstream cmakeListsFile(cmakeListsPath,
                                     std::ios::out | std::ios::trunc | std::ios::binary);
        if (!cmakeListsFile.is_open()) {
            throw std::runtime_error(stdc::formatN("failed to open file: %1", cmakeListsPath));
        }
        cmakeListsFile.write((const char *) CMakeLists_txt_data.data, CMakeLists_txt_data.size);
    }

    // create TestTargets.cmake
    fs::path testTargetsCMakePath = g_ctx.dir / _TSTR("TestTargets.cmake");
    {
        std::ofstream testTargetsCMakeFile(testTargetsCMakePath,
                                           std::ios::out | std::ios::trunc | std::ios::binary);
        if (!testTargetsCMakeFile.is_open()) {
            throw std::runtime_error(
                stdc::formatN("failed to open file: %1", testTargetsCMakePath));
        }
        testTargetsCMakeFile.write((const char *) TestTargets_cmake_data.data,
                                   TestTargets_cmake_data.size);
    }

    // execute CMake
    run_cmake_configure();

    return 0;
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    SCL::Command rootCommand(stdc::system::application_name(), "Dump CMake package specification.");
    rootCommand.addOptions({
        SCL::Option({"--cmake"}, "Path to CMake executable").arg("path"),
        SCL::Option({"--ninja"}, "Path to Ninja executable").arg("path"),
        SCL::Option({"--dir"}, "Path to the temporary directory for CMake configuration")
            .arg("path"),
        SCL::Option({"-o"}, "Output file path").arg("path"),
    });
    rootCommand.addOption(SCL::Option::Verbose);
    rootCommand.addOption(SCL::Option({"--"}, "Extra CMake arguments")
                              .arg(SCL::Argument("args").nargs(SCL::Argument::Remainder)));
    rootCommand.addHelpOption(true);
    rootCommand.addArguments({
        SCL::Argument("script", "Template CMake script"),
    });
    rootCommand.addVersionOption(TOOL_VERSION);
    rootCommand.setHandler(cmd_handler);

    SCL::Parser parser(rootCommand);
    parser.setPrologue(TOOL_DESC);
    parser.setEpilogue(TOOL_COPYRIGHT);

    int ret;
    try {
#ifdef _WIN32
        std::ignore = argc;
        std::ignore = argv;
        ret = parser.invoke(stdc::system::command_line_arguments());
#else
        ret = parser.invoke(argc, argv);
#endif
    } catch (const std::exception &e) {
        std::string msg = exception_message(e);
        stdc::console::printf(stdc::console::nostyle, stdc::console::lightred,
                              stdc::console::nocolor, "Error: %s\n", msg.data());
        ret = -1;
    }
    return ret;
}