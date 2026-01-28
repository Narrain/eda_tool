#include <stdexcept>
#include "parser.hpp"

namespace sv {

// -----------------------------------------------------
// ctor + token helpers
// -----------------------------------------------------

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens) {}

const Token &Parser::peek() const {
    if (idx_ >= tokens_.size()) return tokens_.back();
    return tokens_[idx_];
}

const Token &Parser::get() {
    const Token &t = peek();
    if (idx_ < tokens_.size()) idx_++;
    return t;
}

bool Parser::match(TokenKind kind, const std::string &text) const {
    const Token &t = peek();
    if (t.kind != kind) return false;
    if (!text.empty() && t.text != text) return false;
    return true;
}

const Token &Parser::expect(TokenKind kind, const std::string &text) {
    if (!match(kind, text)) {
        const Token &t = peek();
        throw std::runtime_error(
            "Parse error near token '" + t.text + "' at " +
            t.loc.file + ":" + std::to_string(t.loc.line) +
            ":" + std::to_string(t.loc.column));
    }
    return get();
}

bool Parser::isSymbol(const std::string &s) const {
    const Token &t = peek();
    return t.kind == TokenKind::Symbol && t.text == s;
}

// -----------------------------------------------------
// precedence + op mapping
// -----------------------------------------------------

int Parser::getBinOpPrecedence(const std::string &op) const {
    // lower number => lower precedence
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=" || op == "===" || op == "!==") return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
    if (op == "<<" || op == ">>" || op == "<<<" || op == ">>>") return 5;
    if (op == "+" || op == "-") return 6;
    if (op == "*" || op == "/" || op == "%") return 7;
    if (op == "&" || op == "|" || op == "^") return 8;
    return -1;
}

BinaryOp Parser::mapBinaryOp(const std::string &op) const {
    if (op == "+") return BinaryOp::Add;
    if (op == "-") return BinaryOp::Sub;
    if (op == "*") return BinaryOp::Mul;
    if (op == "/") return BinaryOp::Div;
    if (op == "%") return BinaryOp::Mod;

    if (op == "&") return BinaryOp::BitAnd;
    if (op == "|") return BinaryOp::BitOr;
    if (op == "^") return BinaryOp::BitXor;

    if (op == "&&") return BinaryOp::LogicalAnd;
    if (op == "||") return BinaryOp::LogicalOr;

    if (op == "==") return BinaryOp::Eq;
    if (op == "!=") return BinaryOp::Neq;
    if (op == "===") return BinaryOp::CaseEq;
    if (op == "!==") return BinaryOp::CaseNeq;
    if (op == "<") return BinaryOp::Lt;
    if (op == ">") return BinaryOp::Gt;
    if (op == "<=") return BinaryOp::Le;
    if (op == ">=") return BinaryOp::Ge;

    if (op == "<<") return BinaryOp::Shl;
    if (op == ">>") return BinaryOp::Shr;
    if (op == "<<<") return BinaryOp::Ashl;
    if (op == ">>>") return BinaryOp::Ashr;

    throw std::runtime_error("Unknown binary operator: " + op);
}

// -----------------------------------------------------
// data types
// -----------------------------------------------------

DataType Parser::parseDataType() {
    DataType dt;
    if (match(TokenKind::Keyword, "wire")) {
        get();
        dt.kind = DataTypeKind::Wire;
    } else if (match(TokenKind::Keyword, "logic")) {
        get();
        dt.kind = DataTypeKind::Logic;
    } else if (match(TokenKind::Keyword, "reg")) {
        get();
        dt.kind = DataTypeKind::Reg;
    } else if (match(TokenKind::Keyword, "integer")) {
        get();
        dt.kind = DataTypeKind::Integer;
    } else {
        dt.kind = DataTypeKind::Unknown;
    }

    if (isSymbol("[")) {
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

// -----------------------------------------------------
// design + module
// -----------------------------------------------------

std::unique_ptr<Design> Parser::parseDesign() {
    auto design = std::make_unique<Design>();
    while (!match(TokenKind::EndOfFile)) {
        design->modules.push_back(parseModule());
    }
    return design;
}

std::unique_ptr<ModuleDecl> Parser::parseModule() {
    auto modTok = expect(TokenKind::Keyword, "module");
    auto nameTok = expect(TokenKind::Identifier);

    auto mod = std::make_unique<ModuleDecl>();
    mod->name = nameTok.text;
    mod->loc = modTok.loc;

    // port list
    if (isSymbol("(")) {
        get(); // (
        if (!isSymbol(")")) {
            while (true) {
                mod->ports.push_back(parsePortDecl());
                if (isSymbol(",")) {
                    get();
                    continue;
                }
                break;
            }
        }
        expect(TokenKind::Symbol, ")");
    }
    expect(TokenKind::Symbol, ";");

    // body
    while (!match(TokenKind::Keyword, "endmodule")) {
        mod->items.push_back(parseModuleItem());
    }
    expect(TokenKind::Keyword, "endmodule");
    return mod;
}

// -----------------------------------------------------
// ports + module items
// -----------------------------------------------------

std::unique_ptr<PortDecl> Parser::parsePortDecl() {
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
        const Token &t = peek();
        throw std::runtime_error("Expected port direction near '" + t.text + "'");
    }

    DataType dt = parseDataType();

    auto nameTok = expect(TokenKind::Identifier);
    auto p = std::make_unique<PortDecl>();
    p->dir = dir;
    p->type = dt;
    p->name = nameTok.text;
    p->loc = nameTok.loc;
    return p;
}

std::unique_ptr<ModuleItem> Parser::parseModuleItem() {
    // parameter / localparam
    if (match(TokenKind::Keyword, "parameter") ||
        match(TokenKind::Keyword, "localparam")) {
        auto p = parseParamDecl();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::ParamDecl);
        item->param_decl = std::move(p);
        return item;
    }

    // continuous assign
    if (match(TokenKind::Keyword, "assign")) {
        auto ca = parseContinuousAssign();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::ContinuousAssign);
        item->cont_assign = std::move(ca);
        return item;
    }

    // always
    if (match(TokenKind::Keyword, "always") ||
        match(TokenKind::Keyword, "always_ff") ||
        match(TokenKind::Keyword, "always_comb") ||
        match(TokenKind::Keyword, "always_latch")) {
        auto a = parseAlways();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::Always);
        item->always = std::move(a);
        return item;
    }

    // initial
    if (match(TokenKind::Keyword, "initial")) {
        auto init = parseInitial();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::Initial);
        item->initial = std::move(init);
        return item;
    }

    // net/var decl
    if (match(TokenKind::Keyword, "wire") ||
        match(TokenKind::Keyword, "logic") ||
        match(TokenKind::Keyword, "reg") ||
        match(TokenKind::Keyword, "integer")) {
        DataType dt = parseDataType();
        auto nameTok = expect(TokenKind::Identifier);

        // decide net vs var by type kind
        if (dt.kind == DataTypeKind::Wire || dt.kind == DataTypeKind::Logic) {
            auto net = parseNetDecl(dt, nameTok);
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
            item->net_decl = std::move(net);
            return item;
        } else {
            auto var = parseVarDecl(dt, nameTok);
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::VarDecl);
            item->var_decl = std::move(var);
            return item;
        }
    }

    // instance: module_name inst_name (...)
    if (match(TokenKind::Identifier)) {
        size_t saveIdx = idx_;
        auto modNameTok = get();
        if (match(TokenKind::Identifier)) {
            auto instNameTok = get();
            if (isSymbol("(")) {
                auto inst = std::make_unique<Instance>();
                inst->module_name = modNameTok.text;
                inst->instance_name = instNameTok.text;
                inst->loc = modNameTok.loc;

                get(); // (
                if (!isSymbol(")")) {
                    while (true) {
                        InstancePortConn conn;
                        if (isSymbol(".")) {
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
                        if (isSymbol(",")) {
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
        // not an instance, rewind
        idx_ = saveIdx;
    }

    const Token &t = peek();
    throw std::runtime_error("Unsupported or invalid module item near token '" +
                             t.text + "' at " + t.loc.file + ":" +
                             std::to_string(t.loc.line) + ":" +
                             std::to_string(t.loc.column));
}

// -----------------------------------------------------
// params + decl helpers + continuous assign
// -----------------------------------------------------

std::unique_ptr<ParamDecl> Parser::parseParamDecl() {
    auto kw = get(); // parameter/localparam
    auto nameTok = expect(TokenKind::Identifier);
    expect(TokenKind::Symbol, "=");
    auto expr = parseExpression();
    expect(TokenKind::Symbol, ";");

    auto p = std::make_unique<ParamDecl>();
    p->name = nameTok.text;
    p->value = std::move(expr);
    p->loc = kw.loc;
    return p;
}

std::unique_ptr<NetDecl> Parser::parseNetDecl(const DataType &dt, const Token &nameTok) {
    auto net = std::make_unique<NetDecl>();
    net->type = dt;
    net->name = nameTok.text;
    net->loc = nameTok.loc;

    if (isSymbol("=")) {
        get();
        net->init = parseExpression();
    }
    expect(TokenKind::Symbol, ";");
    return net;
}

std::unique_ptr<VarDecl> Parser::parseVarDecl(const DataType &dt, const Token &nameTok) {
    auto var = std::make_unique<VarDecl>();
    var->type = dt;
    var->name = nameTok.text;
    var->loc = nameTok.loc;

    if (isSymbol("=")) {
        get();
        var->init = parseExpression();
    }
    expect(TokenKind::Symbol, ";");
    return var;
}

std::unique_ptr<ContinuousAssign> Parser::parseContinuousAssign() {
    auto kw = expect(TokenKind::Keyword, "assign");
    auto lhs = parseExpression();
    expect(TokenKind::Symbol, "=");
    auto rhs = parseExpression();
    expect(TokenKind::Symbol, ";");

    auto c = std::make_unique<ContinuousAssign>();
    c->lhs = std::move(lhs);
    c->rhs = std::move(rhs);
    c->loc = kw.loc;
    return c;
}

// -----------------------------------------------------
// always / initial
// -----------------------------------------------------

std::unique_ptr<AlwaysConstruct> Parser::parseAlways() {
    auto kw = get();
    auto a = std::make_unique<AlwaysConstruct>();
    a->loc = kw.loc;

    if (kw.text == "always_ff") a->kind = AlwaysKind::AlwaysFF;
    else if (kw.text == "always_comb") a->kind = AlwaysKind::AlwaysComb;
    else if (kw.text == "always_latch") a->kind = AlwaysKind::AlwaysLatch;
    else a->kind = AlwaysKind::Always;

    if (isSymbol("@")) {
        get();
        expect(TokenKind::Symbol, "(");
        parseSensitivityList(*a);
        expect(TokenKind::Symbol, ")");
    }

    a->body = parseStatementOrBlock();
    return a;
}

std::unique_ptr<InitialConstruct> Parser::parseInitial() {
    auto kw = get(); // initial
    auto init = std::make_unique<InitialConstruct>();
    init->loc = kw.loc;
    init->body = parseStatementOrBlock();
    return init;
}

void Parser::parseSensitivityList(AlwaysConstruct &a) {
    // @* or @(*)
    if (isSymbol("*")) {
        get();
        SensitivityItem item;
        item.star = true;
        a.sensitivity_list.push_back(std::move(item));
        return;
    }

    while (true) {
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
        a.sensitivity_list.push_back(std::move(item));

        if (match(TokenKind::Keyword, "or") || isSymbol(",")) {
            get();
            continue;
        }
        break;
    }
}

// -----------------------------------------------------
// statements
// -----------------------------------------------------

std::unique_ptr<Statement> Parser::parseStatementOrBlock() {
    if (match(TokenKind::Keyword, "begin")) {
        auto beginTok = get();
        auto blk = std::make_unique<Statement>(StmtKind::Block);
        blk->loc = beginTok.loc;
        while (!match(TokenKind::Keyword, "end")) {
            blk->block_stmts.push_back(parseStatement());
        }
        expect(TokenKind::Keyword, "end");
        return blk;
    }
    return parseStatement();
}

std::unique_ptr<Statement> Parser::parseIfStatement() {
    auto ifTok = expect(TokenKind::Keyword, "if");
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
    s->loc = ifTok.loc;
    s->if_cond = std::move(cond);
    s->if_then = std::move(thenStmt);
    s->if_else = std::move(elseStmt);
    return s;
}

bool Parser::isCaseKeyword() const {
    return match(TokenKind::Keyword, "case") ||
           match(TokenKind::Keyword, "casez") ||
           match(TokenKind::Keyword, "casex");
}

std::unique_ptr<Statement> Parser::parseCaseStatement() {
    auto kw = get(); // case/casez/casex
    CaseKind ck = CaseKind::Case;
    if (kw.text == "casez") ck = CaseKind::CaseZ;
    else if (kw.text == "casex") ck = CaseKind::CaseX;

    expect(TokenKind::Symbol, "(");
    auto expr = parseExpression();
    expect(TokenKind::Symbol, ")");

    auto s = std::make_unique<Statement>(StmtKind::Case);
    s->loc = kw.loc;
    s->case_kind = ck;
    s->case_expr = std::move(expr);

    while (!match(TokenKind::Keyword, "endcase")) {
        CaseItem item;
        if (match(TokenKind::Keyword, "default")) {
            get();
            expect(TokenKind::Symbol, ":");
        } else {
            while (true) {
                item.matches.push_back(parseExpression());
                if (isSymbol(",")) {
                    get();
                    continue;
                }
                break;
            }
            expect(TokenKind::Symbol, ":");
        }
        item.stmt = parseStatementOrBlock();
        s->case_items.push_back(std::move(item));
    }
    expect(TokenKind::Keyword, "endcase");
    return s;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    // if
    if (match(TokenKind::Keyword, "if")) {
        return parseIfStatement();
    }

    // case/casez/casex
    if (isCaseKeyword()) {
        return parseCaseStatement();
    }

    // null statement
    if (isSymbol(";")) {
        auto semi = get();
        auto s = std::make_unique<Statement>(StmtKind::Null);
        s->loc = semi.loc;
        return s;
    }

    // assignment-like statement
    auto lhs = parseExpression();
    if (isSymbol("<=")) {
        auto tok = get();
        auto rhs = parseExpression();
        expect(TokenKind::Symbol, ";");
        auto s = std::make_unique<Statement>(StmtKind::NonBlockingAssign);
        s->loc = tok.loc;
        s->lhs = std::move(lhs);
        s->rhs = std::move(rhs);
        return s;
    } else if (isSymbol("=")) {
        auto tok = get();
        auto rhs = parseExpression();
        expect(TokenKind::Symbol, ";");
        auto s = std::make_unique<Statement>(StmtKind::BlockingAssign);
        s->loc = tok.loc;
        s->lhs = std::move(lhs);
        s->rhs = std::move(rhs);
        return s;
    }

    const Token &t = peek();
    throw std::runtime_error("Unsupported statement near token '" +
                             t.text + "' at " + t.loc.file + ":" +
                             std::to_string(t.loc.line) + ":" +
                             std::to_string(t.loc.column));
}

// -----------------------------------------------------
// expressions
// -----------------------------------------------------

std::unique_ptr<Expression> Parser::parseExpression() {
    auto lhs = parseUnary();
    lhs = parseBinaryRHS(0, std::move(lhs));
    if (isSymbol("?")) {
        lhs = parseTernaryRHS(std::move(lhs));
    }
    return lhs;
}

std::unique_ptr<Expression> Parser::parsePrimary() {
    const Token &t = peek();
    if (t.kind == TokenKind::Identifier) {
        auto tok = get();
        auto e = std::make_unique<Expression>(ExprKind::Identifier);
        e->loc = tok.loc;
        e->ident = tok.text;
        return e;
    }
    if (t.kind == TokenKind::Number) {
        auto tok = get();
        auto e = std::make_unique<Expression>(ExprKind::Number);
        e->loc = tok.loc;
        e->literal = tok.text;
        return e;
    }
    if (t.kind == TokenKind::String) {
        auto tok = get();
        auto e = std::make_unique<Expression>(ExprKind::String);
        e->loc = tok.loc;
        e->literal = tok.text;
        return e;
    }
    if (isSymbol("(")) {
        get();
        auto e = parseExpression();
        expect(TokenKind::Symbol, ")");
        return e;
    }
    if (isSymbol("{")) {
        return parseConcatenationOrReplication();
    }

    throw std::runtime_error("Expected expression near token '" +
                             t.text + "' at " + t.loc.file + ":" +
                             std::to_string(t.loc.line) + ":" +
                             std::to_string(t.loc.column));
}

std::unique_ptr<Expression> Parser::parseConcatenationOrReplication() {
    auto lbrace = expect(TokenKind::Symbol, "{");

    // replication: { N { a, b } }
    if (match(TokenKind::Number)) {
        size_t saveIdx = idx_;
        auto countTok = get();
        if (isSymbol("{")) {
            get(); // inner {
            auto rep = std::make_unique<Expression>(ExprKind::Replication);
            rep->loc = lbrace.loc;
            auto countExpr = std::make_unique<Expression>(ExprKind::Number);
            countExpr->loc = countTok.loc;
            countExpr->literal = countTok.text;
            rep->replicate_count = std::move(countExpr);

            while (!isSymbol("}")) {
                rep->replicate_elems.push_back(parseExpression());
                if (isSymbol(",")) {
                    get();
                    continue;
                }
                break;
            }
            expect(TokenKind::Symbol, "}");
            expect(TokenKind::Symbol, "}");
            return rep;
        }
        idx_ = saveIdx;
    }

    // concatenation
    auto cat = std::make_unique<Expression>(ExprKind::Concatenation);
    cat->loc = lbrace.loc;
    while (!isSymbol("}")) {
        cat->concat_elems.push_back(parseExpression());
        if (isSymbol(",")) {
            get();
            continue;
        }
        break;
    }
    expect(TokenKind::Symbol, "}");
    return cat;
}

std::unique_ptr<Expression> Parser::parseUnary() {
    if (isSymbol("+") || isSymbol("-") || isSymbol("!") || isSymbol("~")) {
        auto tok = get();
        auto e = std::make_unique<Expression>(ExprKind::Unary);
        e->loc = tok.loc;
        if (tok.text == "+") e->unary_op = UnaryOp::Plus;
        else if (tok.text == "-") e->unary_op = UnaryOp::Minus;
        else if (tok.text == "!") e->unary_op = UnaryOp::LogicalNot;
        else e->unary_op = UnaryOp::BitNot;
        e->unary_operand = parseUnary();
        return e;
    }
    return parsePrimary();
}

std::unique_ptr<Expression> Parser::parseBinaryRHS(int exprPrec, std::unique_ptr<Expression> lhs) {
    while (true) {
        const Token &t = peek();
        if (t.kind != TokenKind::Symbol) break;

        int tokPrec = getBinOpPrecedence(t.text);
        if (tokPrec < 0 || tokPrec < exprPrec) break;

        std::string op = t.text;
        get(); // consume op

        auto rhs = parseUnary();

        const Token &t2 = peek();
        if (t2.kind == TokenKind::Symbol) {
            int nextPrec = getBinOpPrecedence(t2.text);
            if (nextPrec > tokPrec) {
                rhs = parseBinaryRHS(tokPrec + 1, std::move(rhs));
            }
        }

        auto bin = std::make_unique<Expression>(ExprKind::Binary);
        bin->loc = t.loc;
        bin->binary_op = mapBinaryOp(op);
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

std::unique_ptr<Expression> Parser::parseTernaryRHS(std::unique_ptr<Expression> cond) {
    auto qTok = expect(TokenKind::Symbol, "?");
    auto thenExpr = parseExpression();
    expect(TokenKind::Symbol, ":");
    auto elseExpr = parseExpression();

    auto e = std::make_unique<Expression>(ExprKind::Ternary);
    e->loc = qTok.loc;
    e->cond = std::move(cond);
    e->then_expr = std::move(thenExpr);
    e->else_expr = std::move(elseExpr);
    return e;
}

} // namespace sv
