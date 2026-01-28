#include "lexer.hpp"

#include <stdexcept>

namespace sv {

char Lexer::get() {
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

Token Lexer::makeToken(TokenKind kind, const std::string &text, int line, int col) {
    Token t;
    t.kind = kind;
    t.text = text;
    t.loc.file = file_;
    t.loc.line = line;
    t.loc.column = col;
    return t;
}

void Lexer::skipWhitespaceAndComments() {
    while (!eof()) {
        char c = peek();
        // whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            get();
            continue;
        }
        // line or block comments
        if (c == '/' && !eof()) {
            char n = peekNext();
            // //
            if (n == '/') {
                get(); // '/'
                get(); // '/'
                while (!eof() && peek() != '\n') {
                    get();
                }
                continue;
            }
            // /*
            if (n == '*') {
                get(); // '/'
                get(); // '*'
                while (!eof()) {
                    char d = get();
                    if (d == '*' && !eof() && peek() == '/') {
                        get(); // '/'
                        break;
                    }
                }
                continue;
            }
        }
        break;
    }
}

Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line_;
    int startCol = col_;
    std::string s;
    while (!eof()) {
        char c = peek();
        if (isIdentChar(c)) {
            s.push_back(get());
        } else {
            break;
        }
    }

    static const std::unordered_set<std::string> keywords = {
        // modules
        "module", "endmodule",

        // ports and nets
        "input", "output", "inout",
        "wire", "logic", "reg", "integer",

        // params
        "parameter", "localparam",

        // continuous assign
        "assign",

        // procedural
        "always", "always_ff", "always_comb", "always_latch",
        "initial",
        "begin", "end",
        "if", "else",
        "case", "casez", "casex", "endcase",
        "default",

        // event control
        "posedge", "negedge",

        // generate (future use)
        "generate", "endgenerate",

        // logical aliases
        "or", "and", "not"
    };

    if (keywords.find(s) != keywords.end()) {
        return makeToken(TokenKind::Keyword, s, startLine, startCol);
    }
    return makeToken(TokenKind::Identifier, s, startLine, startCol);
}

Token Lexer::lexNumber() {
    int startLine = line_;
    int startCol = col_;
    std::string s;

    // SystemVerilog number: [size]'[base][digits] or plain decimal
    while (!eof()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '\'' || c == '_' ||
            c == 'x' || c == 'X' ||
            c == 'z' || c == 'Z') {
            s.push_back(get());
        } else {
            break;
        }
    }
    return makeToken(TokenKind::Number, s, startLine, startCol);
}

Token Lexer::lexString() {
    int startLine = line_;
    int startCol = col_;
    std::string s;
    get(); // consume opening "

    while (!eof()) {
        char c = get();
        if (c == '"') {
            break;
        }
        if (c == '\\' && !eof()) {
            // simple escape handling
            char n = get();
            s.push_back('\\');
            s.push_back(n);
        } else {
            s.push_back(c);
        }
    }
    return makeToken(TokenKind::String, s, startLine, startCol);
}

Token Lexer::lexSymbol() {
    int startLine = line_;
    int startCol = col_;
    char c = get();
    char n1 = peek();
    char n2 = (pos_ + 1 < input_.size()) ? input_[pos_ + 1] : '\0';

    // 3-char operators
    if (c == '<' && n1 == '<' && n2 == '<') {
        get(); get();
        return makeToken(TokenKind::Symbol, "<<<", startLine, startCol);
    }
    if (c == '>' && n1 == '>' && n2 == '>') {
        get(); get();
        return makeToken(TokenKind::Symbol, ">>>", startLine, startCol);
    }
    if (c == '=' && n1 == '=' && n2 == '=') {
        get(); get();
        return makeToken(TokenKind::Symbol, "===", startLine, startCol);
    }
    if (c == '!' && n1 == '=' && n2 == '=') {
        get(); get();
        return makeToken(TokenKind::Symbol, "!==", startLine, startCol);
    }

    // 2-char operators
    if (c == '<' && n1 == '<') {
        get();
        return makeToken(TokenKind::Symbol, "<<", startLine, startCol);
    }
    if (c == '>' && n1 == '>') {
        get();
        return makeToken(TokenKind::Symbol, ">>", startLine, startCol);
    }
    if (c == '=' && n1 == '=') {
        get();
        return makeToken(TokenKind::Symbol, "==", startLine, startCol);
    }
    if (c == '!' && n1 == '=') {
        get();
        return makeToken(TokenKind::Symbol, "!=", startLine, startCol);
    }
    if (c == '=' && n1 == '>') {
        get();
        return makeToken(TokenKind::Symbol, "=>", startLine, startCol);
    }
    if (c == '-' && n1 == '>') {
        get();
        return makeToken(TokenKind::Symbol, "->", startLine, startCol);
    }
    if (c == '&' && n1 == '&') {
        get();
        return makeToken(TokenKind::Symbol, "&&", startLine, startCol);
    }
    if (c == '|' && n1 == '|') {
        get();
        return makeToken(TokenKind::Symbol, "||", startLine, startCol);
    }
    if (c == '<' && n1 == '=') {
        get();
        return makeToken(TokenKind::Symbol, "<=", startLine, startCol);
    }
    if (c == '>' && n1 == '=') {
        get();
        return makeToken(TokenKind::Symbol, ">=", startLine, startCol);
    }

    // single-char symbols
    std::string s(1, c);
    switch (c) {
    case '+': case '-': case '*': case '/': case '%':
    case '&': case '|': case '^': case '~':
    case '!':
    case '<': case '>':
    case '=':
    case '?': case ':':
    case '@': case '#':
    case '(': case ')':
    case '[': case ']':
    case '{': case '}':
    case '.':
    case ',': case ';':
        return makeToken(TokenKind::Symbol, s, startLine, startCol);
    case '"':
        // normally handled by lexString() in lex()
        break;
    default:
        break;
    }

    throw std::runtime_error("Unknown symbol '" + s + "' at " +
                             file_ + ":" + std::to_string(startLine) +
                             ":" + std::to_string(startCol));
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (eof()) {
            tokens.push_back(makeToken(TokenKind::EndOfFile, "", line_, col_));
            break;
        }
        char c = peek();
        if (isIdentStart(c)) {
            tokens.push_back(lexIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
        } else if (c == '"') {
            tokens.push_back(lexString());
        } else {
            tokens.push_back(lexSymbol());
        }
    }
    return tokens;
}

} // namespace sv
