// path: src/ir/ir_builder.hpp
#ifndef __IR_BUILDER_HPP__
#define __IR_BUILDER_HPP__

#include <memory>

#include "rtl_ir.hpp"
#include "../frontend/ast.hpp"
#include "../frontend/elab.hpp"
#include "../frontend/symbol_table.hpp"

namespace sv {

class IRBuilder {
public:
    IRBuilder(const Design &design,
              const ElaboratedDesign &elab,
              const SymbolTable &symtab)
        : design_(design), elab_(elab), symtab_(symtab) {}

    RtlDesign build();

private:
    const Design &design_;
    const ElaboratedDesign &elab_;
    const SymbolTable &symtab_;

    RtlModule buildModule(const ModuleDecl &mod);
    void collectParams(const ModuleDecl &mod, RtlModule &out);
    void collectNets(const ModuleDecl &mod, RtlModule &out);
    void collectContinuousAssigns(const ModuleDecl &mod, RtlModule &out);
    void collectProcesses(const ModuleDecl &mod, RtlModule &out);
    void collectInstances(const ModuleDecl &mod, RtlModule &out);

    std::unique_ptr<RtlExpr> lowerExpr(const Expression &e);
    RtlAssign lowerAssign(const Statement &s, RtlAssignKind kind);
};

} // namespace sv

#endif // __IR_BUILDER_HPP__
