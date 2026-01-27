#ifndef __LEXER_HPP__
#define __LEXER_HPP__

#include <string>
#include <vector>
#include "ast.hpp"

namespace sv {

class Lexer {
public:
    Lexer(const std::string &file, const std::string &input);
    std::vector<Token> lex();

private:
    std::string file_;
    std::string input_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    bool eof() const;
    char peek() const;
    char get();
    void skipWhitespaceAndComments();

    Token makeToken(TokenKind kind, const std::string &text);
    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexSymbol();
};

} // namespace sv

#endif // __LEXER_HPP__
