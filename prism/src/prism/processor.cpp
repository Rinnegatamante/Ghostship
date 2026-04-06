#include "processor.h"

#include <spdlog/spdlog.h>
#include <sstream>
#include "utils/exceptions.h"
#include "utils/gv.h"

void prism::Processor::populate(const ContextItems& items) {
    if (CONTAINS(items, "@if")) {
        throw SyntaxError("Reserved keyword if");
    }

    if (CONTAINS(items, "@else")) {
        throw SyntaxError("Reserved keyword else");
    }

    if (CONTAINS(items, "@elseif")) {
        throw SyntaxError("Reserved keyword elseif");
    }

    if (CONTAINS(items, "@for")) {
        throw SyntaxError("Reserved keyword for");
    }

    if (CONTAINS(items, "@end")) {
        throw SyntaxError("Reserved keyword end");
    }

    if (CONTAINS(items, "@prism")) {
        throw SyntaxError("Reserved keyword prism");
    }

    m_items = items;
}

std::string prism::Processor::parse_header(const std::string& data) {
    std::vector<std::string> m_lines;
    std::stringstream ss(data);
    std::string line;
    std::string result;
    while (std::getline(ss, line)) {
        m_lines.push_back(line);
    }

    if (m_lines.empty()) {
        throw RuntimeError("No data to process");
    }
    
    if (m_lines[0].rfind("@prism", 0) != 0) {
        throw SyntaxError("Invalid prism file");
    }

    auto header = gv::parenthesis(m_lines[0].substr(6));
    auto args = gv::il_args(header[0]);

    if (!CONTAINS(args, "type")) {
        throw SyntaxError("Type not specified");
    }

    auto type = args["type"];
    auto name = args["name"].value_or("prism_script");
    auto version = args["version"].value_or("1.0.0");
    auto description = args["description"].value_or("Unknown");
    auto author = args["author"].value_or("Someone very clever");
    m_lines.erase(m_lines.begin());
    if (m_lines[0].empty()) {
        m_lines.erase(m_lines.begin());
    }
    result = "";
    for (const auto& n_line : m_lines) {
        result += n_line + "\n";
    }
    SPDLOG_DEBUG("Prism script: {} v{} by {}", name, version, author);
    return result;
}

void prism::Processor::load(const std::string& data) {
   m_input = parse_header(data);
}

template <typename T> prism::ContextTypes read_array(prism::Processor* proc, prism::ast::ArrayAccessNode& array) {
    auto var = proc->getTypes().at(array.name->name);
    if (!is_type(var, prism::MTDArray<T>)) {
        throw prism::SyntaxError(array.name->name + " is not an array");
    }
    prism::MTDArray<T> arrayVar = std::get<prism::MTDArray<T>>(var);
    auto indices = array.arrayIndices;
    auto length = indices->size();

#define g_idx(x) std::get<int>(proc->evaluate(indices->at(x)))
    if (arrayVar.dimensions.size() != length) {
        switch (length) {
            case 1:
                return arrayVar.get(g_idx(0));
            case 2:
                return arrayVar.get(g_idx(0), g_idx(1));
            case 3:
                return arrayVar.get(g_idx(0), g_idx(1), g_idx(2));
            default:
                throw prism::SyntaxError("We dont support array indexes bigger than 3");
        }
    }

    switch (indices->size()) {
        case 1:
            return arrayVar.at(g_idx(0));
        case 2:
            return arrayVar.at(g_idx(0), g_idx(1));
        case 3:
            return arrayVar.at(g_idx(0), g_idx(1), g_idx(2));
        case 4:
            return arrayVar.at(g_idx(0), g_idx(1), g_idx(2), g_idx(3));
    }
#undef g_idx
    throw prism::SyntaxError("We dont support array indexes bigger than 4");
}

prism::ContextTypes prism::Processor::evaluate(const std::shared_ptr<prism::ast::ASTNode>& node) {
    if (!node)
        return false;
    if (is_type(node->node, prism::ast::VariableNode)) {
        auto var = std::get<prism::ast::VariableNode>(node->node);
        if (!CONTAINS(this->m_items, var.name)) {
            throw SyntaxError("Unknown variable " + var.name);
        }
        auto value = this->m_items.at(var.name);
        if (is_type(value, int)) {
            return std::get<int>(value);
        }
        if (is_type(value, float)) {
            return std::get<float>(value);
        }
        if (is_type(value, std::string)) {
            return std::get<std::string>(value);
        }
        if (is_type(value, MTDArray<bool>)) {
            return std::get<MTDArray<bool>>(value);
        }
        if (is_type(value, MTDArray<int>)) {
            return std::get<MTDArray<int>>(value);
        }
        if (is_type(value, MTDArray<float>)) {
            return std::get<MTDArray<float>>(value);
        }
        throw SyntaxError("Unsupported type");
    } else if (is_type(node->node, prism::ast::IntegerNode)) {
        return std::get<prism::ast::IntegerNode>(node->node).value;
    } else if (is_type(node->node, prism::ast::FloatNode)) {
        return std::get<prism::ast::FloatNode>(node->node).value;
    } else if (is_type(node->node, prism::ast::ArrayAccessNode)) {
        auto array = std::get<prism::ast::ArrayAccessNode>(node->node);
        if (!CONTAINS(this->m_items, array.name->name)) {
            throw SyntaxError("Unknown variable " + array.name->name);
        }
        auto var = this->m_items.at(array.name->name);
        if (is_type(var, MTDArray<bool>)) {
            return read_array<bool>(this, array);
        }
        if (is_type(var, MTDArray<int>)) {
            return read_array<int>(this, array);
        }
        if (is_type(var, MTDArray<float>)) {
            return read_array<float>(this, array);
        }

        throw SyntaxError("Invalid array access");
    } else if (is_type(node->node, prism::ast::InNode)) {
        auto inNode = std::get<prism::ast::InNode>(node->node);
        auto var = std::get<prism::ast::VariableNode>(inNode.left->node);
        auto result = evaluate(inNode.right);
        if (is_type(result, int) || is_type(result, float)) {
            throw SyntaxError("Invalid IN operation");
        }
        if (is_type(result, MTDArray<int>)) {
            return ForContext{ var.name, std::get<MTDArray<int>>(result) };
        }
        if (is_type(result, MTDArray<float>)) {
            return ForContext{ var.name, std::get<MTDArray<float>>(result) };
        }
        if (is_type(result, MTDArray<bool>)) {
            return ForContext{ var.name, std::get<MTDArray<bool>>(result) };
        }
        if (is_type(result, GeneratedRange)) {
            return ForContext{ var.name, std::get<GeneratedRange>(result) };
        }
        throw SyntaxError("Invalid IN operation");
    } else if (is_type(node->node, prism::ast::NotNode)) {
        auto notNode = std::get<prism::ast::NotNode>(node->node);
        auto value = evaluate(notNode.node);
        if (is_type(value, int)) {
            return std::get<int>(value) == 0;
        }
        throw SyntaxError("Invalid NOT operation");
    } else if (is_type(node->node, prism::ast::AssignNode)) {
        auto assignNode = std::get<prism::ast::AssignNode>(node->node);
        auto value = evaluate(assignNode.value);
        if (is_type(value, GeneratedRange) || is_type(value, std::string) || is_type(value, ForContext)) {
            throw SyntaxError("Invalid assign operation");
        }
        this->m_items[assignNode.name.name] = prism::ContextTypes{ value };
        return Void{};
    } else if (is_type(node->node, prism::ast::OrNode)) {
        auto orNode = std::get<prism::ast::OrNode>(node->node);
        auto left = evaluate(orNode.left);
        auto right = evaluate(orNode.right);
        bool resLeft = false;
        bool resRight = false;
        
        if (is_type(left, int)) {
            resLeft = std::get<int>(left) == 1;
        }
        
        if (is_type(right, int)) {
            resRight = std::get<int>(right) == 1;
        }
        
        if (is_type(left, float) || is_type(right, float)) {
            throw SyntaxError("Invalid AND operation, float are not supported");
        }
        
        return resLeft || resRight;
    } else if (is_type(node->node, prism::ast::AndNode)) {
        auto andNode = std::get<prism::ast::AndNode>(node->node);
        auto left = evaluate(andNode.left);
        auto right = evaluate(andNode.right);
        bool resLeft = false;
        bool resRight = false;
        
        if (is_type(left, int)) {
            resLeft = std::get<int>(left) == 1;
        }
        
        if (is_type(right, int)) {
            resRight = std::get<int>(right) == 1;
        }
        
        if (is_type(left, float) || is_type(right, float)) {
            throw SyntaxError("Invalid AND operation, float are not supported");
        }
        return resLeft && resRight;
    } else if (is_type(node->node, prism::ast::EqualNode)) {
        auto equalNode = std::get<prism::ast::EqualNode>(node->node);
        auto left = evaluate(equalNode.left);
        auto right = evaluate(equalNode.right);

        if (is_type(left, int) && is_type(right, int)) {
            return std::get<int>(left) == std::get<int>(right);
        }

        if (is_type(left, float) && is_type(right, float)) {
            return std::get<float>(left) == std::get<float>(right);
        }

        throw SyntaxError("Invalid EQUAL operation");
    } else if (is_type(node->node, prism::ast::AddNode)) {
        auto addNode = std::get<prism::ast::AddNode>(node->node);
        auto left = evaluate(addNode.left);
        auto right = evaluate(addNode.right);

        if (is_type(left, int) && is_type(right, int)) {
            return std::get<int>(left) + std::get<int>(right);
        }

        if (is_type(left, float) && is_type(right, float)) {
            return std::get<float>(left) + std::get<float>(right);
        }

        if (is_type(left, float) && is_type(right, int)) {
            return std::get<float>(left) + std::get<int>(right);
        }

        if (is_type(left, int) && is_type(right, float)) {
            return std::get<int>(left) + std::get<float>(right);
        }

        if (is_type(left, std::string) && is_type(right, std::string)) {
            return std::get<std::string>(left) + std::get<std::string>(right);
        }

        throw SyntaxError("Invalid ADD operation");
    } else if (is_type(node->node, prism::ast::SubNode)) {
        auto subNode = std::get<prism::ast::SubNode>(node->node);
        auto left = evaluate(subNode.left);
        auto right = evaluate(subNode.right);

        if (is_type(left, int) && is_type(right, int)) {
            return std::get<int>(left) - std::get<int>(right);
        }

        if (is_type(left, float) && is_type(right, float)) {
            return std::get<float>(left) - std::get<float>(right);
        }

        if (is_type(left, float) && is_type(right, int)) {
            return std::get<float>(left) - std::get<int>(right);
        }

        if (is_type(left, int) && is_type(right, float)) {
            return std::get<int>(left) - std::get<float>(right);
        }

        throw SyntaxError("Invalid SUB operation");
    } else if (is_type(node->node, prism::ast::MulNode)) {
        auto mulNode = std::get<prism::ast::MulNode>(node->node);
        auto left = evaluate(mulNode.left);
        auto right = evaluate(mulNode.right);

        if (is_type(left, int) && is_type(right, int)) {
            return std::get<int>(left) * std::get<int>(right);
        }

        if (is_type(left, float) && is_type(right, float)) {
            return std::get<float>(left) * std::get<float>(right);
        }

        if (is_type(left, float) && is_type(right, int)) {
            return std::get<float>(left) * std::get<int>(right);
        }

        if (is_type(left, int) && is_type(right, float)) {
            return std::get<int>(left) * std::get<float>(right);
        }

        throw SyntaxError("Invalid MUL operation");
    } else if (is_type(node->node, prism::ast::DivNode)) {
        auto mulNode = std::get<prism::ast::DivNode>(node->node);
        auto left = evaluate(mulNode.left);
        auto right = evaluate(mulNode.right);

        if (is_type(left, int) && is_type(right, int)) {
            return std::get<int>(left) / std::get<int>(right);
        }

        if (is_type(left, float) && is_type(right, float)) {
            return std::get<float>(left) / std::get<float>(right);
        }

        if (is_type(left, float) && is_type(right, int)) {
            return std::get<float>(left) / std::get<int>(right);
        }

        if (is_type(left, int) && is_type(right, float)) {
            return std::get<int>(left) / std::get<float>(right);
        }

        throw SyntaxError("Invalid MUL operation");
    } else if (is_type(node->node, prism::ast::RangeNode)) {
        auto rangeNode = std::get<prism::ast::RangeNode>(node->node);
        auto start = evaluate(rangeNode.left);
        auto end = evaluate(rangeNode.right);
        if (!is_type(start, int) || !is_type(end, int)) {
            throw SyntaxError("Invalid range");
        }

        return prism::GeneratedRange{ (size_t) std::get<int>(start), (size_t) std::get<int>(end) };
    } else if (is_type(node->node, prism::ast::IfNode)) {
        auto ifNode = std::get<prism::ast::IfNode>(node->node);
        auto condition = evaluate(ifNode.condition);
        if ((is_type(condition, int) && std::get<int>(condition) == 1)) {
            return evaluate(ifNode.body);
        } else if (!ifNode.elseIfs->empty()) {
            for (const auto& elseIf : *ifNode.elseIfs) {
                condition = evaluate(elseIf->condition);
                if ((is_type(condition, int) && std::get<int>(condition) == 1)) {
                    return evaluate(elseIf->body);
                }
            }
        }

        if (ifNode.elseBody != nullptr) {
            return evaluate(ifNode.elseBody);
        }
        throw SyntaxError("Invalid IF condition");
    } else if (is_type(node->node, prism::ast::FunctionCallNode)) {
        auto func = std::get<prism::ast::FunctionCallNode>(node->node);
        if (CONTAINS(m_items, func.name->name)) {
            auto value = m_items.at(func.name->name);
            if (is_type(value, InvokeFunc)) {
                std::vector<uintptr_t> args;
                args.push_back((uintptr_t) &m_items);
                for (const auto& arg : *func.args) {
                    ContextTypes type = evaluate(arg);
                    args.push_back((uintptr_t) new ContextTypes{ type });
                }
                auto ptr = std::get<InvokeFunc>(value);
                auto raw = invoke(ptr, args.data(), args.size());
                // erase the first item which is the context items
                args.erase(args.begin());
                for (auto& c : args) {
                    delete (ContextTypes*) c;
                }
                args.clear();
                if (raw != (uintptr_t) nullptr) {
                    ContextTypes cnv = *((ContextTypes*) raw);
                    delete (ContextTypes*) raw;
                    return cnv;
                } else {
                    return Void{};
                }
            }
        }
        throw SyntaxError("Unsupported function call " + func.name->name);
    } else if (is_type(node->node, prism::ast::QuoteNode)) {
        return std::get<prism::ast::QuoteNode>(node->node).value;
    }
    return false;
}

std::string get_parenthesis(std::string::iterator& c, std::string::iterator end) {
    auto start = c;
    int parenthesis = 0;
    while (c != end) {
        if (*c == '(') {
            parenthesis++;
        }
        if (*c == ')') {
            parenthesis--;
        }
        if (parenthesis == 0) {
            break;
        }
        c++;
    }
    if (c == end) {
        throw prism::SyntaxError("Unterminated parenthesis");
    }
    c++;
    return { start, c };
}

std::shared_ptr<prism::ast::ASTNode> parse_parenthesis(std::string::iterator& c, std::string::iterator end) {
    prism::lexer::Lexer eval(get_parenthesis(c, end));
    auto tokens = eval.tokenize();
    prism::ast::Parser parser(tokens);
    auto ast = parser.parse();
    return ast;
}

std::string get_accolade(std::string::iterator& c, std::string::iterator end) {
    auto start = c + 1;
    int accolade = 0;
    while (c != end) {
        if (*c == '{') {
            accolade++;
        }
        if (*c == '}') {
            accolade--;
        }
        if (accolade == 0) {
            break;
        }
        c++;
    }
    if (c == end) {
        throw prism::SyntaxError("Unterminated accolade");
    }
    auto result = std::string(start, c);
    c++;
    return result;
}

std::shared_ptr<prism::ast::ASTNode> parse_accolade(std::string::iterator& c, std::string::iterator end) {
    prism::lexer::Lexer eval(get_accolade(c, end));
    auto tokens = eval.tokenize();
    prism::ast::Parser parser(tokens);
    auto ast = parser.parse();
    return ast;
}

std::string get_keyword(std::string::iterator& c, std::string::iterator end) {
    auto start = c;
    char match[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                     's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
                     'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_' };
    while (std::find(std::begin(match), std::end(match), *c) != std::end(match) && c != end) {
        c++;
    }
    return { start, c };
}

std::shared_ptr<std::vector<std::shared_ptr<prism::Node>>> get_children(const std::shared_ptr<prism::Node>& node) {
    if (is_type(node->node, prism::RootNode)) {
        return std::get<prism::RootNode>(node->node).children;
    }
    if (is_type(node->node, prism::IfNode)) {
        return std::get<prism::IfNode>(node->node).children;
    }
    if (is_type(node->node, prism::ElseNode)) {
        return std::get<prism::ElseNode>(node->node).children;
    }
    if (is_type(node->node, prism::ElseIfNode)) {
        return std::get<prism::ElseIfNode>(node->node).children;
    }
    if (is_type(node->node, prism::ForNode)) {
        return std::get<prism::ForNode>(node->node).children;
    }
    throw prism::SyntaxError("Unsupported node type");
}

bool is_on_the_same_line(std::string::iterator& c, std::string::iterator end) {
    while (*c != '\n') {
        if (*c == ';') {
            return true;
        }
        c++;
    }
    return false;
}

prism::Node prism::Processor::parse(std::string input) {
    std::shared_ptr<prism::Node> root = std::make_shared<prism::Node>(
        prism::RootNode{ std::make_shared<std::vector<std::shared_ptr<prism::Node>>>() }, nullptr);
    auto current = root;
    auto children = std::get<prism::RootNode>(root->node).children;
    auto c = input.begin();
    auto previous = c;
    bool canBeOnTheSameLine = false;
    bool isOnTheSameLine = false;
    int ifCount = 0;
    auto end = input.end();
    while (c != end) {
        if (canBeOnTheSameLine) {
            if (!std::isspace(*c)) {
                isOnTheSameLine = true;
            }
            if (isOnTheSameLine) {
                if (*c == '\n') {
                    canBeOnTheSameLine = false;
                    children->push_back(
                        std::make_shared<prism::Node>(prism::TextNode{ std::string(previous, c + 1) }, current));
                    previous = c;
                    current = current->parent;
                    children = get_children(current);
                }
            }
        }
        if (*c == '\n' && canBeOnTheSameLine && !isOnTheSameLine) {
            isOnTheSameLine = false;
            canBeOnTheSameLine = false;
            continue;
        }
        if (*c == '@') {
            children->push_back(std::make_shared<prism::Node>(prism::TextNode{ std::string(previous, c) }, current));
            c++;
            if (*c == '{') {
                auto ast = parse_accolade(c, input.end());
                children->push_back(std::make_shared<prism::Node>(prism::VariableNode{ ast }, current));
                previous = c;
            } else {
                auto expr = get_keyword(c, input.end());
                previous = c;
                if (expr == "if") {
                    ifCount++;
                    auto ast = parse_parenthesis(c, input.end());
                    previous = c;

                    children->push_back(std::make_shared<prism::Node>(
                        prism::IfNode{ ast, std::make_shared<std::vector<std::shared_ptr<prism::Node>>>() }, current));
                    current = children->back();
                    children = std::get<prism::IfNode>(current->node).children;

                    canBeOnTheSameLine = true;
                    isOnTheSameLine = false;
                    continue;
                } else if (expr == "else") {
                    auto ifNode = current;
                    if (!is_type(ifNode->node, prism::IfNode) && !is_type(ifNode->node, prism::ElseIfNode) ) {
                        auto previous = children->at(children->size() - 2);
                        if (!(isOnTheSameLine && (is_type(previous->node, prism::IfNode) || is_type(previous->node, prism::ElseIfNode))) ) {
                            throw prism::SyntaxError("Else without if");
                        } else if (isOnTheSameLine && (is_type(previous->node, prism::IfNode) || is_type(previous->node, prism::ElseIfNode))) {
                            ifNode = previous;
                        }
                    }

                    if (is_type(ifNode->node, prism::ElseIfNode)) {
                        ifNode = std::get<prism::ElseIfNode>(ifNode->node).parentIf;
                    }

                    current = ifNode->parent;

                    auto newNode = std::make_shared<prism::Node>(
                        prism::ElseNode{ std::make_shared<std::vector<std::shared_ptr<prism::Node>>>() }, current);

                    auto ifNodePtr = std::get<prism::IfNode>(ifNode->node);
                    ifNodePtr.elseBody = newNode;
                    ifNode->node = ifNodePtr;

                    get_children(current)->push_back(newNode);
                    current = get_children(current)->back();
                    children = std::get<prism::ElseNode>(current->node).children;

                    canBeOnTheSameLine = true;
                    isOnTheSameLine = false;
                    continue;
                } else if (expr == "elseif") {
                    auto ifNode = current;
                    if (!is_type(ifNode->node, prism::IfNode) && !is_type(ifNode->node, prism::ElseIfNode) ) {
                        auto previous = children->at(children->size() - 2);
                        if (!(isOnTheSameLine && (is_type(previous->node, prism::IfNode) || is_type(previous->node, prism::ElseIfNode))) ) {
                            throw prism::SyntaxError("Else without if");
                        } else if (isOnTheSameLine && (is_type(previous->node, prism::IfNode) || is_type(previous->node, prism::ElseIfNode))) {
                            ifNode = previous;
                        }
                    }

                    if (is_type(ifNode->node, prism::ElseIfNode)) {
                        ifNode = std::get<prism::ElseIfNode>(ifNode->node).parentIf;
                    }

                    current = ifNode->parent;

                    auto ast = parse_parenthesis(c, input.end());
                    previous = c;

                    auto newNode = std::make_shared<prism::Node>(
                        prism::ElseIfNode{ ast, std::make_shared<std::vector<std::shared_ptr<prism::Node>>>(), ifNode },
                        current);
                    auto ifNodePtr = std::get<prism::IfNode>(ifNode->node);
                    ifNodePtr.elseIfs.push_back(newNode);
                    ifNode->node = ifNodePtr;

                    get_children(current)->push_back(newNode);
                    current = get_children(current)->back();
                    children = std::get<prism::ElseIfNode>(current->node).children;

                    canBeOnTheSameLine = true;
                    isOnTheSameLine = false;
                    continue;
                } else if (expr == "for") {
                    auto ast = parse_parenthesis(c, input.end());
                    previous = c;

                    children->push_back(std::make_shared<prism::Node>(
                        prism::ForNode{ ast, std::make_shared<std::vector<std::shared_ptr<prism::Node>>>() }, current));
                    current = children->back();
                    children = std::get<prism::ForNode>(current->node).children;

                    canBeOnTheSameLine = true;
                    isOnTheSameLine = false;
                    continue;
                } else if (expr == "end") {
                    if (current == root) {
                        throw prism::SyntaxError("Unmatched end at " + input.substr(previous - input.begin()));
                    }
                    current = current->parent;
                    children = get_children(current);
                    previous = c;
                } else if(expr == "include") {
                    if(this->m_include_loader == nullptr) {
                        throw RuntimeError("Include loader not set");
                    }
                    auto file = parse_parenthesis(c, input.end());
                    previous = c;
                    std::string path = std::get<std::string>(evaluate(file));
                    auto res = this->m_include_loader(path);
                    if(!res.has_value()){
                        throw SyntaxError("Failed to load include from" + path);
                    }
                    auto buffer = parse_header(res.value());
                    auto pos = c - input.begin();
                    input.insert(c, buffer.begin(), buffer.end());
                    c = input.begin() + pos;
                    previous = c;
                    end = input.end();
                    continue;
                }
            }
        }
        c++;
    }

    children->push_back(std::make_shared<prism::Node>(prism::TextNode{ std::string(previous, c) }, current));

    if (current != root) {
        throw prism::SyntaxError("Unterminated block");
    }
    return *root;
}

void prism::Processor::evaluate_node(std::shared_ptr<std::vector<std::shared_ptr<prism::Node>>>& children) {
    for (const auto& child : *children) {
        if (is_type(child->node, prism::TextNode)) {
            auto result = std::get<prism::TextNode>(child->node).text;
            m_output << result;
        } else if (is_type(child->node, prism::VariableNode)) {
            auto var = std::get<prism::VariableNode>(child->node);
            auto value = evaluate(var.name);
            if (is_type(value, int)) {
                m_output << std::get<int>(value);
            } else if (is_type(value, float)) {
                m_output << std::get<float>(value);
            } else if (is_type(value, std::string)) {
                m_output << std::get<std::string>(value);
            } else if (is_type(value, Void)) {
                continue;
            } else {
                throw prism::SyntaxError("Unsupported type");
            }
        } else if (is_type(child->node, prism::IfNode)) {
            auto ifNode = std::get<prism::IfNode>(child->node);
            auto condition = evaluate(ifNode.condition);
            if ((is_type(condition, int) && std::get<int>(condition) == 1)) {
                evaluate_node(ifNode.children);
                continue;
            } else if (!ifNode.elseIfs.empty()) {
                for (const auto& node : ifNode.elseIfs) {
                    auto elseIf = std::get<prism::ElseIfNode>(node->node);
                    condition = evaluate(elseIf.condition);
                    if ((is_type(condition, int) && std::get<int>(condition) == 1)) {
                        evaluate_node(elseIf.children);
                        return;
                    }
                }
            }

            if (ifNode.elseBody != nullptr) {
                auto elseNode = std::get<prism::ElseNode>(ifNode.elseBody->node);
                evaluate_node(elseNode.children);
            }

        } else if (is_type(child->node, prism::ForNode)) {
            auto forNode = std::get<prism::ForNode>(child->node);
            auto context = std::get<prism::ForContext>(evaluate(forNode.condition));

            if (is_type(context.iterator, GeneratedRange)) {
                auto range = std::get<GeneratedRange>(context.iterator);
                auto var = context.name;
                for (auto i = range.start; i < range.end; i++) {
                    m_items[var] = ContextTypes{ (int) i };
                    evaluate_node(forNode.children);
                }
                m_items.erase(var);
            } else if (is_type(context.iterator, MTDArray<bool>)) {
                array_iterate<bool>(forNode, context);
            } else if (is_type(context.iterator, MTDArray<int>)) {
                array_iterate<int>(forNode, context);
            } else if (is_type(context.iterator, MTDArray<float>)) {
                array_iterate<float>(forNode, context);
            }
        }
    }
}

void print_node(const prism::Node& node, int depth = 0) {
    for (int i = 0; i < depth; i++) {
        std::cout << ">";
    }
    if (is_type(node.node, prism::TextNode)) {
        std::cout << std::get<prism::TextNode>(node.node).text << std::endl;
    } else if (is_type(node.node, prism::IfNode)) {
        std::cout << "If" << std::endl;
        auto ifNode = std::get<prism::IfNode>(node.node);
        print_ast_node(ifNode.condition, depth + 1);
        for (const auto& child : *ifNode.children) {
            print_node(*child, depth + 1);
        }
        for (int i = 0; i < depth; i++) {
            std::cout << ">";
        }
        std::cout << "End" << std::endl;
    } else if (is_type(node.node, prism::ElseNode)) {
        std::cout << "Else" << std::endl;
        auto elseNode = std::get<prism::ElseNode>(node.node);
        for (const auto& child : *elseNode.children) {
            print_node(*child, depth + 1);
        }
        for (int i = 0; i < depth; i++) {
            std::cout << ">";
        }
        std::cout << "End" << std::endl;
    } else if (std::holds_alternative<prism::ElseIfNode>(node.node)) {
        std::cout << "ElseIf" << std::endl;
        auto elseIfNode = std::get<prism::ElseIfNode>(node.node);
        print_ast_node(elseIfNode.condition, depth + 1);
        for (const auto& child : *elseIfNode.children) {
            print_node(*child, depth + 1);
        }
        for (int i = 0; i < depth; i++) {
            std::cout << ">";
        }
        std::cout << "End" << std::endl;
    } else if (std::holds_alternative<prism::ForNode>(node.node)) {
        std::cout << "For" << std::endl;
        auto forNode = std::get<prism::ForNode>(node.node);
        print_ast_node(forNode.condition, depth + 1);
        for (const auto& child : *forNode.children) {
            print_node(*child, depth + 1);
        }
        for (int i = 0; i < depth; i++) {
            std::cout << ">";
        }
        std::cout << "End" << std::endl;
    } else if (std::holds_alternative<prism::VariableNode>(node.node)) {
        std::cout << "Variable" << std::endl;
        print_ast_node(std::get<prism::VariableNode>(node.node).name, depth + 1);
    }
}

std::string prism::Processor::process() {
    auto node = parse(m_input);
#ifdef DEBUG_PARSE
    for (const auto& child : *std::get<prism::RootNode>(node.node).children) {
        print_node(*child);
    }
#endif
    evaluate_node(std::get<prism::RootNode>(node.node).children);
    for (const auto& child : *std::get<prism::RootNode>(node.node).children) {
        delete_node((std::shared_ptr<prism::Node>&) child);
    }

    std::stringstream result;
    auto lines = gv::new_line_split(m_output.str());
    for (const auto& line : lines) {
        auto trimmed = gv::trim(line);
        if (trimmed.empty()) {
            continue;
        }
        result << trimmed << std::endl;
    }
    return result.str();
}

void prism::delete_node(std::shared_ptr<prism::Node>& node) {
    if (node == nullptr) {
        return;
    }
    if (is_type(node->node, prism::RootNode)) {
        for (const auto& child : *std::get<prism::RootNode>(node->node).children) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
    }
    if (is_type(node->node, prism::IfNode)) {
        for (const auto& child : *std::get<prism::IfNode>(node->node).children) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
        for (const auto& child : std::get<prism::IfNode>(node->node).elseIfs) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
        if (std::get<prism::IfNode>(node->node).elseBody) {
            delete_node(std::get<prism::IfNode>(node->node).elseBody);
        }
    }
    if (is_type(node->node, prism::ElseNode)) {
        for (const auto& child : *std::get<prism::ElseNode>(node->node).children) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
    }
    if (is_type(node->node, prism::ElseIfNode)) {
        for (const auto& child : *std::get<prism::ElseIfNode>(node->node).children) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
    }
    if (is_type(node->node, prism::ForNode)) {
        for (const auto& child : *std::get<prism::ForNode>(node->node).children) {
            delete_node((std::shared_ptr<prism::Node>&) child);
        }
    }
    node.reset();
}
