#include "lexer.h"
#include "utils/exceptions.h"

bool isKeyWord(const std::string& input, size_t pos) {
    return input.substr(pos, 2) != "in" && input.substr(pos, 2) != "if" && input.substr(pos, 4) != "else" &&
           input.substr(pos, 6) != "elseif" && input.substr(pos, 4) != "then" && input.substr(pos, 4) != "true" &&
           input.substr(pos, 5) != "false";
}

std::vector<prism::lexer::Token> prism::lexer::Lexer::tokenize() {
    std::vector<prism::lexer::Token> tokens;
    bool inString = false;
    std::string currentString;

    while (pos < input.size()) {
        char current = input[pos];
        if (inString) {
            if (current == '"') {
                tokens.emplace_back(TokenType::Quote, currentString);
                currentString.clear();
                inString = false;
            } else {
                currentString += current;
            }
            pos++;
            continue;
        }
        if (std::isspace(current)) {
            pos++; // Skip whitespace
            continue;
        }

        if (isKeyWord(input, pos) && std::isalpha(current) || current == '_') {
            tokens.emplace_back(TokenType::Identifier, parseIdentifier());
            continue;
        }

        if (std::isdigit(current)) {
            bool hasDecimal = false;
            if (input.substr(pos + 1, 2) == "..") {
                auto result = parseNumber(nullptr);
                tokens.emplace_back(TokenType::Integer, result);
            } else {
                auto result = parseNumber(&hasDecimal);
                tokens.emplace_back(hasDecimal ? TokenType::Float : TokenType::Integer, result);
            }
            continue;
        }

        switch (current) {
            case '(':
                tokens.emplace_back(TokenType::LParen, "(");
                pos++;
                break;
            case ')':
                tokens.emplace_back(TokenType::RParen, ")");
                pos++;
                break;
            case ',':
                tokens.emplace_back(TokenType::Comma, ",");
                pos++;
                break;
            case '[':
                tokens.emplace_back(TokenType::LBracket, "[");
                pos++;
                break;
            case ']':
                tokens.emplace_back(TokenType::RBracket, "]");
                pos++;
                break;
            case '!':
                tokens.emplace_back(TokenType::Not, "!");
                pos++;
                break;
            case '"':
                pos++;
                inString = true;
                break;
            case '|':
                if (peek() == '|') {
                    tokens.emplace_back(TokenType::Or, "||");
                    pos += 2;
                }
                break;
            case '&':
                if (peek() == '&') {
                    tokens.emplace_back(TokenType::And, "&&");
                    pos += 2;
                }
                break;
            case '=':
                if (peek() == '=') {
                    tokens.emplace_back(TokenType::Equal, "==");
                    pos += 2;
                } else {
                    tokens.emplace_back(TokenType::Assign, "=");
                    pos++;
                }
                break;
            case '+':
                tokens.emplace_back(TokenType::Add, "+");
                pos++;
                break;
            case '-':
                tokens.emplace_back(TokenType::Sub, "-");
                pos++;
                break;
            case '*':
                tokens.emplace_back(TokenType::Mul, "*");
                pos++;
                break;
            case '/':
                tokens.emplace_back(TokenType::Div, "/");
                pos++;
                break;
            case 'i':
                if (peek() == 'n' && input.substr(pos, 7) != "include") {
                    tokens.emplace_back(TokenType::In, "in");
                    pos += 2;
                } else if (peek() == 'f') {
                    tokens.emplace_back(TokenType::If, "if");
                    pos += 2;
                } else {
                    tokens.emplace_back(TokenType::Identifier, parseIdentifier());
                }
                break;
            case 'e':
                if (peek() == 'l') {
                    if (input.substr(pos, 4) == "else") {
                        tokens.emplace_back(TokenType::Else, "else");
                        pos += 4;
                    } else if (input.substr(pos, 6) == "elseif") {
                        tokens.emplace_back(TokenType::Elseif, "elseif");
                        pos += 6;
                    } else {
                        tokens.emplace_back(TokenType::Identifier, parseIdentifier());
                    }
                } else {
                    tokens.emplace_back(TokenType::Identifier, parseIdentifier());
                }
                break;
            case 't':
                if (input.substr(pos, 4) == "then") {
                    tokens.emplace_back(TokenType::Then, "then");
                    pos += 4;
                } else if (input.substr(pos, 4) == "true") {
                    tokens.emplace_back(TokenType::True, "true");
                    pos += 4;
                } else {
                    tokens.emplace_back(TokenType::Identifier, parseIdentifier());
                }
                break;
            case 'f':
                if (input.substr(pos, 5) == "false") {
                    tokens.emplace_back(TokenType::False, "false");
                    pos += 5;
                } else {
                    tokens.emplace_back(TokenType::Identifier, parseIdentifier());
                }
                break;
            case '.':
                if (peek() == '.') {
                    tokens.emplace_back(TokenType::Range, "..");
                    pos += 2;
                } else {
                    throw prism::SyntaxError("Unexpected character " + std::string(1, current));
                }
                break;
            case ';':
                pos++;
                break;
            default:
                throw prism::SyntaxError("Unexpected character " + std::string(1, current));
        }
    }

    tokens.emplace_back(TokenType::EndOfinput);
    return tokens;
}

char prism::lexer::Lexer::peek() const {
    return (pos + 1 < input.size()) ? input[pos + 1] : '\0';
}

std::string prism::lexer::Lexer::parseIdentifier() {
    size_t start = pos;
    while (pos < input.size() && (std::isalnum(input[pos]) || input[pos] == '_')) {
        pos++;
    }
    return input.substr(start, pos - start);
}

std::string prism::lexer::Lexer::parseNumber(bool* hasDecimalPoint) {
    size_t start = pos;

    while (pos < input.size()) {
        char currentChar = input[pos];

        // If it's a digit, we continue parsing
        if (std::isdigit(currentChar)) {
            pos++;
        } else if (currentChar == '.' && hasDecimalPoint != nullptr) {
            if ((*hasDecimalPoint)) {
                break;
            }
            (*hasDecimalPoint) = true;
            pos++;
        } else {
            break;
        }
    }

    return input.substr(start, pos - start);
}
