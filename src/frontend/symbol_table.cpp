// path: src/frontend/symbol_table.cpp
#include "symbol_table.hpp"

namespace sv {

void SymbolTable::build(const Design &design) {
    modules_.clear();
    for (const auto &m : design.modules) {
        Symbol sym;
        sym.kind = SymbolKind::Module;
        sym.name = m->name;
        sym.decl = m.get();
        modules_[sym.name] = sym;
    }
}

} // namespace sv
