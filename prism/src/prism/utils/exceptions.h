#pragma once

#include <exception>
#include <utility>

namespace prism {
class SyntaxError : public std::exception {
  public:
    SyntaxError(std::string message) : m_message(std::move(message)) {
    }
    [[nodiscard]] const char* what() const noexcept override {
        return m_message.c_str();
    }

  private:
    std::string m_message;
};

class RuntimeError : public std::exception {
  public:
    RuntimeError(const char* message) : m_message(message) {
    }
    const char* what() const noexcept override {
        return m_message;
    }

  private:
    const char* m_message;
};
} // namespace prism