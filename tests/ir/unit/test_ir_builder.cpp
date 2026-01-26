// path: tests/ir/unit/test_ir_builder.cpp
#include <cassert>
#include <iostream>
#include <memory>

#include "../../../src/frontend/ast.hpp"
#include "../../../src/frontend/symbol_table.hpp"
#include "../../../src/frontend/elab.hpp"
#include "../../../src/ir/ir_builder.hpp"

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
    pitem->param_decl->value->number_literal = "8";
    mod->items.push_back(std::move(pitem));

    // net
    auto nitem = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
    nitem->net_decl = std::make_unique<NetDecl>();
    nitem->net_decl->name = "a";
    nitem->net_decl->type.kind = DataTypeKind::Logic;
    mod->items.push_back(std::move(nitem));

    // always block with blocking assign: a = 1;
    auto aitem = std::make_unique<ModuleItem>(ModuleItemKind::Always);
    aitem->always = std::make_unique<AlwaysConstruct>();
    auto stmt = std::make_unique<Statement>(StmtKind::BlockingAssign);
    stmt->lhs = std::make_unique<Expression>(ExprKind::Identifier);
    stmt->lhs->ident = "a";
    stmt->rhs = std::make_unique<Expression>(ExprKind::Number);
    stmt->rhs->number_literal = "1";
    aitem->always->body = std::move(stmt);
    mod->items.push_back(std::move(aitem));

    design->modules.push_back(std::move(mod));

    SymbolTable symtab;
    symtab.build(*design);

    Elaborator elab(*design, symtab);
    auto ed = elab.elaborate();

    IRBuilder builder(*design, ed, symtab);
    RtlDesign rd = builder.build();

    assert(rd.modules.size() == 1);
    const auto &rm = rd.modules[0];
    assert(rm.name == "top");
    assert(rm.params.size() == 1);
    assert(rm.params[0].name == "WIDTH");
    assert(rm.params[0].value_str == "8");
    assert(rm.nets.size() == 1);
    assert(rm.nets[0].name == "a");
    assert(rm.processes.size() == 1);
    assert(rm.processes[0].assigns.size() == 1);
    assert(rm.processes[0].assigns[0].lhs_name == "a");

    std::cout << "test_ir_builder: PASS\n";
    return 0;
}
