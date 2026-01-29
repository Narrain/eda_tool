// path: src/frontend/elab.cpp
#include "elab.hpp"
#include <iostream>

namespace sv {

ElaboratedDesign Elaborator::elaborate() {
    ElaboratedDesign ed;
    for (const auto &m : design_.modules) {
        elaborateModule(*m, ed);
    }
    return ed;
}

void Elaborator::elaborateModule(const ModuleDecl &mod, ElaboratedDesign &out) {
    ElabModule em;
    em.name = mod.name;

    // FIX: ConstEval has only default ctor
    ConstEval ce;
    ConstEnv default_env;

    // ------------------------------------------------------------
    // 1) Seed flat_items with original module items
    // ------------------------------------------------------------
    std::vector<const ModuleItem*> flat_items;
    flat_items.reserve(mod.items.size());

    for (const auto &ptr : mod.items) {
        flat_items.push_back(ptr.get());
    }

    // ------------------------------------------------------------
    // 2) Flatten generate blocks
    // ------------------------------------------------------------
    std::vector<const ModuleItem*> work = flat_items;
    flat_items.clear();

    while (!work.empty()) {
        const ModuleItem *item = work.back();
        work.pop_back();
        if (!item) continue;

        if (item->kind == ModuleItemKind::Generate &&
            item->gen && item->gen->item) {

            // Expand generate-for / generate-if / generate-case
            elaborateGenerate(*item->gen->item, default_env, work);

        } else {
            flat_items.push_back(item);
        }
    }

    // ------------------------------------------------------------
    // 3) Parameters
    // ------------------------------------------------------------
    for (const ModuleItem *item : flat_items) {
        if (item->kind == ModuleItemKind::ParamDecl && item->param_decl) {
            ElabParam p;
            p.name = item->param_decl->name;

            if (item->param_decl->value) {
                auto cv = ce.eval(*item->param_decl->value, default_env);
                if (cv.valid) {
                    p.has_int = true;
                    p.int_value = cv.value;
                    p.value_str = std::to_string(cv.value);
                } else if (item->param_decl->value->kind == ExprKind::Number) {
                    p.value_str = item->param_decl->value->literal;
                } else {
                    p.value_str = "<expr>";
                }
            } else {
                p.value_str = "<unset>";
            }

            em.params.push_back(std::move(p));
        }
    }

    // ------------------------------------------------------------
    // 4) Nets / Vars
    // ------------------------------------------------------------
    for (const ModuleItem *item : flat_items) {
        if (item->kind == ModuleItemKind::NetDecl && item->net_decl) {
            ElabNet n;
            n.name = item->net_decl->name;
            n.type = item->net_decl->type;
            em.nets.push_back(std::move(n));
        }
        else if (item->kind == ModuleItemKind::VarDecl && item->var_decl) {
            ElabNet n;
            n.name = item->var_decl->name;
            n.type = item->var_decl->type;
            em.nets.push_back(std::move(n));
        }
    }

    // ------------------------------------------------------------
    // 5) Instances (with param overrides + port connections)
    // ------------------------------------------------------------
    for (const ModuleItem *item : flat_items) {
        if (item->kind == ModuleItemKind::Instance && item->instance) {
            const auto &inst = *item->instance;

            ElabInstance ei;
            ei.module_name = inst.module_name;
            ei.instance_name = inst.instance_name;

            // Build instance param env = defaults
            ConstEnv inst_env = default_env;

            // Apply overrides
            for (const auto &ov : inst.param_overrides) {
                if (!ov.value) continue;
                auto cv = ce.eval(*ov.value, inst_env);
                if (cv.valid) {
                    inst_env[ov.name] = cv.value;
                }
            }

            // Store evaluated params on instance
            for (const auto &mp : em.params) {
                ElabParam ip = mp;
                auto it = inst_env.find(mp.name);
                if (it != inst_env.end()) {
                    ip.has_int = true;
                    ip.int_value = it->second;
                    ip.value_str = std::to_string(it->second);
                }
                ei.params.push_back(std::move(ip));
            }

            // Port connections
            for (const auto &pc : inst.port_conns) {
                std::string sig =
                    (pc.expr && pc.expr->kind == ExprKind::Identifier)
                    ? pc.expr->ident
                    : "<expr>";
                ei.port_conns.emplace_back(pc.port_name, sig);
            }

            em.instances.push_back(std::move(ei));
        }
    }

    // ------------------------------------------------------------
    // 6) Store elaborated module
    // ------------------------------------------------------------
    out.modules[em.name] = std::move(em);
}

void Elaborator::elaborateGenerate(const GenerateItem &gi,
                                   const ConstEnv &env,
                                   std::vector<const ModuleItem*> &out_items) {
    ConstEval ce;

    switch (gi.kind) {

    case GenItemKind::Block: {
        if (!gi.block) return;

        for (const auto &mi : gi.block->items) {
            if (!mi) continue;

            if (mi->kind == ModuleItemKind::Generate &&
                mi->gen && mi->gen->item) {

                elaborateGenerate(*mi->gen->item, env, out_items);

            } else {
                out_items.push_back(mi.get());
            }
        }
        break;
    }

    case GenItemKind::If: {
        if (!gi.if_cond || !gi.if_then) return;

        auto cv = ce.eval(*gi.if_cond, env);
        if (!cv.valid) return;

        if (cv.value) {
            elaborateGenerate(*gi.if_then, env, out_items);
        } else if (gi.if_else) {
            elaborateGenerate(*gi.if_else, env, out_items);
        }
        break;
    }

    case GenItemKind::For: {
        if (!gi.for_init || !gi.for_cond || !gi.for_step || !gi.for_body)
            return;

        ConstEnv local_env = env;

        // init: genvar = init
        auto init_v = ce.eval(*gi.for_init, local_env);
        if (!init_v.valid) return;

        if (gi.genvar_name.empty()) {
            throw std::runtime_error("elaborateGenerate: missing genvar_name");
        }

        local_env[gi.genvar_name] = init_v.value;

        while (true) {
            auto cond_v = ce.eval(*gi.for_cond, local_env);
            if (!cond_v.valid || !cond_v.value)
                break;

            elaborateGenerate(*gi.for_body, local_env, out_items);

            auto step_v = ce.eval(*gi.for_step, local_env);
            if (!step_v.valid)
                break;

            local_env[gi.genvar_name] = step_v.value;
        }
        break;
    }

    case GenItemKind::Case: {
        if (!gi.case_expr) return;

        auto sel_v = ce.eval(*gi.case_expr, env);
        if (!sel_v.valid) return;

        bool matched = false;

        for (const auto &ci : gi.case_items) {
            if (ci.matches.empty()) {
                if (!matched && ci.stmt) {
                    // treat stmt as nested generate block
                }
                continue;
            }

            for (const auto &m : ci.matches) {
                auto mv = ce.eval(*m, env);
                if (mv.valid && mv.value == sel_v.value) {
                    matched = true;
                    break;
                }
            }

            if (matched)
                break;
        }
        break;
    }

    } // switch
}

} // namespace sv
