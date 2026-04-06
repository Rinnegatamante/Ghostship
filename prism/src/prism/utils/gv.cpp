#include "gv.h"

#include <regex>
#include <sstream>
#include "exceptions.h"

std::vector<std::string> prism::gv::split(const std::string line, const char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

std::vector<std::string> prism::gv::parenthesis(const std::string line) {
    std::vector<std::string> result;
    std::regex re(R"(\(([^)]+)\))");
    std::sregex_iterator next(line.begin(), line.end(), re);
    std::sregex_iterator end;
    while (next != end) {
        std::smatch match = *next;
        result.push_back(match[1].str());
        next++;
    }
    return result;
}

std::vector<std::string> prism::gv::fn_args(const std::string line) {
    std::vector<std::string> result;
    std::regex re(R"(\s*,\s*)");
    std::sregex_token_iterator first{ line.begin(), line.end(), re, -1 }, last;

    std::copy_if(first, last, std::back_inserter(result), [](const std::string& s) { return !s.empty(); });
    return result;
}

std::unordered_map<std::string, std::optional<std::string>> prism::gv::il_args(const std::string line) {
    std::unordered_map<std::string, std::optional<std::string>> result;
    auto args = fn_args(line);
    for (const auto& arg : args) {
        auto parts = split(arg, '=');
        if (!parts.empty()) {
            result[parts[0]] = parts[1];
        }
    }
    return result;
}