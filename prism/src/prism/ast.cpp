#include "ast.h"
#include "utils/exceptions.h"

#define is_type(var, type) std::holds_alternative<type>((var))

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parse() {
    return parseAssign();
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseAssign() {
    auto node = parseOr();
    while (match(lexer::TokenType::Assign)) {
        auto var = std::get<VariableNode>(node->node);
        auto right = parseOr();
        auto parent = std::make_shared<prism::ast::ASTNode>(AssignNode{ var, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseOr() {
    auto node = parseAnd();
    while (match(lexer::TokenType::Or)) {
        auto right = parseAnd();
        auto parent = std::make_shared<prism::ast::ASTNode>(OrNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseAnd() {
    auto node = parseEqual();
    while (match(lexer::TokenType::And)) {
        auto right = parseEqual();
        auto parent = std::make_shared<prism::ast::ASTNode>(AndNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseEqual() {
    auto node = parseMul();
    while (match(lexer::TokenType::Equal)) {
        auto right = parseMul();
        auto parent = std::make_shared<prism::ast::ASTNode>(EqualNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseMul() {
    auto node = parseDiv();
    while (match(lexer::TokenType::Mul)) {
        auto right = parseDiv();
        auto parent = std::make_shared<prism::ast::ASTNode>(MulNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseDiv() {
    auto node = parseAdd();
    while (match(lexer::TokenType::Div)) {
        auto right = parseAdd();
        auto parent = std::make_shared<prism::ast::ASTNode>(DivNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseAdd() {
    auto node = parseSub();
    while (match(lexer::TokenType::Add)) {
        auto right = parseSub();
        auto parent = std::make_shared<prism::ast::ASTNode>(AddNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseSub() {
    auto node = parseIn();
    while (match(lexer::TokenType::Sub)) {
        auto right = parseIn();
        auto parent = std::make_shared<prism::ast::ASTNode>(SubNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseIn() {
    auto node = parseRange();
    while (match(lexer::TokenType::In)) {
        auto right = parseRange();
        auto parent = std::make_shared<prism::ast::ASTNode>(InNode{ node, right });
        node = parent;
    }
    return node;
}

std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parseRange() {
    auto node = parsePrimary();
    while (match(lexer::TokenType::Range)) {
        auto right = parsePrimary();
        auto parent = std::make_shared<prism::ast::ASTNode>(RangeNode{ node, right });
        node = parent;
    }
    return node;
}
// Parses parentheses or variables
std::shared_ptr<prism::ast::ASTNode> prism::ast::Parser::parsePrimary() {
    if (match(lexer::TokenType::LParen)) {
        auto node = parse();
        expect(lexer::TokenType::RParen); // Ensure closing ')'
        return node;
    }

    if (match(lexer::TokenType::Integer)) {
        return std::make_shared<ASTNode>(IntegerNode{ std::stoi(previous().value) });
    }

    if (match(lexer::TokenType::Float)) {
        return std::make_shared<ASTNode>(FloatNode{ std::stof(previous().value) });
    }

    if (match(lexer::TokenType::True)) {
        return std::make_shared<ASTNode>(IntegerNode{ 1 });
    }

    if (match(lexer::TokenType::False)) {
        return std::make_shared<ASTNode>(IntegerNode{ 0 });
    }

    if (match(lexer::TokenType::If)) {
        auto condition = parse();
        expect(lexer::TokenType::Then); // Ensure 'then' keyword
        auto body = parse();
        std::shared_ptr<std::vector<std::shared_ptr<ElseIfNode>>> elseIfs =
            std::make_shared<std::vector<std::shared_ptr<ElseIfNode>>>();
        while (match(lexer::TokenType::Elseif)) {
            auto elseIfCondition = parse();
            expect(lexer::TokenType::Then); // Ensure 'then' keyword
            auto elseIfBody = parse();
            elseIfs->push_back(std::make_shared<ElseIfNode>(ElseIfNode{ elseIfCondition, elseIfBody }));
        }

        if (match(lexer::TokenType::Else)) {
            auto elseBody = parse();
            return std::make_shared<ASTNode>(IfNode{ condition, body, elseIfs, elseBody });
        }

        throw std::runtime_error("Unexpected token");
    }

    if (match(lexer::TokenType::Quote)) {
        return std::make_shared<ASTNode>(QuoteNode{ previous().value });
    }

    if (match(lexer::TokenType::Not)) {
        return std::make_shared<ASTNode>(NotNode{ parsePrimary() });
    }

    if (match(lexer::TokenType::Identifier)) {
        std::string varName = previous().value;
        std::shared_ptr<VariableNode> variable = std::make_shared<VariableNode>(VariableNode{ varName });

        if (match(lexer::TokenType::LBracket)) {
            auto arrayIndices = std::make_shared<std::vector<std::shared_ptr<ASTNode>>>();
            // Loop to handle multiple array accesses (e.g., var[0][1][2])
            do {
                auto indexNode = parsePrimary();    // Parse the index (e.g., 0, 1)
                expect(lexer::TokenType::RBracket); // Expect closing bracket

                arrayIndices->push_back(indexNode);
            } while (match(lexer::TokenType::LBracket));

            return std::make_shared<ASTNode>(ArrayAccessNode{ variable, arrayIndices });
        }

        if (match(lexer::TokenType::LParen)) {
            auto args = std::make_shared<std::vector<std::shared_ptr<ASTNode>>>();
            do {
                if (isNext(lexer::TokenType::RParen)) {
                    break;
                }
                auto arg = parse();
                args->push_back(arg);
            } while (match(lexer::TokenType::Comma));
            expect(lexer::TokenType::RParen);
            return std::make_shared<ASTNode>(FunctionCallNode{ variable, args });
        }
        return std::make_shared<ASTNode>(*variable);
    }

    throw std::runtime_error("Unexpected token");
}

// Utility functions
bool prism::ast::Parser::match(lexer::TokenType type) {
    if (pos < tokens.size() && tokens[pos].type == type) {
        pos++;
        return true;
    }
    return false;
}

prism::lexer::Token prism::ast::Parser::expect(lexer::TokenType type) {
    if (pos < tokens.size() && tokens[pos].type == type) {
        return tokens[pos++];
    }

    throw prism::SyntaxError("Unexpected token");
}

bool prism::ast::Parser::isNext(lexer::TokenType type) {
    return pos < tokens.size() && tokens[pos].type == type;
}

prism::lexer::Token prism::ast::Parser::previous() {
    return tokens[pos - 1];
}

void prism::ast::print_ast_node(std::shared_ptr<prism::ast::ASTNode> node, int depth) {
    std::string indent = std::string(depth, ' ');
    if (is_type(node->node, VariableNode)) {
        std::cout << indent << "Variable: " << std::get<VariableNode>(node->node).name << std::endl;
        return;
    } else if (is_type(node->node, IntegerNode)) {
        std::cout << indent << "Integer: " << std::get<IntegerNode>(node->node).value << std::endl;
        return;
    } else if (is_type(node->node, FloatNode)) {
        std::cout << indent << "Float: " << std::get<FloatNode>(node->node).value << std::endl;
        return;
    } else if (is_type(node->node, ArrayAccessNode)) {
        std::cout << indent << "Array Access: " << std::get<ArrayAccessNode>(node->node).name->name << std::endl;
        for (const auto& index : *std::get<ArrayAccessNode>(node->node).arrayIndices) {
            print_ast_node(index, depth + 1);
        }
        return;
    } else if (is_type(node->node, NotNode)) {
        std::cout << indent << "NOT" << std::endl;
        print_ast_node(std::get<NotNode>(node->node).node, depth + 1);
        return;
    } else if (is_type(node->node, EqualNode)) {
        auto equalNode = std::get<EqualNode>(node->node);
        std::cout << indent << "EQUAL" << std::endl;
        print_ast_node(equalNode.left, depth + 1);
        print_ast_node(equalNode.right, depth + 1);
        return;
    } else if (is_type(node->node, RangeNode)) {
        auto rangeNode = std::get<RangeNode>(node->node);
        std::cout << indent << "RANGE" << std::endl;
        print_ast_node(rangeNode.left, depth + 1);
        print_ast_node(rangeNode.right, depth + 1);
        return;
    } else if (is_type(node->node, InNode)) {
        auto inNode = std::get<InNode>(node->node);
        std::cout << indent << "IN" << std::endl;
        print_ast_node(inNode.left, depth + 1);
        print_ast_node(inNode.right, depth + 1);
        return;
    } else if (is_type(node->node, IfNode)) {
        auto ifNode = std::get<IfNode>(node->node);
        std::cout << indent << "IF" << std::endl;
        print_ast_node(ifNode.condition, depth + 1);
        print_ast_node(ifNode.body, depth + 1);
        for (const auto& elseIf : *ifNode.elseIfs) {
            print_ast_node(elseIf->condition, depth + 1);
            print_ast_node(elseIf->body, depth + 1);
        }
        print_ast_node(ifNode.elseBody, depth + 1);
        return;
    } else if (is_type(node->node, OrNode)) {
        auto orNode = std::get<OrNode>(node->node);
        std::cout << indent << "OR" << std::endl;
        print_ast_node(orNode.left, depth + 1);
        print_ast_node(orNode.right, depth + 1);
        return;
    } else if (is_type(node->node, AndNode)) {
        auto andNode = std::get<AndNode>(node->node);
        std::cout << indent << "AND" << std::endl;
        print_ast_node(andNode.left, depth + 1);
        print_ast_node(andNode.right, depth + 1);
        return;
    }
}