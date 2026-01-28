// path: src/frontend/elab.cpp
#include "const_eval.hpp"
#include "elab.hpp"

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

    ConstEval ce;

    // Parameters
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::ParamDecl && item->param_decl) {
            ElabParam p;
            p.name = item->param_decl->name;
            if (item->param_decl->value) {
                auto cv = ce.eval(*item->param_decl->value);
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

    // Nets/vars
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::NetDecl && item->net_decl) {
            ElabNet n;
            n.name = item->net_decl->name;
            n.type = item->net_decl->type;
            em.nets.push_back(std::move(n));
        } else if (item->kind == ModuleItemKind::VarDecl && item->var_decl) {
            ElabNet n;
            n.name = item->var_decl->name;
            n.type = item->var_decl->type;
            em.nets.push_back(std::move(n));
        }
    }

    // Instances
    for (const auto &item : mod.items) {
        if (item->kind == ModuleItemKind::Instance && item->instance) {
            ElabInstance ei;
            ei.module_name = item->instance->module_name;
            ei.instance_name = item->instance->instance_name;
            for (const auto &pc : item->instance->port_conns) {
                std::string sig;
                if (pc.expr && pc.expr->kind == ExprKind::Identifier) {
                    sig = pc.expr->ident;
                } else {
                    sig = "<expr>";
                }
                ei.port_conns.emplace_back(pc.port_name, sig);
            }
            em.instances.push_back(std::move(ei));
        }
    }

    out.modules[em.name] = std::move(em);
}

} // namespace sv
