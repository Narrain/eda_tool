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
    Binary
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

    RtlExpr() = default;
    explicit RtlExpr(RtlExprKind k) : kind(k) {}

    RtlExpr(const RtlExpr &o)
        : kind(o.kind),
          ref_name(o.ref_name),
          const_literal(o.const_literal),
          un_op(o.un_op),
          bin_op(o.bin_op)
    {
        if (o.un_operand)
            un_operand = std::make_unique<RtlExpr>(*o.un_operand);
        if (o.lhs)
            lhs = std::make_unique<RtlExpr>(*o.lhs);
        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
    }

    RtlExpr &operator=(const RtlExpr &o) {
        if (this == &o) return *this;
        kind          = o.kind;
        ref_name      = o.ref_name;
        const_literal = o.const_literal;
        un_op         = o.un_op;
        bin_op        = o.bin_op;

        if (o.un_operand)
            un_operand = std::make_unique<RtlExpr>(*o.un_operand);
        else
            un_operand.reset();

        if (o.lhs)
            lhs = std::make_unique<RtlExpr>(*o.lhs);
        else
            lhs.reset();

        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
        else
            rhs.reset();

        return *this;
    }

    std::unique_ptr<RtlExpr> clone() const {
        return std::make_unique<RtlExpr>(*this);
    }
};

// ============================================================================
// Statements
// ============================================================================

enum class RtlStmtKind {
    BlockingAssign,
    NonBlockingAssign,
    Delay,
    Finish
};

struct RtlStmt {
    RtlStmtKind kind = RtlStmtKind::BlockingAssign;

    std::string lhs_name;
    std::unique_ptr<RtlExpr> rhs;

    std::unique_ptr<RtlExpr> delay_expr;
    RtlStmt *delay_stmt = nullptr;

    RtlStmt *next = nullptr;

    RtlStmt() = default;

    RtlStmt(const RtlStmt &o)
        : kind(o.kind),
          lhs_name(o.lhs_name)
    {
        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
        if (o.delay_expr)
            delay_expr = std::make_unique<RtlExpr>(*o.delay_expr);

        delay_stmt = nullptr;
        next       = nullptr;
    }

    RtlStmt &operator=(const RtlStmt &o) {
        if (this == &o) return *this;
        kind     = o.kind;
        lhs_name = o.lhs_name;

        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
        else
            rhs.reset();

        if (o.delay_expr)
            delay_expr = std::make_unique<RtlExpr>(*o.delay_expr);
        else
            delay_expr.reset();

        delay_stmt = nullptr;
        next       = nullptr;
        return *this;
    }
};

// ============================================================================
// Assigns
// ============================================================================

enum class RtlAssignKind {
    Continuous,
    Blocking,
    NonBlocking
};

struct RtlAssign {
    RtlAssignKind kind = RtlAssignKind::Continuous;
    std::string   lhs_name;
    std::unique_ptr<RtlExpr> rhs;

    RtlAssign() = default;

    RtlAssign(const RtlAssign &o)
        : kind(o.kind),
          lhs_name(o.lhs_name)
    {
        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
    }

    RtlAssign &operator=(const RtlAssign &o) {
        if (this == &o) return *this;
        kind     = o.kind;
        lhs_name = o.lhs_name;
        if (o.rhs)
            rhs = std::make_unique<RtlExpr>(*o.rhs);
        else
            rhs.reset();
        return *this;
    }
};

// ============================================================================
// Processes
// ============================================================================

enum class RtlProcessKind {
    Always,
    Initial
};

struct RtlSensitivity {
    enum class Kind {
        Level,
        Posedge,
        Negedge
    };

    Kind        kind   = Kind::Level;
    std::string signal;
};


struct RtlProcess {
    RtlProcessKind kind = RtlProcessKind::Always;

    std::vector<RtlAssign> assigns;

    RtlStmt *first_stmt = nullptr;
    std::vector<std::unique_ptr<RtlStmt>> stmts;

    std::vector<RtlSensitivity> sensitivity;

    RtlProcess() = default;

    RtlProcess(const RtlProcess &o)
        : kind(o.kind),
          assigns(o.assigns),
          first_stmt(nullptr),
          sensitivity(o.sensitivity)
    {
        stmts.clear();
        stmts.reserve(o.stmts.size());
        for (const auto &sp : o.stmts) {
            if (sp)
                stmts.push_back(std::make_unique<RtlStmt>(*sp));
            else
                stmts.push_back(nullptr);
        }

        if (o.first_stmt) {
            for (std::size_t i = 0; i < o.stmts.size(); ++i) {
                if (o.stmts[i].get() == o.first_stmt) {
                    first_stmt = stmts[i].get();
                    break;
                }
            }
        }

        for (std::size_t i = 0; i < o.stmts.size(); ++i) {
            const RtlStmt *orig = o.stmts[i].get();
            RtlStmt *clone      = stmts[i].get();
            if (!orig || !clone) continue;

            if (orig->next) {
                for (std::size_t j = 0; j < o.stmts.size(); ++j) {
                    if (o.stmts[j].get() == orig->next) {
                        clone->next = stmts[j].get();
                        break;
                    }
                }
            } else {
                clone->next = nullptr;
            }

            clone->delay_stmt = nullptr;
        }
    }

    RtlProcess &operator=(const RtlProcess &o) {
        if (this == &o) return *this;
        kind        = o.kind;
        assigns     = o.assigns;
        sensitivity = o.sensitivity;

        stmts.clear();
        stmts.reserve(o.stmts.size());
        for (const auto &sp : o.stmts) {
            if (sp)
                stmts.push_back(std::make_unique<RtlStmt>(*sp));
            else
                stmts.push_back(nullptr);
        }

        first_stmt = nullptr;
        if (o.first_stmt) {
            for (std::size_t i = 0; i < o.stmts.size(); ++i) {
                if (o.stmts[i].get() == o.first_stmt) {
                    first_stmt = stmts[i].get();
                    break;
                }
            }
        }

        for (std::size_t i = 0; i < o.stmts.size(); ++i) {
            const RtlStmt *orig = o.stmts[i].get();
            RtlStmt *clone      = stmts[i].get();
            if (!orig || !clone) continue;

            if (orig->next) {
                for (std::size_t j = 0; j < o.stmts.size(); ++j) {
                    if (o.stmts[j].get() == orig->next) {
                        clone->next = stmts[j].get();
                        break;
                    }
                }
            } else {
                clone->next = nullptr;
            }

            clone->delay_stmt = nullptr;
        }

        return *this;
    }
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
    std::vector<std::string> inputs;
    std::string out;

    RtlGate() = default;
    RtlGate(const RtlGate &) = default;
    RtlGate &operator=(const RtlGate &) = default;
};

// ============================================================================
// Params, instances
// ============================================================================

struct RtlParam {
    std::string name;

    // Old style used by ir_builder: literal string
    std::string value_str;

    // Rich form: expression value (not currently used by ir_builder)
    std::unique_ptr<RtlExpr> value;

    RtlParam() = default;

    RtlParam(const RtlParam &o)
        : name(o.name),
          value_str(o.value_str)
    {
        if (o.value)
            value = std::make_unique<RtlExpr>(*o.value);
    }

    RtlParam &operator=(const RtlParam &o) {
        if (this == &o) return *this;
        name      = o.name;
        value_str = o.value_str;
        if (o.value)
            value = std::make_unique<RtlExpr>(*o.value);
        else
            value.reset();
        return *this;
    }
};

struct RtlInstanceConn {
    std::string port_name;

    // Old style used by ir_builder: just a signal name
    std::string signal_name;

    // Rich form: expression connection (not currently used by ir_builder)
    std::unique_ptr<RtlExpr> expr;

    RtlInstanceConn() = default;

    RtlInstanceConn(const RtlInstanceConn &o)
        : port_name(o.port_name),
          signal_name(o.signal_name)
    {
        if (o.expr)
            expr = std::make_unique<RtlExpr>(*o.expr);
    }

    RtlInstanceConn &operator=(const RtlInstanceConn &o) {
        if (this == &o) return *this;
        port_name   = o.port_name;
        signal_name = o.signal_name;
        if (o.expr)
            expr = std::make_unique<RtlExpr>(*o.expr);
        else
            expr.reset();
        return *this;
    }
};

struct RtlInstance {
    std::string module_name;

    // Name as used by ir_builder
    std::string instance_name;

    // Alternate name (kept for future use / compatibility)
    std::string inst_name;

    // Old style used by ir_builder
    std::vector<RtlInstanceConn> conns;

    // Rich form (not currently used by ir_builder)
    std::vector<RtlInstanceConn> connections;

    RtlInstance() = default;

    RtlInstance(const RtlInstance &o)
        : module_name(o.module_name),
          instance_name(o.instance_name),
          inst_name(o.inst_name),
          conns(o.conns),
          connections(o.connections)
    {}

    RtlInstance &operator=(const RtlInstance &o) {
        if (this == &o) return *this;
        module_name  = o.module_name;
        instance_name = o.instance_name;
        inst_name     = o.inst_name;
        conns         = o.conns;
        connections   = o.connections;
        return *this;
    }
};

// ============================================================================
// Nets, modules, design
// ============================================================================

struct RtlNet {
    std::string name;
    DataType    type;   // from frontend/ast.hpp
};

struct RtlModule {
    std::string name;

    std::vector<RtlParam>    params;
    std::vector<RtlNet>      nets;
    std::vector<RtlAssign>   continuous_assigns;
    std::vector<RtlProcess>  processes;
    std::vector<RtlGate>     gates;
    std::vector<RtlInstance> instances;

    RtlModule() = default;

    RtlModule(const RtlModule &o)
        : name(o.name),
          params(o.params),
          nets(o.nets),
          continuous_assigns(o.continuous_assigns),
          processes(o.processes),
          gates(o.gates),
          instances(o.instances)
    {}

    RtlModule &operator=(const RtlModule &o) {
        if (this == &o) return *this;
        name               = o.name;
        params             = o.params;
        nets               = o.nets;
        continuous_assigns = o.continuous_assigns;
        processes          = o.processes;
        gates              = o.gates;
        instances          = o.instances;
        return *this;
    }
};

struct RtlDesign {
    std::vector<RtlModule> modules;
};

} // namespace sv

#endif // SV_RTL_IR_HPP
