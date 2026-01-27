#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include <vector>
#include <memory>
#include "ast.hpp"

namespace sv {

class Parser {
public:
    Parser(const std::vector<Token> &tokens);
    std::unique_ptr<Design> parseDesign();

private:
    const std::vector<Token> &tokens_;
    size_t idx_ = 0;

    const Token &peek() const;
    const Token &get();
    bool match(TokenKind kind, const std::string &text = "");
    const Token &expect(TokenKind kind, const std::string &text = "");

    std::unique_ptr<ModuleDecl> parseModule();
    std::unique_ptr<PortDecl> parsePortDecl();
    std::unique_ptr<ModuleItem> parseModuleItem();
    std::unique_ptr<ParamDecl> parseParamDecl();
    std::unique_ptr<ContinuousAssign> parseContinuousAssign();
    std::unique_ptr<AlwaysConstruct> parseAlways();
    std::unique_ptr<Statement> parseStatementOrBlock();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parsePrimary();
    std::unique_ptr<Expression> parseBinaryRHS(int exprPrec, std::unique_ptr<Expression> lhs);

    int getBinOpPrecedence(const std::string &op);
    BinaryOp mapBinaryOp(const std::string &op);
    DataType parseDataType(const std::string &kw);
};

} // namespace sv

#endif // __PARSER_HPP__
