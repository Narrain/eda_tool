#include "ir_builder.hpp"

#include <cstddef>
#include <utility>
#include <functional>

namespace sv {

    static std::string extract_lhs_name(const Expression *lhs) {
    if (!lhs) return "<expr>";

    if (lhs->kind == ExprKind::Identifier) {
        return lhs->ident;
    }

    if (lhs->kind == ExprKind::BitSelect) {
        // BitSelect is represented as lhs = base, rhs = index
        if (lhs->lhs && lhs->lhs->kind == ExprKind::Identifier) {
            return lhs->lhs->ident;   // r[i] → "r"
        }
    }

    return "<expr>";
}

// ---------------------------------------------------------------------
// Internal procedural builder: builds RtlStmt nodes with stable pointers
// ---------------------------------------------------------------------

namespace {

struct ProcStmtBuilder {
    // All nodes we create
    std::vector<std::unique_ptr<RtlStmt>> nodes;

    // Links to patch after nodes are placed
    enum class LinkKind { Next, Delay };

    struct Link {
        std::size_t from;
        std::size_t to;   // index into nodes, or npos for null
        LinkKind kind;
    };

    std::vector<Link> links;

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    std::size_t make_node() {
        nodes.push_back(std::make_unique<RtlStmt>());
        return nodes.size() - 1;
    }

    // Build a linear chain for 's', with 'tail' as the statement that
    // should follow this subtree. Returns the index of the head node,
    // or tail if nothing is created.
    std::size_t build_stmt_list(const Statement &s, std::size_t tail) {
        switch (s.kind) {

        case StmtKind::Null:
            return tail;

        case StmtKind::Block: {
            std::size_t local_tail = tail;
            for (auto it = s.block_stmts.rbegin(); it != s.block_stmts.rend(); ++it) {
                if (!*it) continue;
                local_tail = build_stmt_list(**it, local_tail);
            }
            return local_tail;
        }

        case StmtKind::BlockingAssign: {
            std::size_t idx = make_node();
            RtlStmt &n = *nodes[idx];
            n.kind = RtlStmtKind::BlockingAssign;

            const Expression *lhs = s.lhs.get();
            if (lhs && lhs->kind == ExprKind::Identifier) {
                n.lhs_name = lhs->ident;
            } else if (lhs &&
                       lhs->kind == ExprKind::BitSelect &&
                       lhs->lhs &&
                       lhs->lhs->kind == ExprKind::Identifier) {
                // r[i] = ...  → drive net "r"
                n.lhs_name = lhs->lhs->ident;
            } else {
                n.lhs_name = "<expr>";
            }

            // rhs filled later by IRBuilder
            links.push_back(Link{idx, tail, LinkKind::Next});
            return idx;
        }

        case StmtKind::NonBlockingAssign: {
            std::size_t idx = make_node();
            RtlStmt &n = *nodes[idx];
            n.kind = RtlStmtKind::NonBlockingAssign;

            const Expression *lhs = s.lhs.get();
            if (lhs && lhs->kind == ExprKind::Identifier) {
                n.lhs_name = lhs->ident;
            } else if (lhs &&
                       lhs->kind == ExprKind::BitSelect &&
                       lhs->lhs &&
                       lhs->lhs->kind == ExprKind::Identifier) {
                // r[i] <= ...  → drive net "r"
                n.lhs_name = lhs->lhs->ident;
            } else {
                n.lhs_name = "<expr>";
            }

            links.push_back(Link{idx, tail, LinkKind::Next});
            return idx;
        }

        case StmtKind::Delay: {
            std::size_t idx = make_node();
            RtlStmt &n = *nodes[idx];
            n.kind = RtlStmtKind::Delay;

            // delay_expr filled later

            std::size_t after = tail;
            if (s.delay_stmt) {
                after = build_stmt_list(*s.delay_stmt, tail);
            }

            links.push_back(Link{idx, after, LinkKind::Delay});
            links.push_back(Link{idx, tail,  LinkKind::Next});
            return idx;
        }

        case StmtKind::ExprStmt: {
            if (s.expr &&
                s.expr->kind == ExprKind::Identifier &&
                s.expr->ident == "$finish") {
                std::size_t idx = make_node();
                RtlStmt &n = *nodes[idx];
                n.kind = RtlStmtKind::Finish;

                links.push_back(Link{idx, tail, LinkKind::Next});
                return idx;
            }
            return tail;
        }

        case StmtKind::If:
        case StmtKind::Case:
            return tail;
        }

        return tail;
    }

    // Finalize: move nodes into 'out', patch next/delay_stmt pointers,
    // and return the head pointer (or nullptr if head_idx == npos).
    RtlStmt* finalize(std::vector<std::unique_ptr<RtlStmt>> &out,
                      std::size_t head_idx) {
        if (nodes.empty() || head_idx == npos)
            return nullptr;

        std::size_t base = out.size();
        out.reserve(out.size() + nodes.size());

        std::vector<RtlStmt*> ptrs(nodes.size(), nullptr);

        for (std::size_t i = 0; i < nodes.size(); ++i) {
            out.push_back(std::move(nodes[i]));
            ptrs[i] = out[base + i].get();
        }

        for (const auto &ln : links) {
            RtlStmt *from = ptrs[ln.from];
            RtlStmt *to   = (ln.to == npos) ? nullptr : ptrs[ln.to];
            if (!from) continue;

            switch (ln.kind) {
            case LinkKind::Next:
                from->next = to;
                break;
            case LinkKind::Delay:
                from->delay_stmt = to;
                break;
            }
        }

        return ptrs[head_idx];
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------
// IRBuilder implementation
// ---------------------------------------------------------------------

RtlDesign IRBuilder::build() {
    RtlDesign rd;
    rd.modules.reserve(design_.modules.size());

    for (const auto &m : design_.modules) {
        RtlModule rm = buildModule(*m);
        rd.modules.push_back(std::move(rm));   // <-- REQUIRED
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

RtlStmt* IRBuilder::build_proc_body(const Statement &body, RtlProcess &p) {
    ProcStmtBuilder builder;

    // Build the structural chain first
    std::size_t head_idx = builder.build_stmt_list(body, ProcStmtBuilder::npos);

    // Now fill in RHS and delay_expr by walking AST and nodes in lockstep
    std::size_t cursor = 0;

    std::function<void(const Statement&)> fill = [&](const Statement &s) {
        switch (s.kind) {

        case StmtKind::Null:
            break;

        case StmtKind::Block:
            for (const auto &st : s.block_stmts) {
                if (st) fill(*st);
            }
            break;

        case StmtKind::BlockingAssign:
        case StmtKind::NonBlockingAssign: {
            if (cursor >= builder.nodes.size()) return;
            RtlStmt &n = *builder.nodes[cursor++];
            if (s.rhs) {
                n.rhs = lowerExpr(*s.rhs);
            }
            break;
        }

        case StmtKind::Delay: {
            if (cursor >= builder.nodes.size()) return;
            RtlStmt &n = *builder.nodes[cursor++];
            if (s.delay_expr) {
                n.delay_expr = lowerExpr(*s.delay_expr);
            }
            if (s.delay_stmt) {
                fill(*s.delay_stmt);
            }
            break;
        }

        case StmtKind::ExprStmt:
            if (s.expr &&
                s.expr->kind == ExprKind::Identifier &&
                s.expr->ident == "$finish")
            {
                if (cursor < builder.nodes.size()) {
                    cursor++;
                }
            }
            break;

        case StmtKind::If:
        case StmtKind::Case:
            // Not lowered yet
            break;
        }
    };

    fill(body);

    // Finalize: move nodes into p.stmts and patch pointers
    return builder.finalize(p.stmts, head_idx);
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
                            p.sensitivity_signals.push_back(name);
                        } else if (si.negedge) {
                            p.sensitivity_signals.push_back(name);
                        } else {
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

            // 2) Old flattened assigns (for combinational engine)
            auto lower_body_assigns = [&](const Statement &body, RtlAssignKind default_kind) {
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
                    lower_body_assigns(body, RtlAssignKind::Blocking);
                    break;

                case AlwaysKind::AlwaysFF:
                    lower_body_assigns(body, RtlAssignKind::NonBlocking);
                    break;

                case AlwaysKind::Always:
                case AlwaysKind::AlwaysLatch:
                default:
                    lower_body_assigns(body, RtlAssignKind::Blocking);
                    break;
                }

                // 3) Procedural IR: build RtlStmt chain with stable pointers
                p.first_stmt = build_proc_body(body, p);
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

                // Keep old flattened assigns for simple initial semantics
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

                // Procedural IR: full statement chain with stable pointers
                p.first_stmt = build_proc_body(body, p);
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
        r->const_literal = e.literal;
        return r;
    }
    case ExprKind::Binary: {
        auto r = std::make_unique<RtlExpr>(RtlExprKind::Binary);
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
            r->bin_op = RtlBinOp::Add;
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
        auto cond = lowerExpr(*e.cond);
        auto t    = lowerExpr(*e.then_expr);
        auto f    = lowerExpr(*e.else_expr);

        auto and1 = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        and1->bin_op = RtlBinOp::And;
        and1->lhs = std::make_unique<RtlExpr>(*cond);
        and1->rhs = std::make_unique<RtlExpr>(*t);

        auto notc = std::make_unique<RtlExpr>(RtlExprKind::Unary);
        notc->un_op = RtlUnOp::BitNot;
        notc->un_operand = std::move(cond);

        auto and2 = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        and2->bin_op = RtlBinOp::And;
        and2->lhs = std::move(notc);
        and2->rhs = std::move(f);

        auto or_expr = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        or_expr->bin_op = RtlBinOp::Or;
        or_expr->lhs = std::move(and1);
        or_expr->rhs = std::move(and2);

        return or_expr;
    }

    case ExprKind::Concatenation:
    case ExprKind::Replication:
        return std::make_unique<RtlExpr>(RtlExprKind::Const);
    }

    return std::make_unique<RtlExpr>(RtlExprKind::Const);
}

RtlAssign IRBuilder::lowerAssign(const Statement &s, RtlAssignKind kind) {
    RtlAssign a;
    a.kind = kind;

    const Expression *lhs = s.lhs.get();

    if (lhs && lhs->kind == ExprKind::Identifier) {
        a.lhs_name = lhs->ident;
    } else if (lhs &&
               lhs->kind == ExprKind::BitSelect &&
               lhs->lhs &&
               lhs->lhs->kind == ExprKind::Identifier) {
        // r[i] / r[i] <= ... → "r"
        a.lhs_name = lhs->lhs->ident;
    } else {
        a.lhs_name = "<expr>";
    }

    if (s.rhs) {
        a.rhs = lowerExpr(*s.rhs);
    }

    return a;
}

void IRBuilder::collectContinuousAssigns(const ModuleDecl &mod, RtlModule &out) {
    for (const auto &item_up : mod.items) {
        const ModuleItem *item = item_up.get();
        if (!item) continue;

        if (item->kind == ModuleItemKind::ContinuousAssign && item->cont_assign) {
            const ContinuousAssign &ca = *item->cont_assign;

            RtlAssign a;
            a.kind = RtlAssignKind::Continuous;

            const Expression *lhs = ca.lhs.get();

            if (lhs && lhs->kind == ExprKind::Identifier) {
                a.lhs_name = lhs->ident;
            } else if (lhs &&
                       lhs->kind == ExprKind::BitSelect &&
                       lhs->lhs &&
                       lhs->lhs->kind == ExprKind::Identifier) {
                // assign r[i] = ...; → "r"
                a.lhs_name = lhs->lhs->ident;
            } else {
                a.lhs_name = "<expr>";
            }

            if (ca.rhs) {
                a.rhs = lowerExpr(*ca.rhs);
            }

            out.continuous_assigns.push_back(std::move(a));
        }
    }
}

} // namespace sv
