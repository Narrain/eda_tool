#include <cctype>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>

#include "ast.hpp"

namespace sv {

class Lexer {
public:
    Lexer(const std::string &file, const std::string &input)
        : file_(file), input_(input) {}

    std::vector<Token> lex() {
        std::vector<Token> tokens;
        while (true) {
            skipWhitespaceAndComments();
            if (eof()) {
                tokens.push_back(makeToken(TokenKind::EndOfFile, ""));
                break;
            }
            char c = peek();
            if (isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                tokens.push_back(lexIdentifierOrKeyword());
            } else if (isdigit(static_cast<unsigned char>(c))) {
                tokens.push_back(lexNumber());
            } else {
                tokens.push_back(lexSymbol());
            }
        }
        return tokens;
    }

private:
    std::string file_;
    std::string input_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    bool eof() const { return pos_ >= input_.size(); }
    char peek() const { return eof() ? '\0' : input_[pos_]; }
    char get() {
        if (eof()) return '\0';
        char c = input_[pos_++];
        if (c == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
        return c;
    }

    void skipWhitespaceAndComments() {
        while (!eof()) {
            char c = peek();
            if (isspace(static_cast<unsigned char>(c))) {
                get();
                continue;
            }
            if (c == '/' && pos_ + 1 < input_.size()) {
                char n = input_[pos_ + 1];
                if (n == '/') {
                    get(); get();
                    while (!eof() && peek() != '\n') get();
                    continue;
                } else if (n == '*') {
                    get(); get();
                    while (!eof()) {
                        char d = get();
                        if (d == '*' && !eof() && peek() == '/') {
                            get();
                            break;
                        }
                    }
                    continue;
                }
            }
            break;
        }
    }

    Token makeToken(TokenKind kind, const std::string &text) {
        Token t;
        t.kind = kind;
        t.text = text;
        t.loc.file = file_;
        t.loc.line = line_;
        t.loc.column = col_;
        return t;
    }

    Token lexIdentifierOrKeyword() {
        std::string s;
        while (!eof()) {
            char c = peek();
            if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
                s.push_back(get());
            } else {
                break;
            }
        }
        static const std::unordered_map<std::string, bool> keywords = {
            {"module", true}, {"endmodule", true},
            {"input", true}, {"output", true}, {"inout", true},
            {"wire", true}, {"logic", true}, {"reg", true},
            {"parameter", true}, {"localparam", true},
            {"assign", true},
            {"always", true}, {"always_ff", true}, {"always_comb", true}, {"always_latch", true},
            {"begin", true}, {"end", true},
            {"if", true}, {"else", true},
            {"posedge", true}, {"negedge", true},
            {"generate", true}, {"endgenerate", true}
        };
        if (keywords.count(s)) {
            return makeToken(TokenKind::Keyword, s);
        }
        return makeToken(TokenKind::Identifier, s);
    }

    Token lexNumber() {
        std::string s;
        while (!eof()) {
            char c = peek();
            if (isalnum(static_cast<unsigned char>(c)) || c == '\'' || c == '_') {
                s.push_back(get());
            } else {
                break;
            }
        }
        return makeToken(TokenKind::Number, s);
    }

    Token lexSymbol() {
        char c = get();
        std::string s(1, c);

        if (!eof()) {
            char n = peek();
            if ((c == '=' && n == '=') ||
                (c == '!' && n == '=') ||
                (c == '&' && n == '&') ||
                (c == '|' && n == '|') ||
                (c == '<' && n == '=') ||
                (c == '>' && n == '=')) {
                s.push_back(get());
            }
        }

        return makeToken(TokenKind::Symbol, s);
    }
};

} // namespace sv
