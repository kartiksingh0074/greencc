#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace greencc {

enum class TokenType {
    
    INTEGER_LIT, FLOAT_LIT, STRING_LIT, CHAR_LIT,

    
    IDENTIFIER,

    
    KW_INT, KW_FLOAT, KW_DOUBLE, KW_CHAR, KW_BOOL, KW_VOID, KW_STRING,
    KW_CONST, KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    KW_BREAK, KW_CONTINUE, KW_TRUE, KW_FALSE,
    KW_COUT, KW_CIN, KW_ENDL,
    KW_INCLUDE, KW_USING, KW_NAMESPACE, KW_STD,

    
    PLUS, MINUS, STAR, SLASH, PERCENT,           
    AMPERSAND, PIPE, CARET, TILDE,               
    LSHIFT, RSHIFT,                               
    PLUS_EQ, MINUS_EQ, STAR_EQ, SLASH_EQ,        
    EQ, EQ_EQ, BANG, BANG_EQ,                     
    LT, GT, LT_EQ, GT_EQ,                        
    AND_AND, OR_OR,                               
    PLUS_PLUS, MINUS_MINUS,                       

    
    LPAREN, RPAREN,     
    LBRACE, RBRACE,     
    LBRACKET, RBRACKET, 
    SEMICOLON,          
    COMMA,              
    DOT,                
    COLON,              
    SCOPE,              
    HASH,               

    
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType   type;
    std::string lexeme;
    int         line;
    int         column;

    Token() : type(TokenType::UNKNOWN), line(0), column(0) {}
    Token(TokenType t, std::string lex, int l, int c)
        : type(t), lexeme(std::move(lex)), line(l), column(c) {}
};

inline const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::INTEGER_LIT:  return "INTEGER_LIT";
        case TokenType::FLOAT_LIT:   return "FLOAT_LIT";
        case TokenType::STRING_LIT:  return "STRING_LIT";
        case TokenType::CHAR_LIT:    return "CHAR_LIT";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::KW_INT:      return "int";
        case TokenType::KW_FLOAT:    return "float";
        case TokenType::KW_DOUBLE:   return "double";
        case TokenType::KW_CHAR:     return "char";
        case TokenType::KW_BOOL:     return "bool";
        case TokenType::KW_VOID:     return "void";
        case TokenType::KW_STRING:   return "string";
        case TokenType::KW_CONST:    return "const";
        case TokenType::KW_IF:       return "if";
        case TokenType::KW_ELSE:     return "else";
        case TokenType::KW_WHILE:    return "while";
        case TokenType::KW_FOR:      return "for";
        case TokenType::KW_RETURN:   return "return";
        case TokenType::KW_BREAK:    return "break";
        case TokenType::KW_CONTINUE: return "continue";
        case TokenType::KW_TRUE:     return "true";
        case TokenType::KW_FALSE:    return "false";
        case TokenType::KW_COUT:     return "cout";
        case TokenType::KW_CIN:      return "cin";
        case TokenType::KW_ENDL:     return "endl";
        case TokenType::KW_INCLUDE:  return "include";
        case TokenType::KW_USING:    return "using";
        case TokenType::KW_NAMESPACE:return "namespace";
        case TokenType::KW_STD:      return "std";
        case TokenType::PLUS:        return "+";
        case TokenType::MINUS:       return "-";
        case TokenType::STAR:        return "*";
        case TokenType::SLASH:       return "/";
        case TokenType::PERCENT:     return "%";
        case TokenType::AMPERSAND:   return "&";
        case TokenType::PIPE:        return "|";
        case TokenType::CARET:       return "^";
        case TokenType::TILDE:       return "~";
        case TokenType::LSHIFT:      return "<<";
        case TokenType::RSHIFT:      return ">>";
        case TokenType::PLUS_EQ:     return "+=";
        case TokenType::MINUS_EQ:    return "-=";
        case TokenType::STAR_EQ:     return "*=";
        case TokenType::SLASH_EQ:    return "/=";
        case TokenType::EQ:          return "=";
        case TokenType::EQ_EQ:       return "==";
        case TokenType::BANG:        return "!";
        case TokenType::BANG_EQ:     return "!=";
        case TokenType::LT:          return "<";
        case TokenType::GT:          return ">";
        case TokenType::LT_EQ:       return "<=";
        case TokenType::GT_EQ:       return ">=";
        case TokenType::AND_AND:     return "&&";
        case TokenType::OR_OR:       return "||";
        case TokenType::PLUS_PLUS:   return "++";
        case TokenType::MINUS_MINUS: return "--";
        case TokenType::LPAREN:      return "(";
        case TokenType::RPAREN:      return ")";
        case TokenType::LBRACE:      return "{";
        case TokenType::RBRACE:      return "}";
        case TokenType::LBRACKET:    return "[";
        case TokenType::RBRACKET:    return "]";
        case TokenType::SEMICOLON:   return ";";
        case TokenType::COMMA:       return ",";
        case TokenType::DOT:         return ".";
        case TokenType::COLON:       return ":";
        case TokenType::SCOPE:       return "::";
        case TokenType::HASH:        return "#";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::UNKNOWN:     return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> kw = {
        {"int",       TokenType::KW_INT},
        {"float",     TokenType::KW_FLOAT},
        {"double",    TokenType::KW_DOUBLE},
        {"char",      TokenType::KW_CHAR},
        {"bool",      TokenType::KW_BOOL},
        {"void",      TokenType::KW_VOID},
        {"string",    TokenType::KW_STRING},
        {"const",     TokenType::KW_CONST},
        {"if",        TokenType::KW_IF},
        {"else",      TokenType::KW_ELSE},
        {"while",     TokenType::KW_WHILE},
        {"for",       TokenType::KW_FOR},
        {"return",    TokenType::KW_RETURN},
        {"break",     TokenType::KW_BREAK},
        {"continue",  TokenType::KW_CONTINUE},
        {"true",      TokenType::KW_TRUE},
        {"false",     TokenType::KW_FALSE},
        {"cout",      TokenType::KW_COUT},
        {"cin",       TokenType::KW_CIN},
        {"endl",      TokenType::KW_ENDL},
        {"include",   TokenType::KW_INCLUDE},
        {"using",     TokenType::KW_USING},
        {"namespace", TokenType::KW_NAMESPACE},
        {"std",       TokenType::KW_STD},
    };
    return kw;
}

} 
