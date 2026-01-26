// path: src/frontend/parser.cpp
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "ast.hpp"

namespace sv {

enum class TokenKind;
struct Token;

class Parser {
public:
    Parser(const std::vector<Token> &tokens)
        : tokens_(tokens) {}

    std::unique_ptr<Design> parseDesign() {
        auto design = std::make_unique<Design>();
        while (!match(TokenKind::EndOfFile)) {
            design->modules.push_back(parseModule());
        }
        return design;
    }

private:
    const std::vector<Token> &tokens_;
    size_t idx_ = 0;

    const Token &peek() const {
        if (idx_ >= tokens_.size()) return tokens_.back();
        return tokens_[idx_];
    }

    const Token &get() {
        const Token &t = peek();
        if (idx_ < tokens_.size()) idx_++;
        return t;
    }

    bool match(TokenKind kind, const std::string &text = "") {
        if (peek().kind != kind) return false;
        if (!text.empty() && peek().text != text) return false;
        return true;
    }

    const Token &expect(TokenKind kind, const std::string &text = "") {
        if (!match(kind, text)) {
            throw std::runtime_error("Parse error near token: " + peek().text);
        }
        return get();
    }

    // Forward declarations
    std::unique_ptr<ModuleDecl> parseModule();
    std::unique_ptr<PortDecl> parsePortDecl();
    std::unique_ptr<ModuleItem> parseModuleItem();
    std::unique_ptr<NetDecl> parseNetDecl();
    std::unique_ptr<VarDecl> parseVarDecl();
    std::unique_ptr<ParamDecl> parseParamDecl();
    std::unique_ptr<ContinuousAssign> parseContinuousAssign();
    std::unique_ptr<AlwaysConstruct> parseAlways();
    std::unique_ptr<Instance> parseInstance();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseStatementOrBlock();
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parsePrimary();
    std::unique_ptr<Expression> parseBinaryRHS(int exprPrec, std::unique_ptr<Expression> lhs);

    int getBinOpPrecedence(const std::string &op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/") return 6;
        return -1;
    }

    BinaryOp mapBinaryOp(const std::string &op) {
        if (op == "+") return BinaryOp::Add;
        if (op == "-") return BinaryOp::Sub;
        if (op == "*") return BinaryOp::Mul;
        if (op == "/") return BinaryOp::Div;
        if (op == "&&") return BinaryOp::LogicalAnd;
        if (op == "||") return BinaryOp::LogicalOr;
        if (op == "==") return BinaryOp::Eq;
        if (op == "!=") return BinaryOp::Neq;
        if (op == "<") return BinaryOp::Lt;
        if (op == ">") return BinaryOp::Gt;
        if (op == "<=") return BinaryOp::Le;
        if (op == ">=") return BinaryOp::Ge;
        if (op == "&") return BinaryOp::And;
        if (op == "|") return BinaryOp::Or;
        if (op == "^") return BinaryOp::Xor;
        throw std::runtime_error("Unknown binary op: " + op);
    }

    DataType parseDataType(const std::string &kw) {
        DataType dt;
        if (kw == "wire") dt.kind = DataTypeKind::Wire;
        else if (kw == "logic") dt.kind = DataTypeKind::Logic;
        else if (kw == "reg") dt.kind = DataTypeKind::Reg;
        else dt.kind = DataTypeKind::Unknown;

        if (match(TokenKind::Symbol, "[")) {
            get(); // [
            auto msbTok = expect(TokenKind::Number);
            expect(TokenKind::Symbol, ":");
            auto lsbTok = expect(TokenKind::Number);
            expect(TokenKind::Symbol, "]");
            dt.is_packed = true;
            dt.msb = std::stoi(msbTok.text);
            dt.lsb = std::stoi(lsbTok.text);
        }
        return dt;
    }

    std::unique_ptr<ModuleDecl> parseModule() {
        expect(TokenKind::Keyword, "module");
        auto nameTok = expect(TokenKind::Identifier);
        auto mod = std::make_unique<ModuleDecl>();
        mod->name = nameTok.text;
        if (match(TokenKind::Symbol, "(")) {
            get();
            if (!match(TokenKind::Symbol, ")")) {
                while (true) {
                    mod->ports.push_back(parsePortDecl());
                    if (match(TokenKind::Symbol, ",")) {
                        get();
                        continue;
                    }
                    break;
                }
            }
            expect(TokenKind::Symbol, ")");
        }
        expect(TokenKind::Symbol, ";");

        while (!match(TokenKind::Keyword, "endmodule")) {
            mod->items.push_back(parseModuleItem());
        }
        expect(TokenKind::Keyword, "endmodule");
        return mod;
    }

    std::unique_ptr<PortDecl> parsePortDecl() {
        PortDirection dir;
        if (match(TokenKind::Keyword, "input")) {
            get();
            dir = PortDirection::Input;
        } else if (match(TokenKind::Keyword, "output")) {
            get();
            dir = PortDirection::Output;
        } else if (match(TokenKind::Keyword, "inout")) {
            get();
            dir = PortDirection::Inout;
        } else {
            throw std::runtime_error("Expected port direction");
        }

        DataType dt;
        if (match(TokenKind::Keyword)) {
            auto kw = get();
            dt = parseDataType(kw.text);
        }

        auto nameTok = expect(TokenKind::Identifier);
        auto p = std::make_unique<PortDecl>();
        p->dir = dir;
        p->type = dt;
        p->name = nameTok.text;
        return p;
    }

    std::unique_ptr<ModuleItem> parseModuleItem() {
        if (match(TokenKind::Keyword, "wire") ||
            match(TokenKind::Keyword, "logic") ||
            match(TokenKind::Keyword, "reg")) {
            auto dtkw = get();
            if (match(TokenKind::Identifier)) {
                // net/var decl
                auto dt = parseDataType(dtkw.text);
                auto nameTok = expect(TokenKind::Identifier);
                if (match(TokenKind::Symbol, ";")) {
                    get();
                    auto item = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
                    item->net_decl = std::make_unique<NetDecl>();
                    item->net_decl->type = dt;
                    item->net_decl->name = nameTok.text;
                    return item;
                }
            }
        }

        if (match(TokenKind::Keyword, "parameter") ||
            match(TokenKind::Keyword, "localparam")) {
            auto p = parseParamDecl();
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::ParamDecl);
            item->param_decl = std::move(p);
            return item;
        }

        if (match(TokenKind::Keyword, "assign")) {
            auto ca = parseContinuousAssign();
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::ContinuousAssign);
            item->cont_assign = std::move(ca);
            return item;
        }

        if (match(TokenKind::Keyword, "always") ||
            match(TokenKind::Keyword, "always_ff") ||
            match(TokenKind::Keyword, "always_comb") ||
            match(TokenKind::Keyword, "always_latch")) {
            auto a = parseAlways();
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::Always);
            item->always = std::move(a);
            return item;
        }

        if (match(TokenKind::Identifier)) {
            // instance or something else
            // simple heuristic: ident ident '(' ...
            auto saveIdx = idx_;
            auto modNameTok = get();
            if (match(TokenKind::Identifier)) {
                auto instNameTok = get();
                if (match(TokenKind::Symbol, "(")) {
                    auto inst = std::make_unique<Instance>();
                    inst->module_name = modNameTok.text;
                    inst->instance_name = instNameTok.text;
                    get(); // (
                    if (!match(TokenKind::Symbol, ")")) {
                        while (true) {
                            InstancePortConn conn;
                            if (match(TokenKind::Symbol, ".")) {
                                get();
                                auto portNameTok = expect(TokenKind::Identifier);
                                conn.port_name = portNameTok.text;
                                expect(TokenKind::Symbol, "(");
                                conn.expr = parseExpression();
                                expect(TokenKind::Symbol, ")");
                            } else {
                                conn.expr = parseExpression();
                            }
                            inst->port_conns.push_back(std::move(conn));
                            if (match(TokenKind::Symbol, ",")) {
                                get();
                                continue;
                            }
                            break;
                        }
                    }
                    expect(TokenKind::Symbol, ")");
                    expect(TokenKind::Symbol, ";");
                    auto item = std::make_unique<ModuleItem>(ModuleItemKind::Instance);
                    item->instance = std::move(inst);
                    return item;
                }
            }
            // fallback: not an instance, restore and error
            idx_ = saveIdx;
        }

        throw std::runtime_error("Unsupported or invalid module item near token: " + peek().text);
    }

    std::unique_ptr<ParamDecl> parseParamDecl() {
        get(); // parameter/localparam
        auto nameTok = expect(TokenKind::Identifier);
        expect(TokenKind::Symbol, "=");
        auto expr = parseExpression();
        expect(TokenKind::Symbol, ";");
        auto p = std::make_unique<ParamDecl>();
        p->name = nameTok.text;
        p->value = std::move(expr);
        return p;
    }

    std::unique_ptr<ContinuousAssign> parseContinuousAssign() {
        expect(TokenKind::Keyword, "assign");
        auto lhs = parseExpression();
        expect(TokenKind::Symbol, "=");
        auto rhs = parseExpression();
        expect(TokenKind::Symbol, ";");
        auto c = std::make_unique<ContinuousAssign>();
        c->lhs = std::move(lhs);
        c->rhs = std::move(rhs);
        return c;
    }

    std::unique_ptr<AlwaysConstruct> parseAlways() {
        auto kw = get();
        auto a = std::make_unique<AlwaysConstruct>();
        if (kw.text == "always_ff") a->kind = AlwaysKind::AlwaysFF;
        else if (kw.text == "always_comb") a->kind = AlwaysKind::AlwaysComb;
        else if (kw.text == "always_latch") a->kind = AlwaysKind::AlwaysLatch;
        else a->kind = AlwaysKind::Always;

        if (match(TokenKind::Symbol, "@")) {
            get();
            expect(TokenKind::Symbol, "(");
            while (!match(TokenKind::Symbol, ")")) {
                SensitivityItem item;
                if (match(TokenKind::Keyword, "posedge")) {
                    get();
                    item.posedge = true;
                    item.expr = parseExpression();
                } else if (match(TokenKind::Keyword, "negedge")) {
                    get();
                    item.negedge = true;
                    item.expr = parseExpression();
                } else {
                    item.expr = parseExpression();
                }
                a->sensitivity_list.push_back(std::move(item));
                if (match(TokenKind::Symbol, "or") || match(TokenKind::Symbol, ",")) {
                    get();
                    continue;
                }
                break;
            }
            expect(TokenKind::Symbol, ")");
        }

        a->body = parseStatementOrBlock();
        return a;
    }

    std::unique_ptr<Statement> parseStatementOrBlock() {
        if (match(TokenKind::Keyword, "begin")) {
            get();
            auto blk = std::make_unique<Statement>(StmtKind::Block);
            while (!match(TokenKind::Keyword, "end")) {
                blk->block_stmts.push_back(parseStatement());
            }
            expect(TokenKind::Keyword, "end");
            return blk;
        }
        return parseStatement();
    }

    std::unique_ptr<Statement> parseStatement() {
        if (match(TokenKind::Keyword, "if")) {
            get();
            expect(TokenKind::Symbol, "(");
            auto cond = parseExpression();
            expect(TokenKind::Symbol, ")");
            auto thenStmt = parseStatementOrBlock();
            std::unique_ptr<Statement> elseStmt;
            if (match(TokenKind::Keyword, "else")) {
                get();
                elseStmt = parseStatementOrBlock();
            }
            auto s = std::make_unique<Statement>(StmtKind::If);
            s->if_cond = std::move(cond);
            s->if_then = std::move(thenStmt);
            s->if_else = std::move(elseStmt);
            return s;
        }

        // assignment statement
        auto lhs = parseExpression();
        if (match(TokenKind::Symbol, "<") && tokens_[idx_ + 1].text == "=") {
            get(); // <
            get(); // =
            auto rhs = parseExpression();
            expect(TokenKind::Symbol, ";");
            auto s = std::make_unique<Statement>(StmtKind::NonBlockingAssign);
            s->lhs = std::move(lhs);
            s->rhs = std::move(rhs);
            return s;
        } else if (match(TokenKind::Symbol, "=")) {
            get();
            auto rhs = parseExpression();
            expect(TokenKind::Symbol, ";");
            auto s = std::make_unique<Statement>(StmtKind::BlockingAssign);
            s->lhs = std::move(lhs);
            s->rhs = std::move(rhs);
            return s;
        }

        throw std::runtime_error("Unsupported statement near token: " + peek().text);
    }

    std::unique_ptr<Expression> parseExpression() {
        auto lhs = parsePrimary();
        return parseBinaryRHS(0, std::move(lhs));
    }

    std::unique_ptr<Expression> parsePrimary() {
        if (match(TokenKind::Identifier)) {
            auto t = get();
            auto e = std::make_unique<Expression>(ExprKind::Identifier);
            e->ident = t.text;
            return e;
        }
        if (match(TokenKind::Number)) {
            auto t = get();
            auto e = std::make_unique<Expression>(ExprKind::Number);
            e->number_literal = t.text;
            return e;
        }
        if (match(TokenKind::Symbol, "(")) {
            get();
            auto e = parseExpression();
            expect(TokenKind::Symbol, ")");
            return e;
        }
        throw std::runtime_error("Expected expression near token: " + peek().text);
    }

    std::unique_ptr<Expression> parseBinaryRHS(int exprPrec, std::unique_ptr<Expression> lhs) {
        while (true) {
            if (peek().kind != TokenKind::Symbol) return lhs;
            std::string op = peek().text;
            int tokPrec = getBinOpPrecedence(op);
            if (tokPrec < exprPrec) return lhs;

            get(); // consume op
            auto rhs = parsePrimary();
            if (peek().kind == TokenKind::Symbol) {
                int nextPrec = getBinOpPrecedence(peek().text);
                if (nextPrec > tokPrec) {
                    rhs = parseBinaryRHS(tokPrec + 1, std::move(rhs));
                }
            }
            auto bin = std::make_unique<Expression>(ExprKind::Binary);
            bin->binary_op = mapBinaryOp(op);
            bin->lhs = std::move(lhs);
            bin->rhs = std::move(rhs);
            lhs = std::move(bin);
        }
    }
};

} // namespace sv
