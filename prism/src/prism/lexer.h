#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace prism::lexer {
enum class TokenType {
    Identifier,  // Variable names
    Integer,     // Number
    Float,       // Float
    LParen,      // '('
    RParen,      // ')'
    LBracket,    // '['
    RBracket,    // ']'
    Comma,       // ','
    Or,          // '||'
    And,         // '&&'
    Equal,       // '=='
    Assign,      // '='
    Not,         // '!'
    Add,         // '+'
    Sub,         // '-'
    Mul,         // '*'
    Div,         // '/'
    In,          // in
    Quote,       // '"'
    If,          // if
    Then,        // then
    Else,        // else
    Elseif,      // elseif
    Range,       // ..
    True,        // true
    False,       // false
    EndOfinput // End of script
};

struct Token {
    TokenType type;
    std::string value;

    Token(TokenType type, std::string value = "") : type(type), value(std::move(value)) {
    }
};

class Lexer {
  private:
    std::string input;
    size_t pos = 0;

  public:
    explicit Lexer(std::string text) : input(text) {
    }
    std::vector<Token> tokenize();
    size_t length() {
        return pos;
    }

  private:
    [[nodiscard]] char peek() const;
    std::string parseIdentifier();
    std::string parseNumber(bool* hasDecimalPoint);
};
} // namespace prism::lexer