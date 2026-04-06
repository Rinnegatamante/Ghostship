#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <optional>
#include <algorithm>
#include <unordered_map>
#include "exceptions.h"

namespace prism::gv {
std::vector<std::string> split(const std::string line, char delimiter);
std::vector<std::string> parenthesis(const std::string line);
std::vector<std::string> fn_args(const std::string line);
std::unordered_map<std::string, std::optional<std::string>> il_args(const std::string line);
inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}
inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
            s.end());
}
inline std::string trim(std::string cpy) {
    ltrim(cpy);
    rtrim(cpy);
    return cpy;
}
inline std::vector<std::string> new_line_split(std::string str) {
    auto result = std::vector<std::string>{};
    auto ss = std::stringstream{ str };

    for (std::string line; std::getline(ss, line, '\n');)
        result.push_back(line);

    return result;
}
template <typename T> T get(const std::string& arg) {
    // String
    if (arg[0] == '\'') {
        if (arg[arg.size() - 1] != '\'') {
            throw prism::SyntaxError("Invalid string");
        }

        return arg.substr(1, arg.size() - 2);
    }

    // Integer
    if (arg.find_first_not_of("0123456789") == std::string::npos) {
        return std::stoi(arg);
    }

    // Float
    if (arg.find_first_not_of("0123456789.") == std::string::npos) {
        return std::stof(arg);
    }

    // Boolean
    if (arg == "true" || arg == "false") {
        return arg == "true";
    }

    throw prism::SyntaxError("Unknown data type");
}
} // namespace prism::gv
