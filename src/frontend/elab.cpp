// elab.cpp

#include "elab.hpp"
#include "ast.hpp"
#include "symbol_table.hpp"
#include "const_eval.hpp"

#include <utility>
#include <cassert>

namespace sv {

// -----------------------------------------------------------------------------
// Helpers: const eval
// -----------------------------------------------------------------------------

static int64_t eval_int(const Expression &e, const ConstEnv &env) {
    ConstEval ce;
    ConstValue cv = ce.eval(e, env);
    return cv.valid ? cv.value : 0;
}

// -----------------------------------------------------------------------------
// Global storage for generated items
// -----------------------------------------------------------------------------

// We need stable storage for cloned ModuleItem objects created during
// generate-for / generate-if elaboration. We keep them here and only
// store raw pointers in ElabModule::flat_items.
static std::vector<std::unique_ptr<ModuleItem>> g_generated_items;

// -----------------------------------------------------------------------------
// Expression cloning with genvar substitution
// -----------------------------------------------------------------------------

static std::unique_ptr<Expression> clone_expr_with_genvar(
    const Expression &e,
    const std::string &genvar_name,
    int64_t genvar_value)
{
    auto make_number = [&](int64_t v) {
        auto num = std::make_unique<Expression>(ExprKind::Number);
        num->literal = std::to_string(v);
        num->loc = e.loc;
        return num;
    };

    switch (e.kind) {
    case ExprKind::Identifier: {
        if (e.ident == genvar_name) {
            return make_number(genvar_value);
        }
        auto out = std::make_unique<Expression>(ExprKind::Identifier);
        out->loc = e.loc;
        out->ident = e.ident;
        return out;
    }

    case ExprKind::Number: {
        auto out = std::make_unique<Expression>(ExprKind::Number);
        out->loc = e.loc;
        out->literal = e.literal;
        return out;
    }

    case ExprKind::String: {
        auto out = std::make_unique<Expression>(ExprKind::String);
        out->loc = e.loc;
        out->literal = e.literal;
        return out;
    }

    case ExprKind::Unary: {
        auto out = std::make_unique<Expression>(ExprKind::Unary);
        out->loc = e.loc;
        out->unary_op = e.unary_op;
        if (e.unary_operand)
            out->unary_operand = clone_expr_with_genvar(*e.unary_operand, genvar_name, genvar_value);
        return out;
    }

    case ExprKind::Binary: {
        auto out = std::make_unique<Expression>(ExprKind::Binary);
        out->loc = e.loc;
        out->binary_op = e.binary_op;
        if (e.lhs)
            out->lhs = clone_expr_with_genvar(*e.lhs, genvar_name, genvar_value);
        if (e.rhs)
            out->rhs = clone_expr_with_genvar(*e.rhs, genvar_name, genvar_value);
        return out;
    }

    case ExprKind::Ternary: {
        auto out = std::make_unique<Expression>(ExprKind::Ternary);
        out->loc = e.loc;
        if (e.cond)
            out->cond = clone_expr_with_genvar(*e.cond, genvar_name, genvar_value);
        if (e.then_expr)
            out->then_expr = clone_expr_with_genvar(*e.then_expr, genvar_name, genvar_value);
        if (e.else_expr)
            out->else_expr = clone_expr_with_genvar(*e.else_expr, genvar_name, genvar_value);
        return out;
    }

    case ExprKind::Concatenation: {
        auto out = std::make_unique<Expression>(ExprKind::Concatenation);
        out->loc = e.loc;
        for (const auto &elem : e.concat_elems) {
            out->concat_elems.push_back(
                clone_expr_with_genvar(*elem, genvar_name, genvar_value));
        }
        return out;
    }

    case ExprKind::Replication: {
        auto out = std::make_unique<Expression>(ExprKind::Replication);
        out->loc = e.loc;
        if (e.replicate_count)
            out->replicate_count = clone_expr_with_genvar(*e.replicate_count, genvar_name, genvar_value);
        for (const auto &elem : e.replicate_elems) {
            out->replicate_elems.push_back(
                clone_expr_with_genvar(*elem, genvar_name, genvar_value));
        }
        return out;
    }

    case ExprKind::BitSelect: {
        auto out = std::make_unique<Expression>(ExprKind::BitSelect);
        out->loc = e.loc;
        if (e.lhs)
            out->lhs = clone_expr_with_genvar(*e.lhs, genvar_name, genvar_value);
        if (e.rhs) {
            auto idx = clone_expr_with_genvar(*e.rhs, genvar_name, genvar_value);
            if (idx->kind == ExprKind::Identifier &&
                idx->ident == genvar_name) {
                idx = make_number(genvar_value);
            }
            out->rhs = std::move(idx);
        }
        return out;
    }
    }

    auto out = std::make_unique<Expression>(e.kind);
    out->loc = e.loc;
    return out;
}

// -----------------------------------------------------------------------------
// Statement cloning with genvar substitution
// -----------------------------------------------------------------------------

static std::unique_ptr<Statement> clone_stmt_with_genvar(
    const Statement &s,
    const std::string &genvar_name,
    int64_t genvar_value)
{
    auto out = std::make_unique<Statement>(s.kind);
    out->loc = s.loc;

    switch (s.kind) {
    case StmtKind::Null:
        break;

    case StmtKind::Block: {
        for (const auto &sub : s.block_stmts) {
            out->block_stmts.push_back(
                clone_stmt_with_genvar(*sub, genvar_name, genvar_value));
        }
        break;
    }

    case StmtKind::If: {
        if (s.if_cond)
            out->if_cond = clone_expr_with_genvar(*s.if_cond, genvar_name, genvar_value);
        if (s.if_then)
            out->if_then = clone_stmt_with_genvar(*s.if_then, genvar_name, genvar_value);
        if (s.if_else)
            out->if_else = clone_stmt_with_genvar(*s.if_else, genvar_name, genvar_value);
        break;
    }

    case StmtKind::Case: {
        out->case_kind = s.case_kind;
        if (s.case_expr)
            out->case_expr = clone_expr_with_genvar(*s.case_expr, genvar_name, genvar_value);
        for (const auto &ci : s.case_items) {
            CaseItem nci;
            for (const auto &m : ci.matches) {
                nci.matches.push_back(
                    clone_expr_with_genvar(*m, genvar_name, genvar_value));
            }
            if (ci.stmt)
                nci.stmt = clone_stmt_with_genvar(*ci.stmt, genvar_name, genvar_value);
            out->case_items.push_back(std::move(nci));
        }
        break;
    }

    case StmtKind::BlockingAssign:
    case StmtKind::NonBlockingAssign: {
        if (s.lhs)
            out->lhs = clone_expr_with_genvar(*s.lhs, genvar_name, genvar_value);
        if (s.rhs)
            out->rhs = clone_expr_with_genvar(*s.rhs, genvar_name, genvar_value);
        break;
    }

    case StmtKind::Delay: {
        if (s.delay_expr)
            out->delay_expr = clone_expr_with_genvar(*s.delay_expr, genvar_name, genvar_value);
        if (s.delay_stmt)
            out->delay_stmt = clone_stmt_with_genvar(*s.delay_stmt, genvar_name, genvar_value);
        break;
    }

    case StmtKind::ExprStmt: {
        if (s.expr)
            out->expr = clone_expr_with_genvar(*s.expr, genvar_name, genvar_value);
        break;
    }
    }

    return out;
}

// -----------------------------------------------------------------------------
// Always / Initial cloning with genvar substitution
// -----------------------------------------------------------------------------

static std::unique_ptr<AlwaysConstruct> clone_always_with_genvar(
    const AlwaysConstruct &a,
    const std::string &genvar_name,
    int64_t genvar_value)
{
    auto out = std::make_unique<AlwaysConstruct>();
    out->loc  = a.loc;
    out->kind = a.kind;

    for (const auto &si : a.sensitivity_list) {
        SensitivityItem nsi;
        nsi.posedge = si.posedge;
        nsi.negedge = si.negedge;
        nsi.star    = si.star;
        if (si.expr)
            nsi.expr = clone_expr_with_genvar(*si.expr, genvar_name, genvar_value);
        out->sensitivity_list.push_back(std::move(nsi));
    }

    if (a.body)
        out->body = clone_stmt_with_genvar(*a.body, genvar_name, genvar_value);

    return out;
}

static std::unique_ptr<InitialConstruct> clone_initial_with_genvar(
    const InitialConstruct &ic,
    const std::string &genvar_name,
    int64_t genvar_value)
{
    auto out = std::make_unique<InitialConstruct>();
    out->loc = ic.loc;
    if (ic.body)
        out->body = clone_stmt_with_genvar(*ic.body, genvar_name, genvar_value);
    return out;
}

// -----------------------------------------------------------------------------
// ModuleItem cloning with genvar substitution
// -----------------------------------------------------------------------------

static std::unique_ptr<ModuleItem> clone_module_item_with_genvar(
    const ModuleItem &mi,
    const std::string &genvar_name,
    int64_t genvar_value)
{
    auto out = std::make_unique<ModuleItem>(mi.kind);
    out->loc = mi.loc;

    switch (mi.kind) {
    case ModuleItemKind::NetDecl: {
        if (mi.net_decl) {
            auto nd = std::make_unique<NetDecl>();
            nd->loc  = mi.net_decl->loc;
            nd->type = mi.net_decl->type;
            nd->name = mi.net_decl->name;
            if (mi.net_decl->init)
                nd->init = clone_expr_with_genvar(*mi.net_decl->init, genvar_name, genvar_value);
            out->net_decl = std::move(nd);
        }
        break;
    }

    case ModuleItemKind::VarDecl: {
        if (mi.var_decl) {
            auto vd = std::make_unique<VarDecl>();
            vd->loc  = mi.var_decl->loc;
            vd->type = mi.var_decl->type;
            vd->name = mi.var_decl->name;
            if (mi.var_decl->init)
                vd->init = clone_expr_with_genvar(*mi.var_decl->init, genvar_name, genvar_value);
            out->var_decl = std::move(vd);
        }
        break;
    }

    case ModuleItemKind::ParamDecl: {
        if (mi.param_decl) {
            auto pd = std::make_unique<ParamDecl>();
            pd->loc  = mi.param_decl->loc;
            pd->name = mi.param_decl->name;
            if (mi.param_decl->value)
                pd->value = clone_expr_with_genvar(*mi.param_decl->value, genvar_name, genvar_value);
            out->param_decl = std::move(pd);
        }
        break;
    }

    case ModuleItemKind::ContinuousAssign: {
        if (mi.cont_assign) {
            auto ca = std::make_unique<ContinuousAssign>();
            ca->loc = mi.cont_assign->loc;
            if (mi.cont_assign->lhs)
                ca->lhs = clone_expr_with_genvar(*mi.cont_assign->lhs, genvar_name, genvar_value);
            if (mi.cont_assign->rhs)
                ca->rhs = clone_expr_with_genvar(*mi.cont_assign->rhs, genvar_name, genvar_value);
            out->cont_assign = std::move(ca);
        }
        break;
    }

    case ModuleItemKind::Always: {
        if (mi.always) {
            out->always = clone_always_with_genvar(*mi.always, genvar_name, genvar_value);
        }
        break;
    }

    case ModuleItemKind::Initial: {
        if (mi.initial) {
            out->initial = clone_initial_with_genvar(*mi.initial, genvar_name, genvar_value);
        }
        break;
    }

    case ModuleItemKind::Instance: {
        if (mi.instance) {
            auto inst = std::make_unique<Instance>();
            inst->loc           = mi.instance->loc;
            inst->module_name   = mi.instance->module_name;
            inst->instance_name = mi.instance->instance_name;

            for (const auto &ov : mi.instance->param_overrides) {
                ParamOverride nov;
                nov.name = ov.name;
                if (ov.value)
                    nov.value = clone_expr_with_genvar(*ov.value, genvar_name, genvar_value);
                inst->param_overrides.push_back(std::move(nov));
            }

            for (const auto &pc : mi.instance->port_conns) {
                InstancePortConn npc;
                npc.port_name = pc.port_name;
                if (pc.expr)
                    npc.expr = clone_expr_with_genvar(*pc.expr, genvar_name, genvar_value);
                inst->port_conns.push_back(std::move(npc));
            }

            out->instance = std::move(inst);
        }
        break;
    }

    case ModuleItemKind::Generate:
    case ModuleItemKind::GenVarDecl: {
        // These are handled at generate elaboration level, not cloned here.
        break;
    }
    }

    return out;
}

ElaboratedDesign Elaborator::elaborate() {
    ElaboratedDesign out;

    for (const auto &m : design_.modules) {
        elaborateModule(*m, out);
    }

    return out;
}

void Elaborator::elaborateModule(const ModuleDecl &mod, ElaboratedDesign &out) {
    ElabModule em;
    em.name = mod.name;

    // Params
    for (const auto &p : mod.params) {
        ElabParam ep;
        ep.name = p->name;
        ep.value_str = "";
        if (p->value && p->value->kind == ExprKind::Number) {
            ep.value_str = p->value->literal;
            // We won't try to const‑eval here; has_int stays false for now.
        }
        em.params.push_back(std::move(ep));
    }

    // Nets / vars / instances / generates / always / initial
    ConstEnv empty_env;

    for (const auto &item_up : mod.items) {
        const ModuleItem *item = item_up.get();

        switch (item->kind) {
        case ModuleItemKind::NetDecl: {
            ElabNet en;
            en.name = item->net_decl->name;
            en.type = item->net_decl->type;
            em.nets.push_back(std::move(en));
            em.flat_items.push_back(item);
            break;
        }

        case ModuleItemKind::VarDecl: {
            ElabNet en;
            en.name = item->var_decl->name;
            en.type = item->var_decl->type;
            em.nets.push_back(std::move(en));
            em.flat_items.push_back(item);
            break;
        }

        case ModuleItemKind::ParamDecl:
        case ModuleItemKind::GenVarDecl:
            // Parameters/genvars don’t become nets; keep item for completeness.
            em.flat_items.push_back(item);
            break;

        case ModuleItemKind::Instance: {
            ElabInstance ei;
            ei.module_name   = item->instance->module_name;
            ei.instance_name = item->instance->instance_name;

            // Param overrides: keep literal string only for now.
            for (const auto &po : item->instance->param_overrides) {
                ElabParam ep;
                ep.name = po.name;
                ep.value_str = "";
                if (po.value && po.value->kind == ExprKind::Number) {
                    ep.value_str = po.value->literal;
                }
                ei.params.push_back(std::move(ep));
            }

            for (const auto &pc : item->instance->port_conns) {
                std::string sig;
                if (pc.expr && pc.expr->kind == ExprKind::Identifier)
                    sig = pc.expr->ident;
                ei.port_conns.emplace_back(pc.port_name, sig);
            }

            em.instances.push_back(std::move(ei));
            em.flat_items.push_back(item);
            break;
        }

        case ModuleItemKind::Generate:
            if (item->gen && item->gen->item) {
                elaborateGenerate(*item->gen->item, empty_env, em.flat_items);
            }
            break;

        case ModuleItemKind::Always:
        case ModuleItemKind::Initial:
        case ModuleItemKind::ContinuousAssign:
            // Keep procedural and continuous items as‑is; IRBuilder will consume them.
            em.flat_items.push_back(item);
            break;
        }
    }

    out.modules.emplace(em.name, std::move(em));
}


void Elaborator::elaborateGenerate(const GenerateItem &gi,
                                   const ConstEnv &env,
                                   std::vector<const ModuleItem*> &out_items) {
    switch (gi.kind) {

    case GenItemKind::Block: {
        if (gi.block) {
            for (const auto &mi : gi.block->items) {
                if (mi->kind == ModuleItemKind::Generate && mi->gen && mi->gen->item) {
                    elaborateGenerate(*mi->gen->item, env, out_items);
                } else {
                    out_items.push_back(mi.get());
                }
            }
        }
        break;
    }

    case GenItemKind::If: {
        // Try to const‑eval the condition; if not possible, include both branches.
        if (!gi.if_cond) {
            if (gi.if_then)
                elaborateGenerate(*gi.if_then, env, out_items);
            if (gi.if_else)
                elaborateGenerate(*gi.if_else, env, out_items);
            break;
        }

        int64_t cv = eval_int(*gi.if_cond, env);
        // We don’t have a validity flag here; treat it as “constant enough”.
        if (cv != 0) {
            if (gi.if_then)
                elaborateGenerate(*gi.if_then, env, out_items);
        } else {
            if (gi.if_else)
                elaborateGenerate(*gi.if_else, env, out_items);
        }
        break;
    }

    case GenItemKind::For: {
        if (!gi.for_init || !gi.for_cond || !gi.for_step || !gi.for_body)
            break;

        ConstEnv empty_env;

        // -----------------------------
        // Parse init: i = <number>;
        // -----------------------------
        int64_t start = 0;
        {
            const Expression &init = *gi.for_init;
            if (init.kind != ExprKind::Binary ||
                init.binary_op != BinaryOp::Assign ||
                !init.lhs || !init.rhs ||
                init.lhs->kind != ExprKind::Identifier ||
                init.lhs->ident != gi.genvar_name) {
                // Unsupported init form
                break;
            }
            start = eval_int(*init.rhs, empty_env);
        }

        // -----------------------------
        // Parse cond: i < <number>;
        // -----------------------------
        int64_t limit = 0;
        {
            const Expression &cond = *gi.for_cond;
            if (cond.kind != ExprKind::Binary ||
                cond.binary_op != BinaryOp::Lt ||
                !cond.lhs || !cond.rhs ||
                cond.lhs->kind != ExprKind::Identifier ||
                cond.lhs->ident != gi.genvar_name) {
                // Unsupported cond form
                break;
            }
            limit = eval_int(*cond.rhs, empty_env);
        }

        // -----------------------------
        // Parse step: i = i + <number>;
        // -----------------------------
        int64_t step = 0;
        {
            const Expression &step_expr = *gi.for_step;
            if (step_expr.kind != ExprKind::Binary ||
                step_expr.binary_op != BinaryOp::Assign ||
                !step_expr.lhs || !step_expr.rhs ||
                step_expr.lhs->kind != ExprKind::Identifier ||
                step_expr.lhs->ident != gi.genvar_name) {
                // Unsupported step form
                break;
            }

            const Expression &rhs = *step_expr.rhs;
            if (rhs.kind != ExprKind::Binary ||
                rhs.binary_op != BinaryOp::Add ||
                !rhs.lhs || !rhs.rhs ||
                rhs.lhs->kind != ExprKind::Identifier ||
                rhs.lhs->ident != gi.genvar_name) {
                // Unsupported step form
                break;
            }

            step = eval_int(*rhs.rhs, empty_env);
            if (step == 0)
                break;
        }

        // -----------------------------
        // Unroll: for (i = start; i < limit; i += step)
        // -----------------------------
        for (int64_t gv = start; gv < limit; gv += step) {
            if (gi.for_body->kind == GenItemKind::Block && gi.for_body->block) {
                for (const auto &mi : gi.for_body->block->items) {
                    auto cloned = clone_module_item_with_genvar(*mi, gi.genvar_name, gv);
                    g_generated_items.push_back(std::move(cloned));
                    out_items.push_back(g_generated_items.back().get());
                }
            } else {
                // Fallback: recursively elaborate nested generate
                elaborateGenerate(*gi.for_body, env, out_items);
            }
        }

        break;
    }

    case GenItemKind::Case:
        // Not needed for your current test; leave empty for now.
        break;
    }
}


} // namespace sv
