// path: src/frontend/symbol_table.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "ast.hpp"

namespace sv {

enum class SymbolKind {
    Net,
    Var,
    Param,
    Port,
    Module
};

struct Symbol {
    SymbolKind kind;
    std::string name;
    const Node *decl = nullptr;
};

class Scope {
public:
    Scope(Scope *parent = nullptr) : parent_(parent) {}

    bool add(const Symbol &sym) {
        auto it = table_.find(sym.name);
        if (it != table_.end()) return false;
        table_[sym.name] = sym;
        return true;
    }

    const Symbol *lookup(const std::string &name) const {
        auto it = table_.find(name);
        if (it != table_.end()) return &it->second;
        if (parent_) return parent_->lookup(name);
        return nullptr;
    }

private:
    Scope *parent_;
    std::unordered_map<std::string, Symbol> table_;
};

class SymbolTable {
public:
    SymbolTable() = default;

    void build(const Design &design);

    const Symbol *lookupModule(const std::string &name) const {
        auto it = modules_.find(name);
        if (it != modules_.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<std::string, Symbol> modules_;
};

} // namespace sv
