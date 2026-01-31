// src/frontend/parser.cpp
#include <stdexcept>
#include <iostream>
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

    // parameter list #(parameter ...)
    if (isSymbol("#")) {
        get(); // '#'
        expect(TokenKind::Symbol, "(");

        while (!isSymbol(")")) {
            expect(TokenKind::Keyword, "parameter");
            auto pnameTok = expect(TokenKind::Identifier);
            expect(TokenKind::Symbol, "=");
            auto expr = parseExpression();

            auto p = std::make_unique<ParamDecl>();
            p->name = pnameTok.text;
            p->value = std::move(expr);
            p->loc = pnameTok.loc;
            mod->params.push_back(std::move(p));

            if (isSymbol(",")) {
                get();
                continue;
            }
            break;
        }

        expect(TokenKind::Symbol, ")");
    }

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
    std::cerr << "MI: '" << peek().text
              << "' kind=" << (int)peek().kind
              << " at " << peek().loc.line << ":" << peek().loc.column << "\n";

    // bare generate-for at module level
    if (peek().text == "for") {
        auto gc = std::make_unique<GenerateConstruct>();
        gc->loc = peek().loc;
        gc->item = parseGenerateFor();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::Generate);
        item->loc = gc->loc;
        item->gen = std::move(gc);
        return item;
    }

    // parameter / localparam
    if (match(TokenKind::Keyword, "parameter") ||
        match(TokenKind::Keyword, "localparam")) {
        auto p = parseParamDecl();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::ParamDecl);
        item->loc = p->loc;
        item->param_decl = std::move(p);
        return item;
    }

    // continuous assign
    if (match(TokenKind::Keyword, "assign")) {
        auto ca = parseContinuousAssign();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::ContinuousAssign);
        item->loc = ca->loc;
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
        item->loc = a->loc;
        item->always = std::move(a);
        return item;
    }

    // initial
    if (match(TokenKind::Keyword, "initial")) {
        auto init = parseInitial();
        auto item = std::make_unique<ModuleItem>(ModuleItemKind::Initial);
        item->loc = init->loc;
        item->initial = std::move(init);
        return item;
    }

    // generate ... endgenerate
    if (match(TokenKind::Keyword, "generate")) {
        return parseGenerateConstruct();
    }

    // genvar declaration
    if (peek().text == "genvar") {
        const Token &kw = get(); // 'genvar'
        auto nameTok = expect(TokenKind::Identifier);
        expect(TokenKind::Symbol, ";");

        auto gv = std::make_unique<GenVarDecl>();
        gv->name = nameTok.text;
        gv->loc = kw.loc;

        auto item = std::make_unique<ModuleItem>(ModuleItemKind::GenVarDecl);
        item->loc = kw.loc;
        item->genvar_decl = std::move(gv);
        return item;
    }

    // net/var decl
    if (match(TokenKind::Keyword, "wire") ||
        match(TokenKind::Keyword, "logic") ||
        match(TokenKind::Keyword, "reg") ||
        match(TokenKind::Keyword, "integer")) {
        DataType dt = parseDataType();
        auto nameTok = expect(TokenKind::Identifier);

        if (dt.kind == DataTypeKind::Wire || dt.kind == DataTypeKind::Logic) {
            auto net = parseNetDecl(dt, nameTok);
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::NetDecl);
            item->loc = net->loc;
            item->net_decl = std::move(net);
            return item;
        } else {
            auto var = parseVarDecl(dt, nameTok);
            auto item = std::make_unique<ModuleItem>(ModuleItemKind::VarDecl);
            item->loc = var->loc;
            item->var_decl = std::move(var);
            return item;
        }
    }

    // instance
    if (match(TokenKind::Identifier)) {
        size_t saveIdx = idx_;
        auto modNameTok = get();

        std::vector<ParamOverride> overrides;

        if (isSymbol("#")) {
            get(); // '#'
            expect(TokenKind::Symbol, "(");

            while (!isSymbol(")")) {
                expect(TokenKind::Symbol, ".");
                auto pnameTok = expect(TokenKind::Identifier);
                expect(TokenKind::Symbol, "(");
                auto expr = parseExpression();
                expect(TokenKind::Symbol, ")");

                ParamOverride ov;
                ov.name = pnameTok.text;
                ov.value = std::move(expr);
                overrides.push_back(std::move(ov));

                if (isSymbol(",")) {
                    get();
                    continue;
                }
                break;
            }

            expect(TokenKind::Symbol, ")");
        }

        if (match(TokenKind::Identifier)) {
            auto instNameTok = get();

            if (isSymbol("(")) {
                auto inst = std::make_unique<Instance>();
                inst->module_name = modNameTok.text;
                inst->instance_name = instNameTok.text;
                inst->loc = modNameTok.loc;

                inst->param_overrides = std::move(overrides);

                get(); // '('
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
                item->loc = inst->loc;
                item->instance = std::move(inst);
                return item;
            }
        }

        idx_ = saveIdx; // rewind
    }

    const Token &t = peek();
    throw std::runtime_error(
        "Unsupported or invalid module item near token '" +
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

    // event control: always @(...)
    if (isSymbol("@")) {
        get();
        expect(TokenKind::Symbol, "(");
        parseSensitivityList(*a);
        expect(TokenKind::Symbol, ")");
        a->body = parseStatementOrBlock();
        return a;
    }

    // delay control: always #5 ...
    if (peek().text == "#") {
        auto delayStmt = parseStatement();   // hits delay rule above
        auto blk = std::make_unique<Statement>(StmtKind::Block);
        blk->loc = delayStmt->loc;
        blk->block_stmts.push_back(std::move(delayStmt));
        a->body = std::move(blk);
        return a;
    }

    // normal body
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

std::unique_ptr<Statement> Parser::parseStatement() {
    // MUST BE FIRST — delay control: #expr statement
    if (peek().text == "#") {
        auto hashTok = get(); // '#'
        auto delayExpr = parseExpression();
        auto stmt = parseStatement();

        auto s = std::make_unique<Statement>(StmtKind::Delay);
        s->loc = hashTok.loc;
        s->delay_expr = std::move(delayExpr);
        s->delay_stmt = std::move(stmt);
        return s;
    }

    std::cerr << "DEBUG TOKEN: '" << peek().text
              << "' kind=" << (int)peek().kind
              << " at " << peek().loc.line << ":" << peek().loc.column << "\n";

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
    }

    if (isSymbol("=")) {
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

    if (isSymbol(";")) {
        auto semi = get();
        auto s = std::make_unique<Statement>(StmtKind::ExprStmt);
        s->loc = semi.loc;
        s->expr = std::move(lhs);
        return s;
    }

    throw std::runtime_error(
        "Unsupported statement near token '" +
        t.text + "' at " + t.loc.file + ":" +
        std::to_string(t.loc.line) + ":" +
        std::to_string(t.loc.column));
}

std::unique_ptr<Statement> Parser::parseStatementOrBlock() {
    if (match(TokenKind::Keyword, "begin")) {
        auto beginTok = get();

        auto blk = std::make_unique<Statement>(StmtKind::Block);
        blk->loc = beginTok.loc;

        while (true) {
            if (match(TokenKind::Keyword, "end"))
                break;

            if (isSymbol(";")) { 
                get(); 
                continue; 
            }
            blk->block_stmts.push_back(parseStatement());
        }

        expect(TokenKind::Keyword, "end");

        if (isSymbol(":")) {
            get(); // ':'
            expect(TokenKind::Identifier); // label
        }

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

        auto base = std::make_unique<Expression>(ExprKind::Identifier);
        base->loc = tok.loc;
        base->ident = tok.text;

        while (isSymbol("[")) {
            get(); // '['
            auto index = parseExpression();
            expect(TokenKind::Symbol, "]");

            auto sel = std::make_unique<Expression>(ExprKind::BitSelect);
            sel->loc = tok.loc;
            sel->lhs = std::move(base);
            sel->rhs = std::move(index);

            base = std::move(sel);
        }

        return base;
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

    throw std::runtime_error(
        "Expected expression near token '" + t.text + "' at " +
        t.loc.file + ":" + std::to_string(t.loc.line) + ":" +
        std::to_string(t.loc.column));
}

std::unique_ptr<Expression> Parser::parseConcatenationOrReplication() {
    auto lbrace = expect(TokenKind::Symbol, "{");

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

// -----------------------------------------------------
// generate constructs
// -----------------------------------------------------

std::unique_ptr<ModuleItem> Parser::parseGenerateConstruct() {
    auto genTok = expect(TokenKind::Keyword, "generate");

    auto gc = std::make_unique<GenerateConstruct>();
    gc->loc = genTok.loc;
    gc->item = parseGenerateItem();

    expect(TokenKind::Keyword, "endgenerate");

    auto item = std::make_unique<ModuleItem>(ModuleItemKind::Generate);
    item->loc = genTok.loc;
    item->gen = std::move(gc);
    return item;
}

std::unique_ptr<GenerateItem> Parser::parseGenerateItem() {
    // For now we only support: for (...) begin ... end
    if (peek().text == "for") {
        return parseGenerateFor();
    }

    const Token &t = peek();
    throw std::runtime_error(
        "Unsupported generate item near token '" + t.text + "' at " +
        t.loc.file + ":" + std::to_string(t.loc.line) + ":" +
        std::to_string(t.loc.column));
}

std::unique_ptr<GenerateItem> Parser::parseGenerateFor() {
    if (peek().text != "for") {
        const Token &t = peek();
        throw std::runtime_error(
            "Expected 'for' in generate-for, got '" + t.text + "'");
    }
    auto forTok = get(); // 'for' (identifier token)

    expect(TokenKind::Symbol, "(");

    // ----------------------------------------
    // init: genvar_name = <expr>;
    // ----------------------------------------
    auto genvarTok = expect(TokenKind::Identifier);
    std::string genvar_name = genvarTok.text;

    expect(TokenKind::Symbol, "=");
    auto init_expr = parseExpression();
    expect(TokenKind::Symbol, ";");

    // ----------------------------------------
    // condition: <expr>;
    // (we'll interpret it later as i < limit, etc.)
    // ----------------------------------------
    auto cond_expr = parseExpression();
    expect(TokenKind::Symbol, ";");

    // ----------------------------------------
    // step: genvar_name = <expr>
    // ----------------------------------------
    auto step_lhs = expect(TokenKind::Identifier);
    if (step_lhs.text != genvar_name) {
        throw std::runtime_error(
            "Generate-for step must assign to same genvar '" +
            genvar_name + "'");
    }
    expect(TokenKind::Symbol, "=");
    auto step_expr = parseExpression();

    expect(TokenKind::Symbol, ")");

    // ----------------------------------------
    // body: begin [: label] <module items> end [: label]
    // ----------------------------------------
    auto beginTok = expect(TokenKind::Keyword, "begin");

    // Optional label after 'begin'
    if (isSymbol(":")) {
        get(); // ':'
        expect(TokenKind::Identifier); // label name
    }

    // Build a Block generate item to hold module items
    auto body = std::make_unique<GenerateItem>(GenItemKind::Block);
    body->loc = beginTok.loc;
    body->block = std::make_unique<GenerateBlock>();
    body->block->loc = beginTok.loc;

    while (!match(TokenKind::Keyword, "end")) {
        // Reuse normal module-item parser inside the generate block
        body->block->items.push_back(parseModuleItem());
    }

    expect(TokenKind::Keyword, "end");

    // Optional label after 'end' (end : gen_blk)
    if (isSymbol(":")) {
        get(); // ':'
        expect(TokenKind::Identifier); // label name
    }

    // ----------------------------------------
    // Build the For generate item
    // ----------------------------------------
    auto gi = std::make_unique<GenerateItem>(GenItemKind::For);
    gi->loc = forTok.loc;
    gi->genvar_name = genvar_name;
    gi->for_init = std::move(init_expr);
    gi->for_cond = std::move(cond_expr);
    gi->for_step = std::move(step_expr);
    gi->for_body = std::move(body);

    return gi;
}

} // namespace sv
