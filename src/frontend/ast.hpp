#ifndef __AST_HPP__
#define __AST_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sv {

// -----------------------------------------------------
// Source locations and tokens
// -----------------------------------------------------

struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;
};

enum class TokenKind {
    Identifier,
    Number,
    String,
    Symbol,
    Keyword,
    EndOfFile
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string text;
    SourceLocation loc;
};

// -----------------------------------------------------
// AST base
// -----------------------------------------------------

struct Node {
    SourceLocation loc;
    virtual ~Node() = default;
};

// -----------------------------------------------------
// Expressions
// -----------------------------------------------------

enum class ExprKind {
    Identifier,
    Number,
    String,
    Unary,
    Binary,
    Ternary,
    Concatenation,
    Replication
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    BitAnd,
    BitOr,
    BitXor,

    LogicalAnd,
    LogicalOr,

    Eq,
    Neq,
    CaseEq,
    CaseNeq,
    Lt,
    Gt,
    Le,
    Ge,

    Shl,
    Shr,
    Ashl,
    Ashr
};

enum class UnaryOp {
    Plus,
    Minus,
    LogicalNot,
    BitNot
};

struct Expression : Node {
    ExprKind kind;

    // Identifier
    std::string ident;

    // Number / String
    std::string literal;

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

    // Concatenation {a, b, c}
    std::vector<std::unique_ptr<Expression>> concat_elems;

    // Replication {N{a, b}}
    std::unique_ptr<Expression> replicate_count;
    std::vector<std::unique_ptr<Expression>> replicate_elems;

    explicit Expression(ExprKind k) : kind(k) {}
};

// -----------------------------------------------------
// Statements
// -----------------------------------------------------

enum class StmtKind {
    Null,
    Block,
    If,
    Case,
    BlockingAssign,
    NonBlockingAssign
};

enum class CaseKind {
    Case,
    CaseZ,
    CaseX
};

struct CaseItem {
    std::vector<std::unique_ptr<Expression>> matches; // empty => default
    std::unique_ptr<struct Statement> stmt;
};

struct Statement : Node {
    StmtKind kind;

    // Block
    std::vector<std::unique_ptr<Statement>> block_stmts;

    // If
    std::unique_ptr<Expression> if_cond;
    std::unique_ptr<Statement> if_then;
    std::unique_ptr<Statement> if_else;

    // Case
    CaseKind case_kind = CaseKind::Case;
    std::unique_ptr<Expression> case_expr;
    std::vector<CaseItem> case_items;

    // Assignments
    std::unique_ptr<Expression> lhs;
    std::unique_ptr<Expression> rhs;

    explicit Statement(StmtKind k) : kind(k) {}
};

// -----------------------------------------------------
// Types, ports, declarations
// -----------------------------------------------------

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
    bool is_packed = false;
    int msb = -1;
    int lsb = -1;
};

struct PortDecl : Node {
    PortDirection dir = PortDirection::Input;
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

// -----------------------------------------------------
// Always / initial constructs
// -----------------------------------------------------

enum class AlwaysKind {
    Always,
    AlwaysFF,
    AlwaysComb,
    AlwaysLatch
};

struct SensitivityItem {
    bool posedge = false;
    bool negedge = false;
    bool star = false; // @* or @(*)
    std::unique_ptr<Expression> expr;
};

struct AlwaysConstruct : Node {
    AlwaysKind kind = AlwaysKind::Always;
    std::vector<SensitivityItem> sensitivity_list;
    std::unique_ptr<Statement> body;
};

struct InitialConstruct : Node {
    std::unique_ptr<Statement> body;
};

// -----------------------------------------------------
// Module items and modules
// -----------------------------------------------------

enum class ModuleItemKind {
    NetDecl,
    VarDecl,
    ParamDecl,
    ContinuousAssign,
    Always,
    Initial,
    Instance
};

struct InstancePortConn {
    std::string port_name; // empty => positional
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
    std::unique_ptr<InitialConstruct> initial;
    std::unique_ptr<Instance> instance;

    explicit ModuleItem(ModuleItemKind k) : kind(k) {}
};

struct ModuleDecl : Node {
    std::string name;
    std::vector<std::unique_ptr<PortDecl>> ports;
    std::vector<std::unique_ptr<ModuleItem>> items;
};

// -----------------------------------------------------
// Design root
// -----------------------------------------------------

struct Design : Node {
    std::vector<std::unique_ptr<ModuleDecl>> modules;
};

} // namespace sv

#endif // __AST_HPP__
