#pragma once

#include <utility>
#include <vector>
#include <string>
#include <variant>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <cstdlib>

#include "lexer.h"
#include "ast.h"
#include "utils/invoke.h"
#include "utils/exceptions.h"

#define is_type(var, type) std::holds_alternative<type>((var))
#define M_ARRAY(arr, type, ...)                 \
    prism::MTDArray<type> {                     \
        (uintptr_t) &arr, std::vector<size_t> { \
            __VA_ARGS__                         \
        }                                       \
    }
#define VAR(name, type)             \
    {                               \
        name, prism::ContextTypes { \
            type                    \
        }                           \
    }

#define CONTAINS(_map, value) ((_map).find(value) != (_map).end())

namespace prism {
// todo: handle out of bounds access
template <typename T> struct MTDArray {
    uintptr_t ptr = 0;
    std::vector<size_t> dimensions;

    T& at(int x) {
        if (x >= dimensions[0]) {
            throw RuntimeError("Index out of bounds");
        }
        return ((T*) ptr)[x];
    }

    T& at(int x, int y) {
        if (x >= dimensions[0] || y >= dimensions[1]) {
            throw RuntimeError("Index out of bounds");
        }
        return get(x).at(y);
    }

    T& at(int x, int y, int z) {
        if (x >= dimensions[0] || y >= dimensions[1] || z >= dimensions[2]) {
            throw RuntimeError("Index out of bounds");
        }
        return get(x, y).at(z);
    }

    T& at(int x, int y, int z, int w) {
        if (x >= dimensions[0] || y >= dimensions[1] || z >= dimensions[2] || w >= dimensions[3]) {
            throw RuntimeError("Index out of bounds");
        }
        return get(x, y, z).at(w);
    }

    MTDArray<T> get(int x) {
        if (x >= dimensions[0]) {
            throw RuntimeError("Index out of bounds");
        }
        auto ptr = this->ptr;
        auto offset = x * sizeof(T);
        for (size_t i = 0; i < dimensions.size()-1; i++) {
            offset *= dimensions[i+1];
        }
        ptr += offset;
        return MTDArray<T>{ ptr,
                            std::vector<size_t>{ dimensions.begin() + 1, dimensions.end() } };
    }

    MTDArray<T> get(int x, int y) {
        if (x >= dimensions[0] || y >= dimensions[1]) {
            throw RuntimeError("Index out of bounds");
        }
        return get(x).get(y);
    }

    MTDArray<T> get(int x, int y, int z) {
        if (x >= dimensions[0] || y >= dimensions[1] || z >= dimensions[2]) {
            throw RuntimeError("Index out of bounds");
        }
        return get(x, y).get(z);
    }
};

struct GeneratedRange {
    size_t start;
    size_t end;
};

struct ForContext {
    std::string name;
    std::variant<GeneratedRange, MTDArray<bool>, MTDArray<int>, MTDArray<float>> iterator;
};

struct Void {};
struct Opaque {
    uintptr_t ptr;
};

typedef std::variant<Void, int, float, MTDArray<bool>, MTDArray<int>, MTDArray<float>, GeneratedRange,
                     std::string, ForContext, InvokeFunc, Opaque>
    ContextTypes;
typedef std::unordered_map<std::string, ContextTypes> ContextItems;

enum class ScopeType { None, If, Else, ElseIf, For };

enum class ExpressionType { None, Variable, If, Else, ElseIf, For, End };

struct Node;
struct RootNode {
    std::shared_ptr<std::vector<std::shared_ptr<Node>>> children;
};
struct TextNode {
    std::string text;
};
struct VariableNode {
    std::shared_ptr<ast::ASTNode> name;
};
struct ElseNode {
    std::shared_ptr<std::vector<std::shared_ptr<Node>>> children;
};
struct ElseIfNode {
    std::shared_ptr<ast::ASTNode> condition;
    std::shared_ptr<std::vector<std::shared_ptr<Node>>> children;
    std::shared_ptr<Node> parentIf;
};
struct IfNode {
    std::shared_ptr<ast::ASTNode> condition;
    std::shared_ptr<std::vector<std::shared_ptr<Node>>> children;
    std::shared_ptr<Node> elseBody;
    std::vector<std::shared_ptr<Node>> elseIfs;
};
struct ForNode {
    std::shared_ptr<ast::ASTNode> condition;
    std::shared_ptr<std::vector<std::shared_ptr<Node>>> children;
};
struct EndNode {};

typedef std::variant<RootNode, TextNode, VariableNode, IfNode, ElseIfNode, ElseNode, ForNode, EndNode> NodeType;

void delete_node(std::shared_ptr<prism::Node>& node);

class Node {
  public:
    Node(NodeType node, std::shared_ptr<Node> parent) : node(std::move(node)), parent(std::move(parent)) {
    }
    NodeType node;
    std::shared_ptr<Node> parent;
    int depth = 0;
};

struct RuntimeContext {
    ScopeType scope = ScopeType::None;
    bool skipUntilEnd = false;
};

typedef std::optional<std::string> (*IncludeFunc)(const std::string&);

class Processor {
  public:
    void populate(const ContextItems& items);
    void load(const std::string& input);
    std::string parse_header(const std::string& data);
    prism::Node parse(std::string input);
    ContextTypes evaluate(const std::shared_ptr<prism::ast::ASTNode>& node);
    void evaluate_node(std::shared_ptr<std::vector<std::shared_ptr<prism::Node>>>& children);
    std::string process();
    ContextItems getTypes() {
        return this->m_items;
    }
    void bind_include_loader(IncludeFunc func){
        m_include_loader = func;
    }

    template <typename T> void array_iterate(prism::ForNode& node, prism::ForContext& context) {
        auto var = context.name;
        auto array = std::get<prism::MTDArray<T>>(context.iterator);
        for (size_t i = 0; i < array.dimensions[0]; i++) {
            m_items[var] = prism::ContextTypes{ array.at(i) };
            evaluate_node(node.children);
        }
        m_items.erase(var);
    }

  private:
    ContextItems m_items;
    RuntimeContext m_context;
    std::stringstream m_output;
    std::string m_input;
    std::shared_ptr<prism::Node> m_root;
    IncludeFunc m_include_loader = nullptr;
};
} // namespace prism