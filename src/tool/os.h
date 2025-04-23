#ifndef OS_H
#define OS_H

#include <vector>
#include <string>

namespace os {

    int ExecuteProcess(const std::vector<std::string> &args, const std::string &strout = {},
                       const std::string &strerr = {});

    int CheckProcessOutput(const std::vector<std::string> &args, std::string &output);

}

#endif // OS_H