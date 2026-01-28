// path: tests/frontend/unit/test_frontend_elab.cpp
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../../../src/frontend/ast.hpp"
#include "../../../src/frontend/symbol_table.hpp"
#include "../../../src/frontend/elab.hpp"

int main() {
    using namespace sv;

    auto design = std::make_unique<Design>();
    auto mod = std::make_unique<ModuleDecl>();
    mod->name = "top";

    // parameter
    auto pitem = std::make_unique<ModuleItem>(ModuleItemKind::ParamDecl);
    pitem->param_decl = std::make_unique<ParamDecl>();
    pitem->param_decl->name = "WIDTH";
    pitem->param_decl->value = std::make_unique<Expression>(ExprKind::Number);
    pitem->param_decl->value->literal = "8";
    mod->items.push_back(std::move(pitem));

    // net
    auto nitem = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
    nitem->net_decl = std::make_unique<NetDecl>();
    nitem->net_decl->name = "a";
    nitem->net_decl->type.kind = DataTypeKind::Logic;
    mod->items.push_back(std::move(nitem));

    design->modules.push_back(std::move(mod));

    SymbolTable symtab;
    symtab.build(*design);

    Elaborator elab(*design, symtab);
    auto ed = elab.elaborate();

    assert(ed.modules.size() == 1);
    auto it = ed.modules.find("top");
    assert(it != ed.modules.end());
    const auto &em = it->second;
    assert(em.params.size() == 1);
    assert(em.params[0].name == "WIDTH");
    assert(em.params[0].value_str == "8");
    assert(em.nets.size() == 1);
    assert(em.nets[0].name == "a");

    std::cout << "test_frontend_elab: PASS\n";
    return 0;
}
