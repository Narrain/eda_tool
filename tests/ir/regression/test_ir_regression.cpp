// path: tests/ir/regression/test_ir_regression.cpp
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
    mod->name = "regression_top";

    // simple continuous assign: assign y = a + b;
    auto nitem_a = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
    nitem_a->net_decl = std::make_unique<NetDecl>();
    nitem_a->net_decl->name = "a";
    nitem_a->net_decl->type.kind = DataTypeKind::Logic;
    mod->items.push_back(std::move(nitem_a));

    auto nitem_b = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
    nitem_b->net_decl = std::make_unique<NetDecl>();
    nitem_b->net_decl->name = "b";
    nitem_b->net_decl->type.kind = DataTypeKind::Logic;
    mod->items.push_back(std::move(nitem_b));

    auto nitem_y = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
    nitem_y->net_decl = std::make_unique<NetDecl>();
    nitem_y->net_decl->name = "y";
    nitem_y->net_decl->type.kind = DataTypeKind::Logic;
    mod->items.push_back(std::move(nitem_y));

    auto citem = std::make_unique<ModuleItem>(ModuleItemKind::ContinuousAssign);
    citem->cont_assign = std::make_unique<ContinuousAssign>();
    citem->cont_assign->lhs = std::make_unique<Expression>(ExprKind::Identifier);
    citem->cont_assign->lhs->ident = "y";

    auto lhs = std::make_unique<Expression>(ExprKind::Identifier);
    lhs->ident = "a";
    auto rhs = std::make_unique<Expression>(ExprKind::Identifier);
    rhs->ident = "b";
    auto bin = std::make_unique<Expression>(ExprKind::Binary);
    bin->binary_op = BinaryOp::Add;
    bin->lhs = std::move(lhs);
    bin->rhs = std::move(rhs);
    citem->cont_assign->rhs = std::move(bin);

    mod->items.push_back(std::move(citem));

    design->modules.push_back(std::move(mod));

    SymbolTable symtab;
    symtab.build(*design);

    Elaborator elab(*design, symtab);
    auto ed = elab.elaborate();

    IRBuilder builder(*design, ed, symtab);
    RtlDesign rd = builder.build();

    assert(rd.modules.size() == 1);
    const auto &rm = rd.modules[0];
    assert(rm.name == "regression_top");
    assert(rm.continuous_assigns.size() == 1);
    assert(rm.continuous_assigns[0].lhs_name == "y");

    std::cout << "test_ir_regression: PASS\n";
    return 0;
}
