#include "minidb/parser/lexer.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

using minidb::parser::Token;
using minidb::parser::TokenType;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool expect_types(std::string_view input,
                  const std::vector<TokenType>& expected) {
    const auto result = minidb::parser::tokenize(input);
    if (!expect(!result.error, "Valid input should tokenize without errors.") ||
        !expect(result.tokens.size() == expected.size(),
                "Token count should match expected count.")) {
        return false;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (!expect(result.tokens[index].type == expected[index],
                    "Token type did not match expected type.")) {
            return false;
        }
    }
    return true;
}

bool test_keywords_identifiers_and_whitespace() {
    return expect_types(
               "  cReAtE\tTABLE\nstudent (id INT, name VARCHAR); ",
               {TokenType::Create, TokenType::Table, TokenType::Identifier,
                TokenType::LeftParenthesis, TokenType::Identifier,
                TokenType::Int, TokenType::Comma, TokenType::Identifier,
                TokenType::Varchar, TokenType::RightParenthesis,
                TokenType::Semicolon, TokenType::EndOfInput}) &&
           expect_types("select select_name FROM _table",
                        {TokenType::Select, TokenType::Identifier,
                         TokenType::From, TokenType::Identifier,
                         TokenType::EndOfInput});
}

bool test_numbers_and_operators() {
    const auto numbers = minidb::parser::tokenize(
        "0 123 12.5 .75 4. 1e3 2.5E-2");
    const std::vector<TokenType> expected_numbers{
        TokenType::Integer, TokenType::Integer, TokenType::FloatingPoint,
        TokenType::FloatingPoint, TokenType::FloatingPoint,
        TokenType::FloatingPoint, TokenType::FloatingPoint,
        TokenType::EndOfInput};
    if (!expect(!numbers.error && numbers.tokens.size() == expected_numbers.size(),
                "Integer, decimal, and exponent forms should tokenize.") ||
        !expect_types("= != <> < <= > >= * + -",
                      {TokenType::Equal, TokenType::NotEqual,
                       TokenType::NotEqual, TokenType::Less,
                       TokenType::LessEqual, TokenType::Greater,
                       TokenType::GreaterEqual, TokenType::Asterisk,
                       TokenType::Plus, TokenType::Minus,
                       TokenType::EndOfInput})) {
        return false;
    }

    for (std::size_t index = 0; index < expected_numbers.size(); ++index) {
        if (!expect(numbers.tokens[index].type == expected_numbers[index],
                    "Numeric token type did not match expected type.")) {
            return false;
        }
    }
    return expect(numbers.tokens[2].lexeme == "12.5" &&
                      numbers.tokens[3].lexeme == ".75" &&
                      numbers.tokens[5].lexeme == "1e3" &&
                      numbers.tokens[6].lexeme == "2.5E-2",
                  "Numeric tokens should preserve their source spelling.");
}

bool test_string_literals() {
    const auto doubled = minidb::parser::tokenize("'it''s ready'");
    const auto trailing_backslash = minidb::parser::tokenize("'C:\\'");
    return expect(!doubled.error && doubled.tokens.size() == 2 &&
                      doubled.tokens[0].type == TokenType::String &&
                      doubled.tokens[0].value == "it's ready" &&
                      doubled.tokens[0].lexeme == "'it''s ready'",
                  "Doubled quotes should be decoded and preserve the lexeme.") &&
           expect(!trailing_backslash.error &&
                      trailing_backslash.tokens.size() == 2 &&
                      trailing_backslash.tokens[0].value == "C:\\",
                  "A backslash before a closing quote should remain literal.");
}

bool test_lexical_errors_and_positions() {
    const auto unexpected = minidb::parser::tokenize("SELECT\n@");
    if (!expect(unexpected.error.has_value(),
                "Unexpected characters should produce a lexical error.") ||
        !expect(unexpected.error->message.find("Unexpected character") !=
                    std::string::npos &&
                    unexpected.error->line == 2 &&
                    unexpected.error->column == 1 &&
                    unexpected.error->offset == 7,
                "Unexpected-character errors should include their location.")) {
        return false;
    }

    const auto unterminated = minidb::parser::tokenize("SELECT 'unfinished");
    const auto malformed_exponent = minidb::parser::tokenize("1.2e+");
    const auto malformed_decimal = minidb::parser::tokenize("3.4.5");
    const auto lone_bang = minidb::parser::tokenize("!");
    return expect(unterminated.error &&
                      unterminated.error->message.find(
                          "Unterminated string literal") != std::string::npos,
                  "Unterminated strings should produce a meaningful error.") &&
           expect(malformed_exponent.error &&
                      malformed_exponent.error->message.find(
                          "exponent requires a digit") != std::string::npos,
                  "Malformed exponents should produce a meaningful error.") &&
           expect(malformed_decimal.error &&
                      malformed_decimal.error->message.find(
                          "multiple decimal points") != std::string::npos,
                  "Malformed decimals should produce a meaningful error.") &&
           expect(lone_bang.error &&
                      lone_bang.error->message.find("Expected '='") !=
                          std::string::npos,
                  "A lone exclamation mark should produce a clear error.");
}

bool test_token_locations() {
    const auto result = minidb::parser::tokenize("SELECT\n  id;");
    return expect(!result.error && result.tokens.size() == 4 &&
                      result.tokens[0].line == 1 &&
                      result.tokens[0].column == 1 &&
                      result.tokens[1].line == 2 &&
                      result.tokens[1].column == 3 &&
                      result.tokens[1].offset == 9,
                  "Tokens should expose their source locations.");
}

}  // namespace

int main() {
    if (!test_keywords_identifiers_and_whitespace() ||
        !test_numbers_and_operators() || !test_string_literals() ||
        !test_lexical_errors_and_positions() || !test_token_locations()) {
        return 1;
    }
    return 0;
}
