#ifndef __LEXER_HPP__
#define __LEXER_HPP__

#include <cctype>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

#include "ast.hpp"

namespace sv {

class Lexer {
public:
    Lexer(const std::string &file, const std::string &input)
        : file_(file), input_(input) {}

    std::vector<Token> lex();

private:
    std::string file_;
    std::string input_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    bool eof() const { return pos_ >= input_.size(); }
    char peek() const { return eof() ? '\0' : input_[pos_]; }
    char peekNext() const { return (pos_ + 1 < input_.size()) ? input_[pos_ + 1] : '\0'; }

    char get();

    void skipWhitespaceAndComments();

    Token makeToken(TokenKind kind, const std::string &text, int line, int col);

    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexString();
    Token lexSymbol();

    static bool isIdentStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
    }

    static bool isIdentChar(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
    }
};

} // namespace sv

#endif // __LEXER_HPP__
