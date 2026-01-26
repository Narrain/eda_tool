// path: src/frontend/elab.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "ast.hpp"
#include "symbol_table.hpp"

namespace sv {

// Elaborated net/var
struct ElabNet {
    std::string name;
    DataType type;
};

struct ElabParam {
    std::string name;
    std::string value_str; // simple textual value for now
};

struct ElabInstance;

struct ElabModule {
    std::string name;
    std::vector<ElabParam> params;
    std::vector<ElabNet> nets;
    std::vector<ElabInstance> instances;
};

struct ElabInstance {
    std::string module_name;
    std::string instance_name;
    std::vector<std::pair<std::string, std::string>> port_conns; // port -> signal
};

struct ElaboratedDesign {
    std::unordered_map<std::string, ElabModule> modules;
};

class Elaborator {
public:
    Elaborator(const Design &design, const SymbolTable &symtab)
        : design_(design), symtab_(symtab) {}

    ElaboratedDesign elaborate();

private:
    const Design &design_;
    const SymbolTable &symtab_;

    void elaborateModule(const ModuleDecl &mod, ElaboratedDesign &out);
};

} // namespace sv
