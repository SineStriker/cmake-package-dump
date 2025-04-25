#ifndef OS_H
#define OS_H

#include <vector>
#include <string>
#include <filesystem>

namespace os {

    int ExecuteProcess(const std::filesystem::path &command, const std::vector<std::string> &args,
                       const std::filesystem::path &cwd, const std::string &strout = {},
                       const std::string &strerr = {});

    int CheckProcessOutput(const std::filesystem::path &command,
                           const std::vector<std::string> &args, const std::filesystem::path &cwd,
                           std::string &output);

}

#endif // OS_H