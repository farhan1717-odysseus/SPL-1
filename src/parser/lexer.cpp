#include "minidb/parser/lexer.hpp"

#include <array>
#include <cctype>
#include <string>
#include <utility>

namespace minidb::parser
{
    namespace
    {

        using Keyword = std::pair<std::string_view, TokenType>;

        constexpr std::array<Keyword, 31> keywords{{
            {"AND", TokenType::And},
            {"BOOLEAN", TokenType::Boolean},
            {"CREATE", TokenType::Create},
            {"DATABASE", TokenType::Database},
            {"DATABASES", TokenType::Databases},
            {"DELETE", TokenType::Delete},
            {"DESCRIBE", TokenType::Describe},
            {"DOUBLE", TokenType::Double},
            {"DROP", TokenType::Drop},
            {"FALSE", TokenType::False},
            {"FLOAT", TokenType::Float},
            {"FROM", TokenType::From},
            {"INSERT", TokenType::Insert},
            {"IS", TokenType::Is},
            {"INT", TokenType::Int},
            {"INTO", TokenType::Into},
            {"NOT", TokenType::Not},
            {"NULL", TokenType::Null},
            {"OR", TokenType::Or},
            {"SELECT", TokenType::Select},
            {"SET", TokenType::Set},
            {"SHOW", TokenType::Show},
            {"TABLE", TokenType::Table},
            {"TABLES", TokenType::Tables},
            {"TEXT", TokenType::Text},
            {"TRUE", TokenType::True},
            {"UPDATE", TokenType::Update},
            {"USE", TokenType::Use},
            {"VALUES", TokenType::Values},
            {"VARCHAR", TokenType::Varchar},
            {"WHERE", TokenType::Where},
        }};

        bool is_identifier_start(char character) noexcept
        {
            return (character >= 'A' && character <= 'Z') ||
                   (character >= 'a' && character <= 'z') || character == '_';
        }

        bool is_identifier_continue(char character) noexcept
        {
            return is_identifier_start(character) ||
                   (character >= '0' && character <= '9');
        }

        bool is_digit(char character) noexcept
        {
            return character >= '0' && character <= '9';
        }

        char ascii_upper(char character) noexcept
        {
            if (character >= 'a' && character <= 'z')
            {
                return static_cast<char>(character - ('a' - 'A'));
            }
            return character;
        }

        std::optional<TokenType> keyword_type(std::string_view identifier) noexcept
        {
            for (const auto &keyword : keywords)
            {
                if (keyword.first.empty() || keyword.first.size() != identifier.size())
                {
                    continue;
                }

                bool matches = true;
                for (std::size_t index = 0; index < identifier.size(); ++index)
                {
                    if (ascii_upper(identifier[index]) != keyword.first[index])
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                {
                    return keyword.second;
                }
            }
            return std::nullopt;
        }

        class Lexer
        {
        public:
            explicit Lexer(std::string_view input) : input_(input) {}

            TokenizeResult run()
            {
                while (!at_end() && !error_)
                {
                    if (is_whitespace(peek()))
                    {
                        advance();
                    }
                    else
                    {
                        scan_token();
                    }
                }

                if (error_)
                {
                    return {{}, std::move(error_)};
                }

                tokens_.push_back(
                    {TokenType::EndOfInput, {}, {}, offset_, line_, column_});
                return {std::move(tokens_), std::nullopt};
            }

        private:
            bool at_end() const noexcept
            {
                return offset_ >= input_.size();
            }

            char peek() const noexcept
            {
                return at_end() ? '\0' : input_[offset_];
            }

            char peek_next() const noexcept
            {
                return offset_ + 1 >= input_.size() ? '\0' : input_[offset_ + 1];
            }

            char advance() noexcept
            {
                const char character = input_[offset_++];
                if (character == '\n')
                {
                    ++line_;
                    column_ = 1;
                }
                else
                {
                    ++column_;
                }
                return character;
            }

            static bool is_whitespace(char character) noexcept
            {
                return std::isspace(static_cast<unsigned char>(character)) != 0;
            }

            void add_token(TokenType type, std::size_t start, std::size_t start_line,
                           std::size_t start_column, std::string value = {})
            {
                tokens_.push_back(
                    {type, std::string(input_.substr(start, offset_ - start)),
                     std::move(value), start, start_line, start_column});
            }

            void fail(std::string message, std::size_t offset, std::size_t line,
                      std::size_t column)
            {
                error_ = LexicalError{std::move(message), offset, line, column};
            }

            void scan_token()
            {
                const std::size_t start = offset_;
                const std::size_t start_line = line_;
                const std::size_t start_column = column_;
                const char character = advance();

                switch (character)
                {
                case '(':
                    add_token(TokenType::LeftParenthesis, start, start_line,
                              start_column);
                    return;
                case ')':
                    add_token(TokenType::RightParenthesis, start, start_line,
                              start_column);
                    return;
                case ',':
                    add_token(TokenType::Comma, start, start_line, start_column);
                    return;
                case ';':
                    add_token(TokenType::Semicolon, start, start_line,
                              start_column);
                    return;
                case '*':
                    add_token(TokenType::Asterisk, start, start_line, start_column);
                    return;
                case '=':
                    add_token(TokenType::Equal, start, start_line, start_column);
                    return;
                case '<':
                    if (peek() == '=')
                    {
                        advance();
                        add_token(TokenType::LessEqual, start, start_line,
                                  start_column);
                    }
                    else if (peek() == '>')
                    {
                        advance();
                        add_token(TokenType::NotEqual, start, start_line,
                                  start_column);
                    }
                    else
                    {
                        add_token(TokenType::Less, start, start_line, start_column);
                    }
                    return;
                case '>':
                    if (peek() == '=')
                    {
                        advance();
                        add_token(TokenType::GreaterEqual, start, start_line,
                                  start_column);
                    }
                    else
                    {
                        add_token(TokenType::Greater, start, start_line,
                                  start_column);
                    }
                    return;
                case '+':
                    add_token(TokenType::Plus, start, start_line, start_column);
                    return;
                case '-':
                    add_token(TokenType::Minus, start, start_line, start_column);
                    return;
                case '!':
                    if (peek() == '=')
                    {
                        advance();
                        add_token(TokenType::NotEqual, start, start_line,
                                  start_column);
                    }
                    else
                    {
                        fail("Expected '=' after '!'.", start, start_line,
                             start_column);
                    }
                    return;
                case '\'':
                    scan_string(start, start_line, start_column);
                    return;
                default:
                    break;
                }

                if (is_identifier_start(character))
                {
                    scan_identifier(start, start_line, start_column);
                }
                else if (is_digit(character) ||
                         (character == '.' && is_digit(peek())))
                {
                    scan_number(start, start_line, start_column);
                }
                else
                {
                    fail("Unexpected character '" + std::string(1, character) + "'.",
                         start, start_line, start_column);
                }
            }

            void scan_identifier(std::size_t start, std::size_t start_line,
                                 std::size_t start_column)
            {
                while (is_identifier_continue(peek()))
                {
                    advance();
                }

                const auto text = input_.substr(start, offset_ - start);
                const auto type = keyword_type(text);
                add_token(type.value_or(TokenType::Identifier), start, start_line,
                          start_column);
            }

            void scan_number(std::size_t start, std::size_t start_line,
                             std::size_t start_column)
            {
                bool floating = false;
                if (input_[start] == '.')
                {
                    floating = true;
                    while (is_digit(peek()))
                    {
                        advance();
                    }
                }
                else
                {
                    while (is_digit(peek()))
                    {
                        advance();
                    }
                    if (peek() == '.')
                    {
                        floating = true;
                        advance();
                        while (is_digit(peek()))
                        {
                            advance();
                        }
                    }
                }

                if (peek() == 'e' || peek() == 'E')
                {
                    floating = true;
                    const std::size_t exponent_offset = offset_;
                    const std::size_t exponent_line = line_;
                    const std::size_t exponent_column = column_;
                    advance();
                    if (peek() == '+' || peek() == '-')
                    {
                        advance();
                    }
                    if (!is_digit(peek()))
                    {
                        fail("Malformed numeric literal: exponent requires a digit.",
                             exponent_offset, exponent_line, exponent_column);
                        return;
                    }
                    while (is_digit(peek()))
                    {
                        advance();
                    }
                }

                if (peek() == '.' && floating)
                {
                    fail("Malformed numeric literal: multiple decimal points.", offset_,
                         line_, column_);
                    return;
                }

                add_token(floating ? TokenType::FloatingPoint : TokenType::Integer,
                          start, start_line, start_column);
            }

            void scan_string(std::size_t start, std::size_t start_line,
                             std::size_t start_column)
            {
                std::string value;
                while (!at_end())
                {
                    const char character = advance();
                    if (character == '\'')
                    {
                        if (peek() == '\'')
                        {
                            advance();
                            value.push_back('\'');
                            continue;
                        }
                        add_token(TokenType::String, start, start_line, start_column,
                                  std::move(value));
                        return;
                    }
                    value.push_back(character);
                }

                fail("Unterminated string literal.", start, start_line, start_column);
            }

            std::string_view input_;
            std::vector<Token> tokens_;
            std::optional<LexicalError> error_;
            std::size_t offset_ = 0;
            std::size_t line_ = 1;
            std::size_t column_ = 1;
        };

    } // namespace

    std::string_view token_type_name(TokenType type) noexcept
    {
        switch (type)
        {
        case TokenType::EndOfInput:
            return "end of input";
        case TokenType::Identifier:
            return "identifier";
        case TokenType::Integer:
            return "integer";
        case TokenType::FloatingPoint:
            return "floating-point number";
        case TokenType::String:
            return "string";
        case TokenType::LeftParenthesis:
            return "(";
        case TokenType::RightParenthesis:
            return ")";
        case TokenType::Comma:
            return ",";
        case TokenType::Semicolon:
            return ";";
        case TokenType::Asterisk:
            return "*";
        case TokenType::Equal:
            return "=";
        case TokenType::NotEqual:
            return "!=";
        case TokenType::Less:
            return "<";
        case TokenType::LessEqual:
            return "<=";
        case TokenType::Greater:
            return ">";
        case TokenType::GreaterEqual:
            return ">=";
        case TokenType::Plus:
            return "+";
        case TokenType::Minus:
            return "-";
        case TokenType::Create:
            return "CREATE";
        case TokenType::Table:
            return "TABLE";
        case TokenType::Drop:
            return "DROP";
        case TokenType::Show:
            return "SHOW";
        case TokenType::Tables:
            return "TABLES";
        case TokenType::Describe:
            return "DESCRIBE";
        case TokenType::Database:
            return "DATABASE";
        case TokenType::Databases:
            return "DATABASES";
        case TokenType::Use:
            return "USE";
        case TokenType::Select:
            return "SELECT";
        case TokenType::From:
            return "FROM";
        case TokenType::Where:
            return "WHERE";
        case TokenType::Insert:
            return "INSERT";
        case TokenType::Into:
            return "INTO";
        case TokenType::Values:
            return "VALUES";
        case TokenType::Update:
            return "UPDATE";
        case TokenType::Set:
            return "SET";
        case TokenType::Delete:
            return "DELETE";
        case TokenType::And:
            return "AND";
        case TokenType::Or:
            return "OR";
        case TokenType::Is:
            return "IS";
        case TokenType::Not:
            return "NOT";
        case TokenType::Null:
            return "NULL";
        case TokenType::True:
            return "TRUE";
        case TokenType::False:
            return "FALSE";
        case TokenType::Int:
            return "INT";
        case TokenType::Float:
            return "FLOAT";
        case TokenType::Double:
            return "DOUBLE";
        case TokenType::Varchar:
            return "VARCHAR";
        case TokenType::Text:
            return "TEXT";
        case TokenType::Boolean:
            return "BOOLEAN";
        }
        return "unknown";
    }

    TokenizeResult tokenize(std::string_view input)
    {
        return Lexer(input).run();
    }

} // namespace minidb::parser
