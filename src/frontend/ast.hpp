// path: src/frontend/ast.hpp
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace sv {

// Forward declarations
struct Expression;
struct Statement;
struct ModuleItem;
struct PortDecl;
struct NetDecl;
struct VarDecl;
struct ParamDecl;
struct ContinuousAssign;
struct AlwaysConstruct;
struct ModuleDecl;

// Basic source location
struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;
};

// Base node
struct Node {
    SourceLocation loc;
    virtual ~Node() = default;
};

// Expressions
enum class ExprKind {
    Identifier,
    Number,
    Binary,
    Unary,
    Ternary
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    And,
    Or,
    Xor,
    Eq,
    Neq,
    Lt,
    Gt,
    Le,
    Ge,
    LogicalAnd,
    LogicalOr
};

enum class UnaryOp {
    Plus,
    Minus,
    Not,
    BitNot
};

struct Expression : Node {
    ExprKind kind;

    // Identifier
    std::string ident;

    // Number
    std::string number_literal;

    // Unary
    UnaryOp unary_op;
    std::unique_ptr<Expression> unary_operand;

    // Binary
    BinaryOp binary_op;
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;

    // Ternary
    std::unique_ptr<Expression> cond;
    std::unique_ptr<Expression> then_expr;
    std::unique_ptr<Expression> else_expr;

    Expression(ExprKind k) : kind(k) {}
};

// Statements
enum class StmtKind {
    Null,
    Block,
    If,
    Case, // reserved for future
    Assign,
    NonBlockingAssign,
    BlockingAssign,
    DelayControl,
    EventControl
};

struct Statement : Node {
    StmtKind kind;

    // Block
    std::vector<std::unique_ptr<Statement>> block_stmts;

    // If
    std::unique_ptr<Expression> if_cond;
    std::unique_ptr<Statement> if_then;
    std::unique_ptr<Statement> if_else;

    // Assignments
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;

    Statement(StmtKind k) : kind(k) {}
};

// Ports and declarations
enum class PortDirection {
    Input,
    Output,
    Inout
};

enum class DataTypeKind {
    Logic,
    Wire,
    Reg,
    Integer,
    Unknown
};

struct DataType {
    DataTypeKind kind = DataTypeKind::Unknown;
    int msb = -1;
    int lsb = -1;
    bool is_packed = false;
};

struct PortDecl : Node {
    PortDirection dir;
    DataType type;
    std::string name;
};

struct NetDecl : Node {
    DataType type;
    std::string name;
    std::unique_ptr<Expression> init;
};

struct VarDecl : Node {
    DataType type;
    std::string name;
    std::unique_ptr<Expression> init;
};

struct ParamDecl : Node {
    std::string name;
    std::unique_ptr<Expression> value;
};

struct ContinuousAssign : Node {
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;
};

enum class AlwaysKind {
    Always,
    AlwaysFF,
    AlwaysComb,
    AlwaysLatch
};

struct SensitivityItem {
    bool posedge = false;
    bool negedge = false;
    std::unique_ptr<Expression> expr;
};

struct AlwaysConstruct : Node {
    AlwaysKind kind = AlwaysKind::Always;
    std::vector<SensitivityItem> sensitivity_list;
    std::unique_ptr<Statement> body;
};

// Module items
enum class ModuleItemKind {
    NetDecl,
    VarDecl,
    ParamDecl,
    ContinuousAssign,
    Always,
    Instance
};

struct InstancePortConn {
    std::string port_name;
    std::unique_ptr<Expression> expr;
};

struct Instance : Node {
    std::string module_name;
    std::string instance_name;
    std::vector<InstancePortConn> port_conns;
};

struct ModuleItem : Node {
    ModuleItemKind kind;
    std::unique_ptr<NetDecl> net_decl;
    std::unique_ptr<VarDecl> var_decl;
    std::unique_ptr<ParamDecl> param_decl;
    std::unique_ptr<ContinuousAssign> cont_assign;
    std::unique_ptr<AlwaysConstruct> always;
    std::unique_ptr<Instance> instance;

    ModuleItem(ModuleItemKind k) : kind(k) {}
};

struct ModuleDecl : Node {
    std::string name;
    std::vector<std::unique_ptr<PortDecl>> ports;
    std::vector<std::unique_ptr<ModuleItem>> items;
};

// Design root
struct Design : Node {
    std::vector<std::unique_ptr<ModuleDecl>> modules;
};

} // namespace sv
