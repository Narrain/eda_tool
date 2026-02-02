#ifndef SV_RTL_IR_HPP
#define SV_RTL_IR_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../frontend/ast.hpp"  // for sv::DataType

namespace sv {

// ============================================================================
// Expressions
// ============================================================================

enum class RtlExprKind {
    Ref,
    Const,
    Unary,
    Binary,
    BitSelect        // support for r[i]
};

enum class RtlUnOp {
    Plus,
    Minus,
    Not,
    BitNot
};

enum class RtlBinOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
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

struct RtlExpr;

// helper to deep‑clone an expression tree
inline std::unique_ptr<RtlExpr> clone_expr(const std::unique_ptr<RtlExpr> &src);

struct RtlExpr {
    RtlExprKind kind = RtlExprKind::Const;

    // Ref
    std::string ref_name;

    // Const
    std::string const_literal;

    // Unary
    RtlUnOp un_op = RtlUnOp::Plus;
    std::unique_ptr<RtlExpr> un_operand;

    // Binary
    RtlBinOp bin_op = RtlBinOp::Add;
    std::unique_ptr<RtlExpr> lhs;
    std::unique_ptr<RtlExpr> rhs;

    // BitSelect: base[index]
    std::unique_ptr<RtlExpr> base;
    std::unique_ptr<RtlExpr> index;

    RtlExpr() = default;
    explicit RtlExpr(RtlExprKind k) : kind(k) {}

    // deep copy
    RtlExpr(const RtlExpr &o)
        : kind(o.kind),
          ref_name(o.ref_name),
          const_literal(o.const_literal),
          un_op(o.un_op),
          un_operand(clone_expr(o.un_operand)),
          bin_op(o.bin_op),
          lhs(clone_expr(o.lhs)),
          rhs(clone_expr(o.rhs)),
          base(clone_expr(o.base)),
          index(clone_expr(o.index)) {}

    RtlExpr &operator=(const RtlExpr &o) {
        if (this == &o) return *this;
        kind          = o.kind;
        ref_name      = o.ref_name;
        const_literal = o.const_literal;
        un_op         = o.un_op;
        un_operand    = clone_expr(o.un_operand);
        bin_op        = o.bin_op;
        lhs           = clone_expr(o.lhs);
        rhs           = clone_expr(o.rhs);
        base          = clone_expr(o.base);
        index         = clone_expr(o.index);
        return *this;
    }

    // default move
    RtlExpr(RtlExpr &&) noexcept = default;
    RtlExpr &operator=(RtlExpr &&) noexcept = default;
};

inline std::unique_ptr<RtlExpr> clone_expr(const std::unique_ptr<RtlExpr> &src) {
    if (!src) return nullptr;
    return std::make_unique<RtlExpr>(*src);
}

// ============================================================================
// Statements
// ============================================================================

enum class RtlStmtKind {
    BlockingAssign,
    NonBlockingAssign,
    Delay,
    Finish
};

enum class RtlAssignKind {
    Continuous,
    Blocking,
    NonBlocking
};

struct RtlStmt {
    RtlStmtKind kind = RtlStmtKind::BlockingAssign;

    // LHS can be either a plain name or an expression (BitSelect)
    std::string lhs_name;                 // whole‑net assignment
    std::unique_ptr<RtlExpr> lhs_expr;    // bit‑select or other LHS expr

    std::unique_ptr<RtlExpr> rhs;         // RHS expression
    std::unique_ptr<RtlExpr> delay_expr;  // for #delay

    RtlStmt *next = nullptr;

    RtlStmt() = default;

    // deep copy (do NOT copy next pointer; it will be rebuilt by RtlProcess)
    RtlStmt(const RtlStmt &o)
        : kind(o.kind),
          lhs_name(o.lhs_name),
          lhs_expr(clone_expr(o.lhs_expr)),
          rhs(clone_expr(o.rhs)),
          delay_expr(clone_expr(o.delay_expr)),
          next(nullptr) {}

    RtlStmt &operator=(const RtlStmt &o) {
        if (this == &o) return *this;
        kind       = o.kind;
        lhs_name   = o.lhs_name;
        lhs_expr   = clone_expr(o.lhs_expr);
        rhs        = clone_expr(o.rhs);
        delay_expr = clone_expr(o.delay_expr);
        next       = nullptr; // rebuilt by owner
        return *this;
    }

    RtlStmt(RtlStmt &&) noexcept = default;
    RtlStmt &operator=(RtlStmt &&) noexcept = default;
};

// ============================================================================
// Continuous assign
// ============================================================================

struct RtlAssign {
    RtlAssignKind kind = RtlAssignKind::Continuous;
    std::string lhs_name;
    std::unique_ptr<RtlExpr> rhs;

    RtlAssign() = default;

    RtlAssign(const RtlAssign &o)
        : kind(o.kind),
          lhs_name(o.lhs_name),
          rhs(clone_expr(o.rhs)) {}

    RtlAssign &operator=(const RtlAssign &o) {
        if (this == &o) return *this;
        kind     = o.kind;
        lhs_name = o.lhs_name;
        rhs      = clone_expr(o.rhs);
        return *this;
    }

    RtlAssign(RtlAssign &&) noexcept = default;
    RtlAssign &operator=(RtlAssign &&) noexcept = default;
};

// ============================================================================
// Sensitivity
// ============================================================================

struct RtlSensitivity {
    enum class Kind { Level, Posedge, Negedge } kind = Kind::Level;
    std::string signal;
};

// ============================================================================
// Processes
// ============================================================================

enum class RtlProcessKind {
    Always,
    Initial
};

struct RtlProcess {
    RtlProcessKind kind = RtlProcessKind::Always;

    std::vector<RtlSensitivity> sensitivity;

    // Procedural statements
    RtlStmt *first_stmt = nullptr;
    std::vector<std::unique_ptr<RtlStmt>> stmts;

    // For simple always blocks with only assigns
    std::vector<RtlAssign> assigns;

    RtlProcess() = default;

    // deep copy: clone stmts and rebuild first_stmt/next chain
    RtlProcess(const RtlProcess &o)
        : kind(o.kind),
          sensitivity(o.sensitivity),
          first_stmt(nullptr),
          assigns(o.assigns) {

        // clone statements
        stmts.reserve(o.stmts.size());
        for (const auto &sp : o.stmts) {
            if (sp) {
                stmts.push_back(std::make_unique<RtlStmt>(*sp));
            } else {
                stmts.push_back(nullptr);
            }
        }

        // rebuild linear next chain and first_stmt if there are any stmts
        if (!stmts.empty()) {
            first_stmt = stmts[0].get();
            for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
                if (stmts[i])
                    stmts[i]->next = stmts[i + 1].get();
            }
            if (stmts.back())
                stmts.back()->next = nullptr;
        }
    }

    RtlProcess &operator=(const RtlProcess &o) {
        if (this == &o) return *this;

        kind        = o.kind;
        sensitivity = o.sensitivity;
        assigns     = o.assigns;

        stmts.clear();
        first_stmt = nullptr;

        stmts.reserve(o.stmts.size());
        for (const auto &sp : o.stmts) {
            if (sp) {
                stmts.push_back(std::make_unique<RtlStmt>(*sp));
            } else {
                stmts.push_back(nullptr);
            }
        }

        if (!stmts.empty()) {
            first_stmt = stmts[0].get();
            for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
                if (stmts[i])
                    stmts[i]->next = stmts[i + 1].get();
            }
            if (stmts.back())
                stmts.back()->next = nullptr;
        }

        return *this;
    }

    RtlProcess(RtlProcess &&) noexcept = default;
    RtlProcess &operator=(RtlProcess &&) noexcept = default;
};

// ============================================================================
// Gates
// ============================================================================

enum class RtlGateKind {
    And,
    Or,
    Not,
    Nand,
    Nor,
    Xor,
    Xnor,
    Buf
};

struct RtlGate {
    RtlGateKind kind = RtlGateKind::And;
    std::string out;
    std::vector<std::string> inputs;
};

// ============================================================================
// Instances
// ============================================================================

struct RtlInstanceConn {
    std::string port_name;
    std::string signal_name;
};

struct RtlInstance {
    std::string module_name;
    std::string instance_name;
    std::vector<RtlInstanceConn> conns;
};

// ============================================================================
// Parameters
// ============================================================================

struct RtlParam {
    std::string name;
    std::string value_str;
};

// ============================================================================
// Nets
// ============================================================================

struct RtlNet {
    std::string name;
    DataType type;
};

// ============================================================================
// Module
// ============================================================================

struct RtlModule {
    std::string name;

    std::vector<RtlParam> params;
    std::vector<RtlNet> nets;
    std::vector<RtlAssign> continuous_assigns;
    std::vector<RtlProcess> processes;
    std::vector<RtlGate> gates;
    std::vector<RtlInstance> instances;
};

// ============================================================================
// Design
// ============================================================================

struct RtlDesign {
    std::vector<RtlModule> modules;
};

} // namespace sv

#endif // SV_RTL_IR_HPP
