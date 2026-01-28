#ifndef __RTL_IR_HPP__
#define __RTL_IR_HPP__

#include <string>
#include <vector>
#include <memory>

#include "../frontend/ast.hpp"

namespace sv {

// ----------------------
// Expression
// ----------------------

enum class RtlExprKind {
    Ref,
    Const,
    Binary,
    Unary
};

enum class RtlBinOp {
    // arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    // bitwise
    And,
    Or,
    Xor,

    // comparisons
    Eq,
    Neq,
    CaseEq,
    CaseNeq,
    Lt,
    Gt,
    Le,
    Ge,

    // logical
    LogicalAnd,
    LogicalOr,

    // shifts
    Shl,
    Shr,
    Ashl,
    Ashr
};

enum class RtlUnOp {
    Plus,
    Minus,
    Not,     // logical not
    BitNot   // bitwise not
};

struct RtlExpr {
    RtlExprKind kind;

    // Ref
    std::string ref_name;

    // Const
    std::string const_literal;

    // Unary
    RtlUnOp un_op{};
    std::unique_ptr<RtlExpr> un_operand;

    // Binary
    RtlBinOp bin_op{};
    std::unique_ptr<RtlExpr> lhs;
    std::unique_ptr<RtlExpr> rhs;

    explicit RtlExpr(RtlExprKind k) : kind(k) {}

    // ---- deep copy constructor ----
    RtlExpr(const RtlExpr &o)
        : kind(o.kind),
          ref_name(o.ref_name),
          const_literal(o.const_literal),
          un_op(o.un_op),
          bin_op(o.bin_op)
    {
        if (o.un_operand) un_operand = std::make_unique<RtlExpr>(*o.un_operand);
        if (o.lhs)        lhs        = std::make_unique<RtlExpr>(*o.lhs);
        if (o.rhs)        rhs        = std::make_unique<RtlExpr>(*o.rhs);
    }

    // ---- deep copy assignment ----
    RtlExpr &operator=(const RtlExpr &o) {
        if (this == &o) return *this;
        kind          = o.kind;
        ref_name      = o.ref_name;
        const_literal = o.const_literal;
        un_op         = o.un_op;
        bin_op        = o.bin_op;

        un_operand = o.un_operand ? std::make_unique<RtlExpr>(*o.un_operand) : nullptr;
        lhs        = o.lhs        ? std::make_unique<RtlExpr>(*o.lhs)        : nullptr;
        rhs        = o.rhs        ? std::make_unique<RtlExpr>(*o.rhs)        : nullptr;

        return *this;
    }

    // ---- move support ----
    RtlExpr(RtlExpr &&) noexcept = default;
    RtlExpr &operator=(RtlExpr &&) noexcept = default;
};

// ----------------------
// Assignments
// ----------------------

enum class RtlAssignKind {
    Continuous,
    Blocking,
    NonBlocking
};

struct RtlAssign {
    RtlAssignKind kind{};
    std::string lhs_name;
    std::unique_ptr<RtlExpr> rhs;

    RtlAssign() = default;

    // deep copy
    RtlAssign(const RtlAssign &o)
        : kind(o.kind), lhs_name(o.lhs_name)
    {
        if (o.rhs) rhs = std::make_unique<RtlExpr>(*o.rhs);
    }

    RtlAssign &operator=(const RtlAssign &o) {
        if (this == &o) return *this;
        kind     = o.kind;
        lhs_name = o.lhs_name;
        rhs      = o.rhs ? std::make_unique<RtlExpr>(*o.rhs) : nullptr;
        return *this;
    }

    // move
    RtlAssign(RtlAssign &&) noexcept = default;
    RtlAssign &operator=(RtlAssign &&) noexcept = default;
};

// ----------------------
// Processes
// ----------------------

enum class RtlProcessKind {
    Initial,
    Always
};

struct RtlProcess {
    RtlProcessKind kind{};
    std::vector<RtlAssign> assigns;

    // For event‑driven scheduling: list of signal names this process is sensitive to.
    // For @* / always_comb, this can be left empty and treated as "combinational".
    std::vector<std::string> sensitivity_signals;

    RtlProcess() = default;

    // deep copy
    RtlProcess(const RtlProcess &o)
        : kind(o.kind),
          assigns(o.assigns),
          sensitivity_signals(o.sensitivity_signals) {}

    RtlProcess &operator=(const RtlProcess &o) {
        if (this == &o) return *this;
        kind               = o.kind;
        assigns            = o.assigns;
        sensitivity_signals = o.sensitivity_signals;
        return *this;
    }

    // move
    RtlProcess(RtlProcess &&) noexcept = default;
    RtlProcess &operator=(RtlProcess &&) noexcept = default;
};

// ----------------------
// Nets, Params
// ----------------------

struct RtlNet {
    std::string name;
    DataType type;
};

struct RtlParam {
    std::string name;
    std::string value_str;
};

// ----------------------
// Gate‑level primitives
// ----------------------

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
    RtlGateKind kind;
    std::string out;
    std::vector<std::string> inputs;
};

// ----------------------
// Instances
// ----------------------

struct RtlInstanceConn {
    std::string port_name;
    std::string signal_name;
};

struct RtlInstance {
    std::string module_name;
    std::string instance_name;
    std::vector<RtlInstanceConn> conns;

    RtlInstance() = default;

    RtlInstance(const RtlInstance &) = default;
    RtlInstance &operator=(const RtlInstance &) = default;

    RtlInstance(RtlInstance &&) noexcept = default;
    RtlInstance &operator=(RtlInstance &&) noexcept = default;
};

// ----------------------
// Module
// ----------------------

struct RtlModule {
    std::string name;
    std::vector<RtlParam> params;
    std::vector<RtlNet> nets;
    std::vector<RtlProcess> processes;
    std::vector<RtlAssign> continuous_assigns;
    std::vector<RtlInstance> instances;

    // Gate‑level primitives (populated later from synth/netlist)
    std::vector<RtlGate> gates;

    RtlModule() = default;

    RtlModule(const RtlModule &) = default;
    RtlModule &operator=(const RtlModule &) = default;

    RtlModule(RtlModule &&) noexcept = default;
    RtlModule &operator=(RtlModule &&) noexcept = default;
};

// ----------------------
// Design
// ----------------------

struct RtlDesign {
    std::vector<RtlModule> modules;

    RtlDesign() = default;
    RtlDesign(const RtlDesign &) = default;
    RtlDesign &operator=(const RtlDesign &) = default;

    RtlDesign(RtlDesign &&) noexcept = default;
    RtlDesign &operator=(RtlDesign &&) noexcept = default;
};

} // namespace sv

#endif // __RTL_IR_HPP__
