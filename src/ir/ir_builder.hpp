#ifndef IR_BUILDER_HPP
#define IR_BUILDER_HPP

#include "../frontend/ast.hpp"
#include "../frontend/elab.hpp"
#include "rtl_ir.hpp"

namespace sv {

class IRBuilder {
public:
    // Keep 3‑arg ctor so existing tests compile; symtab can be ignored here.
    IRBuilder(const Design &design,
              const ElaboratedDesign &elab,
              const SymbolTable &)
        : design_(design), elab_(elab) {}

    // Also allow the 2‑arg form if you want to use it elsewhere.
    IRBuilder(const Design &design, const ElaboratedDesign &elab)
        : design_(design), elab_(elab) {}

    RtlDesign build();

private:
    const Design &design_;
    const ElaboratedDesign &elab_;

    RtlModule buildModule(const ModuleDecl &mod);

    void collectParams(const ModuleDecl &mod, RtlModule &out);
    void collectNets(const ModuleDecl &mod, RtlModule &out);
    void collectContinuousAssigns(const ModuleDecl &mod, RtlModule &out);
    void collectProcesses(const ModuleDecl &mod, RtlModule &out);
    void collectInstances(const ModuleDecl &mod, RtlModule &out);

    void collectProcessFromAlways(const AlwaysConstruct &ac, RtlModule &out);
    void collectProcessFromInitial(const InitialConstruct &ic, RtlModule &out);

    std::unique_ptr<RtlExpr> lowerExpr(const Expression &e);
    RtlAssign lowerAssign(const Statement &s, RtlAssignKind kind);

    RtlStmt* build_proc_body(const Statement &body, RtlProcess &p);
};
void dump_rtl_module(const RtlModule &m);

} // namespace sv

#endif // IR_BUILDER_HPP
