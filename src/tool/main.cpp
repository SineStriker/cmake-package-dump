#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <stdexcept>

#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/str.h>
#include <stdcorelib/path.h>
#include <stdcorelib/experimental/process.h>

#include <syscmdline/parser.h>
#include <syscmdline/parseresult.h>

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

namespace tool {

    using stdc::console::debug;
    using stdc::console::success;
    using stdc::console::warning;
    using stdc::console::critical;

    using stdc::experimental::Process;

}

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
    tool::debug(cmdLine);
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
        ret = tool::Process::checkOptput(g_ctx.cmakePath, cmakeArgs, {}, output);
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
        ret = tool::Process::checkOptput(g_ctx.ninjaPath, ninjaArgs, {}, output);
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
        ret = tool::Process::start(g_ctx.cmakePath, cmakeArgs, g_ctx.dir, g_ctx.verbose ? "-" : "",
                                   g_ctx.verbose ? "-" : "");
    } catch (const std::exception &e) {
        throw std::runtime_error(stdc::formatN("execute cmake failed: %1", exception_message(e)));
    }
    if (ret != 0) {
        throw std::runtime_error(
            stdc::formatN("execute cmake failed: process exits with code %1", ret));
    }

    if (g_ctx.verbose) {
        tool::success("Run cmake configuration OK!");
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

    // analyze CMakeCache.txt
    fs::path build_dir = g_ctx.dir / _TSTR("build");
    bool is_msvc = false;
    {
        std::ifstream cache(build_dir / _TSTR("CMakeCache.txt"));
        std::string line;
        while (std::getline(cache, line)) {
            std::string_view line_view = line;
            if (stdc::starts_with(line_view, "CMAKE_CXX_COMPILER")) {
                auto eq_pos = line_view.find('=');
                if (eq_pos != std::string::npos) {
                    auto compiler = stdc::trim(line_view.substr(eq_pos + 1));
                    auto basename = fs::path(stdc::path::from_utf8(compiler)).stem();
                    if (stdc::to_lower(basename) == _TSTR("cl"))
                        is_msvc = true;
                }
                break;
            }
        }
    }

    struct NinjaTarget {
        // msvc: /D -D
        // gcc:  -D
        std::vector<std::string> defines;
        // gcc: -l
        std::vector<std::string> links;
        // msvc: -LIBPATH: /LIBPATH
        // gcc:  -L
        std::vector<std::string> linkdirs;
        // msvc: -I /I -external:I /external:I
        // gcc:  -I -isystem -idirafter
        std::vector<std::string> includes;
        std::vector<std::string> flags;
        std::vector<std::string> linkflags;
    };
    std::map<std::string, NinjaTarget> targets;

    // analyze build.ninja
    fs::path ninjaFilePath = g_ctx.dir / _TSTR("build") / _TSTR("build.ninja");
    {
        std::ifstream ninjaFile(ninjaFilePath);
        if (!ninjaFile.is_open()) {
            throw std::runtime_error(stdc::formatN("failed to open file: %1", ninjaFilePath));
        }

        struct NinjaBuild {
            std::string build_target;
            std::map<std::string, std::string> variables;
        };
        std::vector<NinjaBuild> ninja_builds;

        std::string line;
        std::string current_build;
        std::map<std::string, std::string> current_vars;

        while (std::getline(ninjaFile, line)) {
            std::string_view line_view = line;

            // https://ninja-build.org/manual.html#_build_statements
            // match build statement
            // e.g.
            //      build CMakeFiles/main.dir/main.cpp.obj: ...
            //      build main.exe: ...
            static const std::regex build_re(R"(^build\s+([^:]+):.+$)");
            std::smatch match;
            if (std::regex_search(line, match, build_re)) {
                if (!current_build.empty()) {
                    if (!current_vars.empty()) {
                        ninja_builds.push_back({current_build, current_vars});
                        current_vars.clear();
                    }
                    current_build.clear();
                }

                auto build_part = match[1].str();
                auto build_target = stdc::system::split_command_line(build_part)[0];
                auto stem = fs::path(stdc::path::from_utf8((build_target))).stem();
                if (stdc::starts_with(stem.native(), _TSTR("_AUX_LIB_"))) {
                    current_build = build_target;
                }
                continue;
            }
            if (!current_build.empty()) {
                static const std::regex var_re(R"(^\s+([\w_]+)\s*=\s*(.+)$)");
                if (std::regex_search(line, match, var_re) && match.size() >= 2) {
                    current_vars[match[1].str()] = match[2].str();
                } else {
                    if (!current_vars.empty()) {
                        ninja_builds.push_back({current_build, current_vars});
                        current_vars.clear();
                    }
                    current_build.clear();
                }
            }
        }
        if (!current_build.empty() && !current_vars.empty()) {
            ninja_builds.push_back({current_build, current_vars});
        }

        if (g_ctx.verbose) {
            tool::debug("Parse build.ninja:");
            for (const auto &build : ninja_builds) {
                stdc::u8println("build %1:", build.build_target);
                for (const auto &var : build.variables) {
                    stdc::u8println("  %1 = %2", var.first, var.second);
                }
                stdc::u8println();
            }
        }

        // combine arguments
        for (const auto &build : ninja_builds) {
            auto name = fs::path(stdc::path::from_utf8((build.build_target))).stem().string();
            auto dot_idx = name.find('.');
            if (dot_idx != std::string::npos) {
                name = name.substr(0, dot_idx);
            }

            auto &target = targets[name];
            for (const auto &var : build.variables) {
                auto &key = var.first;
                auto &value = var.second;
                if (key == "DEFINES") {
                    auto items = stdc::system::split_command_line(value);
                    for (const auto &item : items) {
                        if (is_msvc) {
                            if (stdc::starts_with(item, "-D") || stdc::starts_with(item, "/D")) {
                                target.defines.push_back(item.substr(2));
                            }
                        } else {
                            if (stdc::starts_with(item, "-D")) {
                                target.defines.push_back(item.substr(2));
                            }
                        }
                    }
                    continue;
                }

                if (key == "LINK_LIBRARIES") {
                    auto items = stdc::system::split_command_line(value);
                    for (const auto &item : items) {
                        if (is_msvc) {
                            target.links.push_back(item);
                        } else {
                            if (stdc::starts_with(item, "-l")) {
                                target.links.push_back(item.substr(2));
                            } else {
                                target.links.push_back(item);
                            }
                        }
                    }
                    continue;
                }

                if (key == "LINK_PATH") {
                    auto items = stdc::system::split_command_line(value);
                    for (const auto &item : items) {
                        if (is_msvc) {
                            auto item_lower = stdc::to_upper(item);
                            if (stdc::starts_with(item_lower, "-LIBPATH:") ||
                                stdc::starts_with(item_lower, "/LIBPATH:")) {
                                target.linkdirs.push_back(item.substr(9));
                            } else {
                                target.linkdirs.push_back(item);
                            }
                        } else {
                            if (stdc::starts_with(item, "-L")) {
                                target.linkdirs.push_back(item.substr(2));
                            } else {
                                target.linkdirs.push_back(item);
                            }
                        }
                    }
                    continue;
                }

                if (key == "INCLUDES") {
                    auto items = stdc::system::split_command_line(value);
                    bool has_include = false;
                    for (const auto &item : items) {
                        if (has_include) {
                            target.includes.push_back(item);
                            has_include = false;
                        } else if (is_msvc) {
                            if (item == "-I" || item == "/I" || item == "-external:I" ||
                                item == "/external:I") {
                                has_include = true;
                            } else if (stdc::starts_with(item, "-I") ||
                                       stdc::starts_with(item, "/I")) {
                                target.includes.push_back(item.substr(2));
                            } else if (stdc::starts_with(item, "-external:I") ||
                                       stdc::starts_with(item, "/external:I")) {
                                target.includes.push_back(item.substr(11));
                            }
                        } else {
                            if (item == "-isystem" || item == "-idirafter" || item == "-I") {
                                has_include = true;
                            } else if (stdc::starts_with(item, "-I")) {
                                target.includes.push_back(item.substr(2));
                            }
                        }
                    }
                    continue;
                }

                if (key == "FLAGS") {
                    auto items = stdc::system::split_command_line(value);
                    for (const auto &item : items) {
                        // ignore C/C++ standard
                        // if (is_msvc) {
                        //     if (stdc::starts_with(item, "/std:") ||
                        //         stdc::starts_with(item, "-std:")) {
                        //         continue;
                        //     }
                        // } else {
                        //     if (stdc::starts_with(item, "-std=")) {
                        //         continue;
                        //     }
                        // }
                        target.flags.push_back(item);
                    }
                    continue;
                }

                if (key == "LINK_FLAGS") {
                    auto items = stdc::system::split_command_line(value);
                    target.linkflags.insert(target.linkflags.end(), items.begin(), items.end());
                    continue;
                }
            }
        }
    }

    // print ninja targets
    if (g_ctx.verbose) {
        tool::debug("Auxiliary Targets:");
        for (const auto &target : targets) {
            stdc::u8println("TARGET %1:", target.first);
            const auto &t = target.second;
            if (!t.defines.empty()) {
                stdc::u8println("  DEFINES:");
                for (const auto &define : t.defines) {
                    stdc::u8println("    %1", define);
                }
            }
            if (!t.links.empty()) {
                stdc::u8println("  LINKS:");
                for (const auto &link : t.links) {
                    stdc::u8println("    %1", link);
                }
            }
            if (!t.linkdirs.empty()) {
                stdc::u8println("  LINK_DIRS:");
                for (const auto &linkdir : t.linkdirs) {
                    stdc::u8println("    %1", linkdir);
                }
            }
            if (!t.includes.empty()) {
                stdc::u8println("  INCLUDE_DIRS:");
                for (const auto &include : t.includes) {
                    stdc::u8println("    %1", include);
                }
            }
            if (!t.flags.empty()) {
                stdc::u8println("  FLAGS:");
                for (const auto &flag : t.flags) {
                    stdc::u8println("    %1", flag);
                }
            }
            if (!t.linkflags.empty()) {
                stdc::u8println("  LINK_FLAGS:");
                for (const auto &linkflag : t.linkflags) {
                    stdc::u8println("    %1", linkflag);
                }
            }
        }
    }

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
    rootCommand.addArguments({
        SCL::Argument("script", "Template CMake script"),
    });
    rootCommand.addVersionOption(TOOL_VERSION);
    rootCommand.addHelpOption(true);
    rootCommand.setHandler(cmd_handler);

    SCL::Parser parser(rootCommand);
    parser.setPrologue(TOOL_DESC);
    parser.setEpilogue(TOOL_COPYRIGHT);
    parser.setDisplayOptions(SCL::Parser::AlignAllCatalogues);

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
        tool::critical("Error: %1", msg);
        ret = -1;
    }
    return ret;
}