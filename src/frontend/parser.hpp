#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"

namespace sv {

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens);

    std::unique_ptr<Design> parseDesign();
    std::unique_ptr<ModuleItem> parseGenerateConstruct();
    std::unique_ptr<GenerateItem> parseGenerateItem();
    std::unique_ptr<GenerateItem> parseGenerateFor();
    std::unique_ptr<GenerateItem> parseGenerateBlock();

    void skipGenerateJunk();

private:
    const std::vector<Token> &tokens_;
    size_t idx_ = 0;

    // token helpers
    const Token &peek() const;
    const Token &get();
    bool match(TokenKind kind, const std::string &text = "") const;
    const Token &expect(TokenKind kind, const std::string &text = "");

    bool isSymbol(const std::string &s) const;

    // precedence + op mapping
    int getBinOpPrecedence(const std::string &op) const;
    BinaryOp mapBinaryOp(const std::string &op) const;

    // data types
    DataType parseDataType();

    // top level
    std::unique_ptr<ModuleDecl> parseModule();

    // ports and items
    std::unique_ptr<PortDecl> parsePortDecl();
    std::unique_ptr<ModuleItem> parseModuleItem();

    // declarations
    std::unique_ptr<ParamDecl> parseParamDecl();
    std::unique_ptr<NetDecl> parseNetDecl(const DataType &dt, const Token &nameTok);
    std::unique_ptr<VarDecl> parseVarDecl(const DataType &dt, const Token &nameTok);

    // continuous assign
    std::unique_ptr<ContinuousAssign> parseContinuousAssign();

    // always / initial
    std::unique_ptr<AlwaysConstruct> parseAlways();
    std::unique_ptr<InitialConstruct> parseInitial();
    void parseSensitivityList(AlwaysConstruct &a);

    // statements
    std::unique_ptr<Statement> parseStatementOrBlock();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseIfStatement();
    std::unique_ptr<Statement> parseCaseStatement();

    // expressions
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parsePrimary();
    std::unique_ptr<Expression> parseConcatenationOrReplication();
    std::unique_ptr<Expression> parseUnary();
    std::unique_ptr<Expression> parseBinaryRHS(int exprPrec, std::unique_ptr<Expression> lhs);
    std::unique_ptr<Expression> parseTernaryRHS(std::unique_ptr<Expression> cond);

    // helpers
    bool isCaseKeyword() const;
};

} // namespace sv

#endif // __PARSER_HPP__
