// path: tests/ir/negative/test_ir_negative.cpp
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
    mod->name = "neg_top";

    // malformed: always with no body assignments -> IRBuilder should not crash
    auto aitem = std::make_unique<ModuleItem>(ModuleItemKind::Always);
    aitem->always = std::make_unique<AlwaysConstruct>();
    mod->items.push_back(std::move(aitem));

    design->modules.push_back(std::move(mod));

    SymbolTable symtab;
    symtab.build(*design);

    Elaborator elab(*design, symtab);
    auto ed = elab.elaborate();

    IRBuilder builder(*design, ed, symtab);
    RtlDesign rd = builder.build();

    assert(rd.modules.size() == 1);
    std::cout << "test_ir_negative: PASS\n";
    return 0;
}
