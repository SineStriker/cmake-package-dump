#include <iostream>
#include <sstream>
#include <regex>
#include <filesystem>

#include <stdcorelib/system.h>
#include <stdcorelib/console.h>
#include <stdcorelib/str.h>

#include <syscmdline/parser.h>
#include <syscmdline/parseresult.h>

#include "os.h"

namespace SCL = SysCmdLine;

namespace fs = std::filesystem;

struct GlobalContext {
    bool verbose = false;
    std::string cmakePath = "cmake";
    std::string ninjaPath = "ninja";
};

static GlobalContext g_ctx;

static const char *g_CMAKE_VARIABLES_TO_CLEAR[] = {
    // default libraries
    "CMAKE_C_STANDARD_LIBRARIES", "CMAKE_CXX_STANDARD_LIBRARIES",

    // implicit directories
    "CMAKE_C_IMPLICIT_LINK_LIBRARIES", "CMAKE_C_IMPLICIT_LINK_DIRECTORIES",
    "CMAKE_C_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES", "CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES",
    "CMAKE_CXX_IMPLICIT_LINK_LIBRARIES", "CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES",
    "CMAKE_CXX_IMPLICIT_LINK_FRAMEWORK_DIRECTORIES", "CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES",

    // c flags
    "CMAKE_C_FLAGS", "CMAKE_C_FLAGS_DEBUG", "CMAKE_C_FLAGS_RELEASE", "CMAKE_C_FLAGS_MINSIZEREL",
    "CMAKE_C_FLAGS_RELWITHDEBINFO", "CMAKE_C_FLAGS_INIT", "CMAKE_C_FLAGS_DEBUG_INIT",
    "CMAKE_C_FLAGS_RELEASE_INIT", "CMAKE_C_FLAGS_MINSIZEREL_INIT",
    "CMAKE_C_FLAGS_RELWITHDEBINFO_INIT", "CMAKE_C_FLAGS",

    // c++ flags
    "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_DEBUG", "CMAKE_CXX_FLAGS_RELEASE",
    "CMAKE_CXX_FLAGS_MINSIZEREL", "CMAKE_CXX_FLAGS_RELWITHDEBINFO", "CMAKE_CXX_FLAGS_INIT",
    "CMAKE_CXX_FLAGS_DEBUG_INIT", "CMAKE_CXX_FLAGS_RELEASE_INIT", "CMAKE_CXX_FLAGS_MINSIZEREL_INIT",
    "CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT",

    // linker flags
    "CMAKE_EXE_LINKER_FLAGS", "CMAKE_EXE_LINKER_FLAGS_DEBUG", "CMAKE_EXE_LINKER_FLAGS_RELEASE",
    "CMAKE_EXE_LINKER_FLAGS_MINSIZEREL", "CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO",
    "CMAKE_EXE_LINKER_FLAGS_INIT", "CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT",
    "CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT", "CMAKE_EXE_LINKER_FLAGS_MINSIZEREL_INIT",
    "CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT", "CMAKE_SHARED_LINKER_FLAGS",
    "CMAKE_SHARED_LINKER_FLAGS_DEBUG", "CMAKE_SHARED_LINKER_FLAGS_RELEASE",
    "CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL", "CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO",
    "CMAKE_SHARED_LINKER_FLAGS_INIT", "CMAKE_SHARED_LINKER_FLAGS_DEBUG_INIT",
    "CMAKE_SHARED_LINKER_FLAGS_RELEASE_INIT", "CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL_INIT",
    "CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO_INIT", "CMAKE_STATIC_LINKER_FLAGS",
    "CMAKE_STATIC_LINKER_FLAGS_DEBUG", "CMAKE_STATIC_LINKER_FLAGS_RELEASE",
    "CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL", "CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO",
    "CMAKE_STATIC_LINKER_FLAGS_INIT", "CMAKE_STATIC_LINKER_FLAGS_DEBUG_INIT",
    "CMAKE_STATIC_LINKER_FLAGS_RELEASE_INIT", "CMAKE_STATIC_LINKER_FLAGS_MINSIZEREL_INIT",
    "CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO_INIT", "CMAKE_MODULE_LINKER_FLAGS",
    "CMAKE_MODULE_LINKER_FLAGS_DEBUG", "CMAKE_MODULE_LINKER_FLAGS_RELEASE",
    "CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL", "CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO",
    "CMAKE_MODULE_LINKER_FLAGS_INIT", "CMAKE_MODULE_LINKER_FLAGS_DEBUG_INIT",
    "CMAKE_MODULE_LINKER_FLAGS_RELEASE_INIT", "CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL_INIT",
    "CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO_INIT",

    // msvc
    "CMAKE_MSVC_RUNTIME_LIBRARY",

    // windows
    // https://github.com/Kitware/CMake/blob/e66e0b2cfefaf61fa995a0aa117df31e680b1c7e/Source/cmLocalGenerator.cxx#L1604
    // https://github.com/Kitware/CMake/blob/e66e0b2cfefaf61fa995a0aa117df31e680b1c7e/Modules/Platform/Windows-MSVC.cmake#L404
    "CMAKE_CXX_CREATE_WIN32_EXE", "CMAKE_CXX_CREATE_CONSOLE_EXE"};

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

static void check_cmake(const std::filesystem::path &programPath) {
    int ret;
    std::string output;

    // execute: cmake --version
    try {
        ret = os::CheckProcessOutput({stdc::to_string(programPath), "--version"}, output);
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
                std::cout << "cmake version: " << match[1] << std::endl;
            }
            return;
        }
    }
    throw std::runtime_error("check cmake failed: failed to get version");
}

static void check_ninja(const std::filesystem::path &programPath) {
    int ret;
    std::string output;

    // execute: ninja --version
    try {
        ret = os::CheckProcessOutput({stdc::to_string(programPath), "--version"}, output);
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
            std::cout << "ninja version: " << line << std::endl;
        }
        return;
    }
    throw std::runtime_error("check ninja failed: failed to get version");
}

static int cmd_handler(const SCL::ParseResult &result) {
    if (result.isRoleSet(SCL::Option::Verbose)) {
        g_ctx.verbose = true;
    }

    auto cmakePath = result.valueForOption("--cmake").toString();
    if (!cmakePath.empty()) {
        g_ctx.cmakePath = cmakePath;
    }

    auto ninjaPath = result.valueForOption("--ninja").toString();
    if (!ninjaPath.empty()) {
        g_ctx.ninjaPath = ninjaPath;
    }

    auto extraArgs = result.option("--").values();
    auto output = result.valueForOption("-o").toString();
    auto dir = result.valueForOption("--dir").toString();
    auto script = result.value("script").toString();

    // check tools
    check_cmake(g_ctx.cmakePath);
    check_ninja(g_ctx.ninjaPath);

    // create temporary directory
    // if (!fs::is_directory(dir)) {
    //     fs::create_directory(dir);
    // }

    // create CMakeLists.txt



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