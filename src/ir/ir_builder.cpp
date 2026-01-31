// src/ir/ir_builder.cpp
#include "ir_builder.hpp"
#include "../frontend/const_eval.hpp"

#include <iostream>
#include <functional>
#include <unordered_set>

namespace sv {

static RtlBinOp map_bin_op(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:      return RtlBinOp::Add;
    case BinaryOp::Sub:      return RtlBinOp::Sub;
    case BinaryOp::Mul:      return RtlBinOp::Mul;
    case BinaryOp::Div:      return RtlBinOp::Div;
    case BinaryOp::Mod:      return RtlBinOp::Mod;

    case BinaryOp::BitAnd:   return RtlBinOp::And;
    case BinaryOp::BitOr:    return RtlBinOp::Or;
    case BinaryOp::BitXor:   return RtlBinOp::Xor;

    case BinaryOp::Eq:       return RtlBinOp::Eq;
    case BinaryOp::Neq:      return RtlBinOp::Neq;
    case BinaryOp::CaseEq:   return RtlBinOp::CaseEq;
    case BinaryOp::CaseNeq:  return RtlBinOp::CaseNeq;
    case BinaryOp::Lt:       return RtlBinOp::Lt;
    case BinaryOp::Gt:       return RtlBinOp::Gt;
    case BinaryOp::Le:       return RtlBinOp::Le;
    case BinaryOp::Ge:       return RtlBinOp::Ge;

    case BinaryOp::LogicalAnd: return RtlBinOp::LogicalAnd;
    case BinaryOp::LogicalOr:  return RtlBinOp::LogicalOr;

    case BinaryOp::Shl:      return RtlBinOp::Shl;
    case BinaryOp::Shr:      return RtlBinOp::Shr;
    case BinaryOp::Ashl:     return RtlBinOp::Ashl;
    case BinaryOp::Ashr:     return RtlBinOp::Ashr;

    default:
        return RtlBinOp::Add;
    }
}

static RtlUnOp map_un_op(UnaryOp op) {
    switch (op) {
    case UnaryOp::Plus:        return RtlUnOp::Plus;
    case UnaryOp::Minus:       return RtlUnOp::Minus;
    case UnaryOp::LogicalNot:  return RtlUnOp::Not;
    case UnaryOp::BitNot:      return RtlUnOp::BitNot;
    }
    return RtlUnOp::Plus;
}

static bool is_finish_call(const Expression *e) {
    return e && e->kind == ExprKind::Identifier && e->ident == "$finish";
}

RtlDesign IRBuilder::build() {
    RtlDesign out;
    for (const auto &m : design_.modules) {
        out.modules.push_back(buildModule(*m));
    }
    return out;
}

RtlModule IRBuilder::buildModule(const ModuleDecl &mod) {
    RtlModule out;
    out.name = mod.name;

    collectParams(mod, out);
    collectNets(mod, out);
    collectContinuousAssigns(mod, out);
    collectProcesses(mod, out);
    collectInstances(mod, out);

    return out;
}

void IRBuilder::collectParams(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &p : mod.params) {
        RtlParam rp;
        rp.name = p->name;
        if (p->value && p->value->kind == ExprKind::Number)
            rp.value_str = p->value->literal;
        out.params.push_back(std::move(rp));
    }
}

void IRBuilder::collectNets(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::NetDecl && item->net_decl) {
            RtlNet n;
            n.name = item->net_decl->name;
            n.type = item->net_decl->type;
            out.nets.push_back(std::move(n));
        } else if (item->kind == ModuleItemKind::VarDecl && item->var_decl) {
            RtlNet n;
            n.name = item->var_decl->name;
            n.type = item->var_decl->type;
            out.nets.push_back(std::move(n));
        }
    }
}

void IRBuilder::collectContinuousAssigns(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind != ModuleItemKind::ContinuousAssign || !item->cont_assign)
            continue;

        const auto &ca = *item->cont_assign;
        if (!ca.lhs || ca.lhs->kind != ExprKind::Identifier)
            continue;

        RtlAssign a;
        a.kind     = RtlAssignKind::Continuous;
        a.lhs_name = ca.lhs->ident;
        a.rhs      = lowerExpr(*ca.rhs);
        out.continuous_assigns.push_back(std::move(a));
    }
}

void IRBuilder::collectProcesses(const ModuleDecl &mod, RtlModule &out) {
    const auto &em = elab_.modules.at(mod.name);

    for (const ModuleItem *item : em.flat_items) {
        switch (item->kind) {
        case ModuleItemKind::Always:
            if (item->always)
                collectProcessFromAlways(*item->always, out);
            break;
        case ModuleItemKind::Initial:
            if (item->initial)
                collectProcessFromInitial(*item->initial, out);
            break;
        default:
            break;
        }
    }
}

void IRBuilder::collectProcessFromAlways(const AlwaysConstruct &ac, RtlModule &out) {
    RtlProcess p;
    p.kind = RtlProcessKind::Always;

    for (const auto &si : ac.sensitivity_list) {
        if (si.star) {
            RtlSensitivity s;
            s.kind   = RtlSensitivity::Kind::Level;
            s.signal = "*";
            p.sensitivity.push_back(std::move(s));
        } else if (si.expr && si.expr->kind == ExprKind::Identifier) {
            RtlSensitivity s;
            s.signal = si.expr->ident;
            if (si.posedge)
                s.kind = RtlSensitivity::Kind::Posedge;
            else if (si.negedge)
                s.kind = RtlSensitivity::Kind::Negedge;
            else
                s.kind = RtlSensitivity::Kind::Level;
            p.sensitivity.push_back(std::move(s));
        }
    }

    if (ac.body)
        p.first_stmt = build_proc_body(*ac.body, p);

    out.processes.push_back(std::move(p));
}

void IRBuilder::collectProcessFromInitial(const InitialConstruct &ic, RtlModule &out) {
    RtlProcess p;
    p.kind = RtlProcessKind::Initial;

    if (ic.body)
        p.first_stmt = build_proc_body(*ic.body, p);

    out.processes.push_back(std::move(p));
}

void IRBuilder::collectInstances(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind != ModuleItemKind::Instance || !item->instance)
            continue;

        const auto &inst = *item->instance;
        RtlInstance ri;
        ri.module_name   = inst.module_name;
        ri.instance_name = inst.instance_name;

        for (const auto &pc : inst.port_conns) {
            RtlInstanceConn c;
            c.port_name = pc.port_name;
            if (pc.expr && pc.expr->kind == ExprKind::Identifier)
                c.signal_name = pc.expr->ident;
            ri.conns.push_back(std::move(c));
        }

        out.instances.push_back(std::move(ri));
    }
}

std::unique_ptr<RtlExpr> IRBuilder::lowerExpr(const Expression &e) {
    switch (e.kind) {
    case ExprKind::Identifier: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Ref);
        r->ref_name = e.ident;
        return r;
    }
    case ExprKind::Number: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Const);
        r->const_literal = e.literal;
        return r;
    }
    case ExprKind::Unary: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Unary);
        r->un_op = map_un_op(e.unary_op);
        if (e.unary_operand)
            r->un_operand = lowerExpr(*e.unary_operand);
        return r;
    }
    case ExprKind::Binary: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        r->bin_op = map_bin_op(e.binary_op);
        if (e.lhs)
            r->lhs = lowerExpr(*e.lhs);
        if (e.rhs)
            r->rhs = lowerExpr(*e.rhs);
        return r;
    }
    default: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Const);
        r->const_literal = "0";
        return r;
    }
    }
}

RtlAssign IRBuilder::lowerAssign(const Statement &s, RtlAssignKind kind) {
    RtlAssign a;
    a.kind = kind;

    if (s.lhs && s.lhs->kind == ExprKind::Identifier)
        a.lhs_name = s.lhs->ident;
    if (s.rhs)
        a.rhs = lowerExpr(*s.rhs);

    return a;
}

static void append_stmt(RtlProcess &p, RtlStmt *&head, RtlStmt *&tail, std::unique_ptr<RtlStmt> ns) {
    RtlStmt *raw = ns.get();
    p.stmts.push_back(std::move(ns));
    if (!head) {
        head = tail = raw;
    } else {
        tail->next = raw;
        tail = raw;
    }
}

RtlStmt* IRBuilder::build_proc_body(const Statement &body, RtlProcess &p) {
    RtlStmt *head = nullptr;
    RtlStmt *tail = nullptr;

    std::unordered_set<const Statement*> visited;

    std::function<void(const Statement&)> build_stmt =
        [&](const Statement &s) {
            if (visited.find(&s) != visited.end())
                return;
            visited.insert(&s);

            switch (s.kind) {

            case StmtKind::Block: {
                for (const auto &sub : s.block_stmts) {
                    if (sub)
                        build_stmt(*sub);
                }
                break;
            }

            case StmtKind::BlockingAssign: {
                if (s.delay_expr) {
                    auto d = std::make_unique<RtlStmt>();
                    d->kind = RtlStmtKind::Delay;
                    d->delay_expr = lowerExpr(*s.delay_expr);
                    append_stmt(p, head, tail, std::move(d));
                }

                auto ns = std::make_unique<RtlStmt>();
                ns->kind = RtlStmtKind::BlockingAssign;
                if (s.lhs && s.lhs->kind == ExprKind::Identifier)
                    ns->lhs_name = s.lhs->ident;
                if (s.rhs)
                    ns->rhs = lowerExpr(*s.rhs);
                append_stmt(p, head, tail, std::move(ns));
                break;
            }

            case StmtKind::NonBlockingAssign: {
                if (s.delay_expr) {
                    auto d = std::make_unique<RtlStmt>();
                    d->kind = RtlStmtKind::Delay;
                    d->delay_expr = lowerExpr(*s.delay_expr);
                    append_stmt(p, head, tail, std::move(d));
                }

                auto ns = std::make_unique<RtlStmt>();
                ns->kind = RtlStmtKind::NonBlockingAssign;
                if (s.lhs && s.lhs->kind == ExprKind::Identifier)
                    ns->lhs_name = s.lhs->ident;
                if (s.rhs)
                    ns->rhs = lowerExpr(*s.rhs);
                append_stmt(p, head, tail, std::move(ns));
                break;
            }

            case StmtKind::Delay: {
                auto ns = std::make_unique<RtlStmt>();
                ns->kind = RtlStmtKind::Delay;
                if (s.delay_expr)
                    ns->delay_expr = lowerExpr(*s.delay_expr);
                append_stmt(p, head, tail, std::move(ns));
                if (s.delay_stmt)
                    build_stmt(*s.delay_stmt);
                break;
            }

            case StmtKind::ExprStmt: {
                if (is_finish_call(s.expr.get())) {
                    auto ns = std::make_unique<RtlStmt>();
                    ns->kind = RtlStmtKind::Finish;
                    append_stmt(p, head, tail, std::move(ns));
                }
                break;
            }

            case StmtKind::If: {
                if (s.if_then)
                    build_stmt(*s.if_then);
                if (s.if_else)
                    build_stmt(*s.if_else);
                break;
            }

            case StmtKind::Case: {
                for (const auto &ci : s.case_items) {
                    if (ci.stmt)
                        build_stmt(*ci.stmt);
                }
                break;
            }

            case StmtKind::Null:
            default:
                break;
            }
        };

    build_stmt(body);
    return head;
}

void dump_rtl_module(const RtlModule &m) {
    std::cout << "RTL Module: " << m.name << "\n";

    std::cout << "  Nets:\n";
    for (const auto &n : m.nets)
        std::cout << "    " << n.name << "\n";

    std::cout << "  Continuous assigns:\n";
    for (const auto &a : m.continuous_assigns)
        std::cout << "    " << a.lhs_name << " = ...\n";

    std::cout << "  Processes:\n";
    for (const auto &p : m.processes) {
        std::cout << "    Process kind=" << (p.kind == RtlProcessKind::Always ? "always" : "initial")
                  << " sens=";
        for (const auto &s : p.sensitivity) {
            char k = (s.kind == RtlSensitivity::Kind::Posedge ? '+' :
                      s.kind == RtlSensitivity::Kind::Negedge ? '-' : ' ');
            std::cout << k << s.signal << " ";
        }
        std::cout << "\n";

        const RtlStmt *s = p.first_stmt;
        int idx = 0;
        std::unordered_set<const RtlStmt*> visited;

        while (s && visited.find(s) == visited.end() && idx < 1024) {
            visited.insert(s);

            std::cout << "      stmt[" << idx++ << "]: ";
            switch (s->kind) {
            case RtlStmtKind::BlockingAssign:
                std::cout << "BA " << s->lhs_name << " = ...\n";
                break;
            case RtlStmtKind::NonBlockingAssign:
                std::cout << "NBA " << s->lhs_name << " <= ...\n";
                break;
            case RtlStmtKind::Delay:
                std::cout << "DELAY #(...)\n";
                break;
            case RtlStmtKind::Finish:
                std::cout << "FINISH\n";
                break;
            }
            s = s->next;
        }
    }
}

} // namespace sv
