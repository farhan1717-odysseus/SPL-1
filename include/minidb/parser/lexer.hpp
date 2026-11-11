#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minidb::parser {

enum class TokenType {
    EndOfInput,
    Identifier,
    Integer,
    FloatingPoint,
    String,
    LeftParenthesis,
    RightParenthesis,
    Comma,
    Semicolon,
    Asterisk,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Plus,
    Minus,
    Create,
    Table,
    Drop,
    Show,
    Tables,
    Describe,
    Database,
    Databases,
    Use,
    Select,
    From,
    Where,
    Insert,
    Into,
    Values,
    Update,
    Set,
    Delete,
    And,
    Or,
    Is,
    Not,
    Null,
    True,
    False,
    Int,
    Float,
    Double,
    Varchar,
    Text,
    Boolean
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::string value;
    std::size_t offset;
    std::size_t line;
    std::size_t column;
};

struct LexicalError {
    std::string message;
    std::size_t offset;
    std::size_t line;
    std::size_t column;
};

struct TokenizeResult {
    std::vector<Token> tokens;
    std::optional<LexicalError> error;
};

std::string_view token_type_name(TokenType type) noexcept;
TokenizeResult tokenize(std::string_view input);

}  // namespace minidb::parser
