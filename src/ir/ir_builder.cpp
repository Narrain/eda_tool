#include "ir_builder.hpp"

namespace sv {

RtlDesign IRBuilder::build() {
    RtlDesign rd;
    for (const auto &m : design_.modules) {
        rd.modules.push_back(buildModule(*m));
    }
    return rd;
}

RtlModule IRBuilder::buildModule(const ModuleDecl &mod) {
    RtlModule rm;
    rm.name = mod.name;

    // 1) Look up the elaborated module
    auto it = elab_.modules.find(mod.name);
    if (it == elab_.modules.end()) {
        // fallback: no elaborated version, use raw AST
        collectParams(mod, rm);
        collectNets(mod, rm);
        collectContinuousAssigns(mod, rm);
        collectProcesses(mod, rm);
        collectInstances(mod, rm);
        return rm;
    }

    const ElabModule &em = it->second;

    // 2) Params (from elaborated design)
    for (const auto &p : em.params) {
        RtlParam rp;
        rp.name = p.name;
        rp.value_str = p.value_str;
        rm.params.push_back(std::move(rp));
    }

    // 3) Nets (from elaborated design)
    for (const auto &n : em.nets) {
        RtlNet rn;
        rn.name = n.name;
        rn.type = n.type;
        rm.nets.push_back(std::move(rn));
    }

    // 4) Instances (from elaborated design)
    for (const auto &inst : em.instances) {
        RtlInstance ri;
        ri.module_name = inst.module_name;
        ri.instance_name = inst.instance_name;

        for (const auto &pc : inst.port_conns) {
            RtlInstanceConn c;
            c.port_name = pc.first;
            c.signal_name = pc.second;
            ri.conns.push_back(std::move(c));
        }

        rm.instances.push_back(std::move(ri));
    }

    // 5) Processes + continuous assigns
    //    These still come from the AST, but now generate-for is already expanded
    collectProcesses(mod, rm);
    collectContinuousAssigns(mod, rm);

    return rm;
}



void IRBuilder::collectParams(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::ParamDecl && item->param_decl) {
            RtlParam p;
            p.name = item->param_decl->name;
            if (item->param_decl->value &&
                item->param_decl->value->kind == ExprKind::Number) {
                // new AST: use .literal instead of .number_literal
                p.value_str = item->param_decl->value->literal;
            } else {
                p.value_str = "<expr>";
            }
            out.params.push_back(std::move(p));
        }
    }
}

void IRBuilder::collectNets(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::NetDecl && item->net_decl) {
            RtlNet n;
            n.name = item->net_decl->name;
            n.type = item->net_decl->type;
            out.nets.push_back(std::move(n));

            // Declaration initializer → Initial process
            if (item->net_decl->init) {
                RtlProcess p;
                p.kind = RtlProcessKind::Initial;

                RtlAssign a;
                a.kind = RtlAssignKind::Blocking;
                a.lhs_name = item->net_decl->name;
                a.rhs = lowerExpr(*item->net_decl->init);

                p.assigns.push_back(std::move(a));
                out.processes.push_back(std::move(p));
            }

        } else if (item->kind == ModuleItemKind::VarDecl && item->var_decl) {
            RtlNet n;
            n.name = item->var_decl->name;
            n.type = item->var_decl->type;
            out.nets.push_back(std::move(n));

            // Declaration initializer → Initial process
            if (item->var_decl->init) {
                RtlProcess p;
                p.kind = RtlProcessKind::Initial;

                RtlAssign a;
                a.kind = RtlAssignKind::Blocking;
                a.lhs_name = item->var_decl->name;
                a.rhs = lowerExpr(*item->var_decl->init);

                p.assigns.push_back(std::move(a));
                out.processes.push_back(std::move(p));
            }
        }
    }
}


void IRBuilder::collectContinuousAssigns(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::ContinuousAssign && item->cont_assign) {
            RtlAssign a;
            a.kind = RtlAssignKind::Continuous;
            if (item->cont_assign->lhs &&
                item->cont_assign->lhs->kind == ExprKind::Identifier) {
                a.lhs_name = item->cont_assign->lhs->ident;
            } else {
                a.lhs_name = "<expr>";
            }
            if (item->cont_assign->rhs) {
                a.rhs = lowerExpr(*item->cont_assign->rhs);
            }
            out.continuous_assigns.push_back(std::move(a));
        }
    }
}

void IRBuilder::collectProcesses(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {

        // -------------------------
        // Always constructs
        // -------------------------
        if (item->kind == ModuleItemKind::Always && item->always) {
            const auto &ac = *item->always;

            RtlProcess p;
            p.kind = RtlProcessKind::Always;

            // 1) Sensitivity list
            const auto &alist = ac.sensitivity_list;

            if (!alist.empty()) {
                for (const auto &si : alist) {

                    // @* or @(*)
                    if (si.star) {
                        // leave p.sensitivity_signals empty
                        // kernel will infer from RHS
                        continue;
                    }

                    if (!si.expr) continue;

                    // Identifier: @a, @(posedge a), @(negedge a)
                    if (si.expr->kind == ExprKind::Identifier) {
                        const std::string &name = si.expr->ident;

                        if (si.posedge) {
                            // edge-sensitive: handled in kernel via posedge_watchers_
                            // we still record as level sensitivity for now
                            p.sensitivity_signals.push_back(name);
                        } else if (si.negedge) {
                            p.sensitivity_signals.push_back(name);
                        } else {
                            // level-sensitive @a
                            p.sensitivity_signals.push_back(name);
                        }
                        continue;
                    }

                    // OR-chain: @(a or b or c)
                    if (si.expr->kind == ExprKind::Binary &&
                        si.expr->binary_op == BinaryOp::LogicalOr &&
                        !si.posedge && !si.negedge) {

                        auto walk_or = [&](const Expression *e,
                                           const auto &self_ref) -> void {
                            if (!e) return;

                            if (e->kind == ExprKind::Identifier) {
                                p.sensitivity_signals.push_back(e->ident);
                                return;
                            }

                            if (e->kind == ExprKind::Binary &&
                                e->binary_op == BinaryOp::LogicalOr) {
                                self_ref(e->lhs.get(), self_ref);
                                self_ref(e->rhs.get(), self_ref);
                                return;
                            }
                        };

                        walk_or(si.expr.get(), walk_or);
                        continue;
                    }

                    // other forms ignored for now
                }
            } else if (ac.kind == AlwaysKind::AlwaysComb) {
                // always_comb with no explicit list => @(*)
                // leave p.sensitivity_signals empty; kernel uses RHS deps
            }

            // 2) Body flattening with semantics:
            //    - always_comb => blocking
            //    - always_ff   => nonblocking
            //    - plain always => as written

            auto lower_body = [&](const Statement &body, RtlAssignKind default_kind) {
                if (body.kind == StmtKind::BlockingAssign) {
                    p.assigns.push_back(lowerAssign(body, RtlAssignKind::Blocking));
                } else if (body.kind == StmtKind::NonBlockingAssign) {
                    p.assigns.push_back(lowerAssign(body, RtlAssignKind::NonBlocking));
                } else if (body.kind == StmtKind::Block) {
                    for (const auto &s : body.block_stmts) {
                        if (!s) continue;
                        if (s->kind == StmtKind::BlockingAssign) {
                            p.assigns.push_back(lowerAssign(*s, RtlAssignKind::Blocking));
                        } else if (s->kind == StmtKind::NonBlockingAssign) {
                            p.assigns.push_back(lowerAssign(*s, RtlAssignKind::NonBlocking));
                        }
                    }
                }
            };

            if (ac.body) {
                const Statement &body = *ac.body;

                switch (ac.kind) {
                case AlwaysKind::AlwaysComb:
                    // force blocking semantics
                    lower_body(body, RtlAssignKind::Blocking);
                    break;

                case AlwaysKind::AlwaysFF:
                    // force nonblocking semantics
                    lower_body(body, RtlAssignKind::NonBlocking);
                    break;

                case AlwaysKind::Always:
                case AlwaysKind::AlwaysLatch:
                default:
                    // as written
                    lower_body(body, RtlAssignKind::Blocking);
                    break;
                }
            }

            out.processes.push_back(std::move(p));
        }

        // -------------------------
        // Initial constructs
        // -------------------------
        else if (item->kind == ModuleItemKind::Initial && item->initial) {
            const auto &ic = *item->initial;

            RtlProcess p;
            p.kind = RtlProcessKind::Initial;

            if (ic.body) {
                const Statement &body = *ic.body;

                if (body.kind == StmtKind::BlockingAssign) {
                    p.assigns.push_back(lowerAssign(body, RtlAssignKind::Blocking));
                } else if (body.kind == StmtKind::NonBlockingAssign) {
                    p.assigns.push_back(lowerAssign(body, RtlAssignKind::NonBlocking));
                } else if (body.kind == StmtKind::Block) {
                    for (const auto &s : body.block_stmts) {
                        if (!s) continue;
                        if (s->kind == StmtKind::BlockingAssign) {
                            p.assigns.push_back(lowerAssign(*s, RtlAssignKind::Blocking));
                        } else if (s->kind == StmtKind::NonBlockingAssign) {
                            p.assigns.push_back(lowerAssign(*s, RtlAssignKind::NonBlocking));
                        }
                    }
                }
            }

            out.processes.push_back(std::move(p));
        }
    }
}

void IRBuilder::collectInstances(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::Instance && item->instance) {
            RtlInstance ri;
            ri.module_name = item->instance->module_name;
            ri.instance_name = item->instance->instance_name;
            for (const auto &pc : item->instance->port_conns) {
                RtlInstanceConn c;
                c.port_name = pc.port_name;
                if (pc.expr && pc.expr->kind == ExprKind::Identifier) {
                    c.signal_name = pc.expr->ident;
                } else {
                    c.signal_name = "<expr>";
                }
                ri.conns.push_back(std::move(c));
            }
            out.instances.push_back(std::move(ri));
        }
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
        // new AST: .literal
        r->const_literal = e.literal;
        return r;
    }
    case ExprKind::Binary: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        // map BinaryOp to RtlBinOp
        switch (e.binary_op) {
        case BinaryOp::Add:        r->bin_op = RtlBinOp::Add;        break;
        case BinaryOp::Sub:        r->bin_op = RtlBinOp::Sub;        break;
        case BinaryOp::Mul:        r->bin_op = RtlBinOp::Mul;        break;
        case BinaryOp::Div:        r->bin_op = RtlBinOp::Div;        break;
        case BinaryOp::Mod:        r->bin_op = RtlBinOp::Mod;        break;

        case BinaryOp::BitAnd:     r->bin_op = RtlBinOp::And;        break;
        case BinaryOp::BitOr:      r->bin_op = RtlBinOp::Or;         break;
        case BinaryOp::BitXor:     r->bin_op = RtlBinOp::Xor;        break;

        case BinaryOp::LogicalAnd: r->bin_op = RtlBinOp::LogicalAnd; break;
        case BinaryOp::LogicalOr:  r->bin_op = RtlBinOp::LogicalOr;  break;

        case BinaryOp::Eq:         r->bin_op = RtlBinOp::Eq;         break;
        case BinaryOp::Neq:        r->bin_op = RtlBinOp::Neq;        break;
        case BinaryOp::CaseEq:     r->bin_op = RtlBinOp::CaseEq;     break;
        case BinaryOp::CaseNeq:    r->bin_op = RtlBinOp::CaseNeq;    break;
        case BinaryOp::Lt:         r->bin_op = RtlBinOp::Lt;         break;
        case BinaryOp::Gt:         r->bin_op = RtlBinOp::Gt;         break;
        case BinaryOp::Le:         r->bin_op = RtlBinOp::Le;         break;
        case BinaryOp::Ge:         r->bin_op = RtlBinOp::Ge;         break;

        case BinaryOp::Shl:        r->bin_op = RtlBinOp::Shl;        break;
        case BinaryOp::Shr:        r->bin_op = RtlBinOp::Shr;        break;
        case BinaryOp::Ashl:       r->bin_op = RtlBinOp::Ashl;       break;
        case BinaryOp::Ashr:       r->bin_op = RtlBinOp::Ashr;       break;

        default:
            r->bin_op = RtlBinOp::Add; // safe default; should not hit in RTL subset
            break;
        }
        if (e.lhs) r->lhs = lowerExpr(*e.lhs);
        if (e.rhs) r->rhs = lowerExpr(*e.rhs);
        return r;
    }
    case ExprKind::Unary: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Unary);
        switch (e.unary_op) {
        case UnaryOp::Plus:       r->un_op = RtlUnOp::Plus;   break;
        case UnaryOp::Minus:      r->un_op = RtlUnOp::Minus;  break;
        case UnaryOp::LogicalNot: r->un_op = RtlUnOp::Not;    break;
        case UnaryOp::BitNot:     r->un_op = RtlUnOp::BitNot; break;
        }
        if (e.unary_operand) r->un_operand = lowerExpr(*e.unary_operand);
        return r;
    }
    case ExprKind::Ternary: {
        // cond ? then_expr : else_expr
        // Lower into: (cond & then_expr) | (~cond & else_expr)

        // Lower subexpressions
        auto cond = lowerExpr(*e.cond);
        auto t    = lowerExpr(*e.then_expr);   // <-- correct field name
        auto f    = lowerExpr(*e.else_expr);   // <-- correct field name

        // (cond & t)
        auto and1 = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        and1->bin_op = RtlBinOp::And;
        and1->lhs = std::make_unique<RtlExpr>(*cond);
        and1->rhs = std::make_unique<RtlExpr>(*t);

        // ~cond
        auto notc = std::make_unique<RtlExpr>(RtlExprKind::Unary);
        notc->un_op = RtlUnOp::BitNot;
        notc->un_operand = std::move(cond);

        // (~cond & f)
        auto and2 = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        and2->bin_op = RtlBinOp::And;
        and2->lhs = std::move(notc);
        and2->rhs = std::move(f);

        // (cond & t) | (~cond & f)
        auto or_expr = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        or_expr->bin_op = RtlBinOp::Or;
        or_expr->lhs = std::move(and1);
        or_expr->rhs = std::move(and2);

        return or_expr;
    }

    case ExprKind::Concatenation:
    case ExprKind::Replication:
        // Not yet modeled in RtlExpr; placeholder
        return std::make_unique<RtlExpr>(RtlExprKind::Const);
    }

    return std::make_unique<RtlExpr>(RtlExprKind::Const);
}

RtlAssign IRBuilder::lowerAssign(const Statement &s, RtlAssignKind kind) {
    RtlAssign a;
    a.kind = kind;
    if (s.lhs && s.lhs->kind == ExprKind::Identifier) {
        a.lhs_name = s.lhs->ident;
    } else {
        a.lhs_name = "<expr>";
    }
    if (s.rhs) {
        a.rhs = lowerExpr(*s.rhs);
    }
    return a;
}


} // namespace sv
