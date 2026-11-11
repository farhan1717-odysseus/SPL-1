#include "minidb/parser/sql_parser.hpp"

#include <string>
#include <utility>

namespace minidb::parser
{
    namespace
    {

        class Parser
        {
        public:
            explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

            SqlParseResult run()
            {
                if (peek().type == TokenType::EndOfInput)
                {
                    return failure("Expected a SQL command.");
                }

                auto command = parse_command();
                if (!command)
                {
                    return {std::nullopt, std::move(error_)};
                }

                if (match(TokenType::Semicolon) &&
                    peek().type != TokenType::EndOfInput)
                {
                    return failure("Only one SQL command is allowed per input.");
                }
                if (peek().type != TokenType::EndOfInput)
                {
                    return failure("Unexpected token '" + peek().lexeme +
                                   "' after the SQL command.");
                }
                return {std::move(command), std::nullopt};
            }

        private:
            const Token &peek() const noexcept
            {
                return tokens_[position_];
            }

            bool match(TokenType type) noexcept
            {
                if (peek().type != type)
                {
                    return false;
                }
                ++position_;
                return true;
            }

            bool expect(TokenType type, std::string message)
            {
                if (match(type))
                {
                    return true;
                }
                fail(std::move(message), peek());
                return false;
            }

            void fail(std::string message, const Token &token)
            {
                if (!error_)
                {
                    error_ = SqlParseError{std::move(message), token.offset, token.line,
                                           token.column};
                }
            }

            SqlParseResult failure(std::string message)
            {
                fail(std::move(message), peek());
                return {std::nullopt, std::move(error_)};
            }

            std::optional<std::string> parse_identifier(std::string_view purpose)
            {
                if (peek().type != TokenType::Identifier)
                {
                    fail("Expected " + std::string(purpose) + " identifier.", peek());
                    return std::nullopt;
                }
                return tokens_[position_++].lexeme;
            }

            std::optional<SqlCommand> parse_command()
            {
                if (match(TokenType::Create))
                {
                    return parse_create();
                }
                if (match(TokenType::Drop))
                {
                    return parse_drop();
                }
                if (match(TokenType::Show))
                {
                    return parse_show();
                }
                if (match(TokenType::Use))
                {
                    auto name = parse_identifier("database");
                    if (name)
                    {
                        return UseDatabaseCommand{std::move(*name)};
                    }
                    return std::nullopt;
                }
                if (match(TokenType::Describe))
                {
                    if (!expect(TokenType::Table,
                                "Expected TABLE after DESCRIBE."))
                    {
                        return std::nullopt;
                    }
                    auto name = parse_identifier("table");
                    if (name)
                    {
                        return DescribeTableCommand{std::move(*name)};
                    }
                    return std::nullopt;
                }
                if (match(TokenType::Insert))
                {
                    return parse_insert();
                }
                if (match(TokenType::Select))
                {
                    return parse_select();
                }
                if (match(TokenType::Update))
                {
                    return parse_update();
                }
                if (match(TokenType::Delete))
                {
                    return parse_delete();
                }

                fail("Expected a supported database, table, or data command.", peek());
                return std::nullopt;
            }

            std::optional<SqlCommand> parse_create()
            {
                if (match(TokenType::Database))
                {
                    auto name = parse_identifier("database");
                    if (name)
                    {
                        return CreateDatabaseCommand{std::move(*name)};
                    }
                    return std::nullopt;
                }
                if (match(TokenType::Table))
                {
                    return parse_create_table();
                }
                fail("Expected DATABASE or TABLE after CREATE.", peek());
                return std::nullopt;
            }

            std::optional<SqlCommand> parse_create_table()
            {
                auto name = parse_identifier("table");
                if (!name)
                {
                    return std::nullopt;
                }
                if (!expect(TokenType::LeftParenthesis,
                            "Expected '(' after the table name."))
                {
                    return std::nullopt;
                }

                std::vector<SqlColumnDefinition> columns;
                if (peek().type == TokenType::RightParenthesis)
                {
                    fail("A table must declare at least one column.", peek());
                    return std::nullopt;
                }

                while (true)
                {
                    auto column_name = parse_identifier("column");
                    if (!column_name)
                    {
                        return std::nullopt;
                    }
                    auto column_type = parse_column_type();
                    if (!column_type)
                    {
                        return std::nullopt;
                    }
                    columns.push_back({std::move(*column_name), *column_type});

                    if (!match(TokenType::Comma))
                    {
                        break;
                    }
                }

                if (!expect(TokenType::RightParenthesis,
                            "Expected ')' after the table columns."))
                {
                    return std::nullopt;
                }
                return CreateTableCommand{std::move(*name), std::move(columns)};
            }

            std::optional<SqlDataType> parse_column_type()
            {
                switch (peek().type)
                {
                case TokenType::Int:
                    ++position_;
                    return SqlDataType::Int;
                case TokenType::Float:
                    ++position_;
                    return SqlDataType::Float;
                case TokenType::Double:
                    ++position_;
                    return SqlDataType::Double;
                case TokenType::Varchar:
                    ++position_;
                    return SqlDataType::Varchar;
                case TokenType::Text:
                    ++position_;
                    return SqlDataType::Text;
                case TokenType::Boolean:
                    ++position_;
                    return SqlDataType::Boolean;
                default:
                    fail("Expected a column type: INT, FLOAT, DOUBLE, VARCHAR, "
                         "TEXT, or BOOLEAN.",
                         peek());
                    return std::nullopt;
                }
            }

            std::optional<SqlCommand> parse_drop()
            {
                if (match(TokenType::Database))
                {
                    auto name = parse_identifier("database");
                    if (name)
                    {
                        return DropDatabaseCommand{std::move(*name)};
                    }
                    return std::nullopt;
                }
                if (match(TokenType::Table))
                {
                    auto name = parse_identifier("table");
                    if (name)
                    {
                        return DropTableCommand{std::move(*name)};
                    }
                    return std::nullopt;
                }
                fail("Expected DATABASE or TABLE after DROP.", peek());
                return std::nullopt;
            }

            std::optional<SqlCommand> parse_show()
            {
                if (match(TokenType::Databases))
                {
                    return ShowDatabasesCommand{};
                }
                if (match(TokenType::Tables))
                {
                    return ShowTablesCommand{};
                }
                fail("Expected DATABASES or TABLES after SHOW.", peek());
                return std::nullopt;
            }

            std::optional<SqlCommand> parse_insert()
            {
                if (!expect(TokenType::Into, "Expected INTO after INSERT."))
                {
                    return std::nullopt;
                }
                auto table = parse_identifier("table");
                if (!table ||
                    !expect(TokenType::Values, "Expected VALUES after the table name.") ||
                    !expect(TokenType::LeftParenthesis,
                            "Expected '(' after VALUES."))
                {
                    return std::nullopt;
                }

                std::vector<SqlValue> values;
                auto value = parse_value();
                if (!value)
                {
                    return std::nullopt;
                }
                values.push_back(std::move(*value));
                while (match(TokenType::Comma))
                {
                    value = parse_value();
                    if (!value)
                    {
                        return std::nullopt;
                    }
                    values.push_back(std::move(*value));
                }
                if (!expect(TokenType::RightParenthesis,
                            "Expected ')' after the INSERT values."))
                {
                    return std::nullopt;
                }
                return InsertCommand{std::move(*table), std::move(values)};
            }

            std::optional<SqlCommand> parse_select()
            {
                SelectCommand command;
                if (match(TokenType::Asterisk))
                {
                    command.select_all = true;
                }
                else
                {
                    auto column = parse_identifier("selected column");
                    if (!column)
                    {
                        return std::nullopt;
                    }
                    command.columns.push_back(std::move(*column));
                    while (match(TokenType::Comma))
                    {
                        column = parse_identifier("selected column");
                        if (!column)
                        {
                            return std::nullopt;
                        }
                        command.columns.push_back(std::move(*column));
                    }
                }

                if (!expect(TokenType::From, "Expected FROM after the selected columns."))
                {
                    return std::nullopt;
                }
                auto table = parse_identifier("table");
                if (!table)
                {
                    return std::nullopt;
                }
                command.table = std::move(*table);
                if (match(TokenType::Where))
                {
                    auto condition = parse_or_condition();
                    if (!condition)
                    {
                        return std::nullopt;
                    }
                    command.condition =
                        std::make_shared<const SqlCondition>(std::move(*condition));
                }
                return command;
            }

            std::optional<SqlCommand> parse_update()
            {
                auto table = parse_identifier("table");
                if (!table ||
                    !expect(TokenType::Set, "Expected SET after the table name."))
                {
                    return std::nullopt;
                }
                auto column = parse_identifier("column");
                if (!column || !expect(TokenType::Equal,
                                       "Expected '=' after the column name."))
                {
                    return std::nullopt;
                }
                auto value = parse_value();
                if (!value ||
                    !expect(TokenType::Where, "Expected WHERE after the assignment."))
                {
                    return std::nullopt;
                }
                auto condition = parse_or_condition();
                if (!condition)
                {
                    return std::nullopt;
                }
                return UpdateCommand{std::move(*table), std::move(*column),
                                     std::move(*value), std::move(*condition)};
            }

            std::optional<SqlCommand> parse_delete()
            {
                if (!expect(TokenType::From, "Expected FROM after DELETE."))
                {
                    return std::nullopt;
                }
                auto table = parse_identifier("table");
                if (!table ||
                    !expect(TokenType::Where, "Expected WHERE after the table name."))
                {
                    return std::nullopt;
                }
                auto condition = parse_or_condition();
                if (!condition)
                {
                    return std::nullopt;
                }
                return DeleteCommand{std::move(*table), std::move(*condition)};
            }

            std::optional<SqlValue> parse_value()
            {
                std::string sign;
                bool has_sign = false;
                if (match(TokenType::Minus))
                {
                    sign = "-";
                    has_sign = true;
                }
                else if (match(TokenType::Plus))
                {
                    has_sign = true;
                }

                const Token &token = peek();
                switch (token.type)
                {
                case TokenType::Integer:
                    ++position_;
                    return SqlValue{SqlValueType::Integer, sign + token.lexeme};
                case TokenType::FloatingPoint:
                    ++position_;
                    return SqlValue{SqlValueType::FloatingPoint,
                                    sign + token.lexeme};
                case TokenType::String:
                    if (has_sign)
                    {
                        fail("A sign can only be used with a numeric value.", token);
                        return std::nullopt;
                    }
                    ++position_;
                    return SqlValue{SqlValueType::String, token.value};
                case TokenType::True:
                case TokenType::False:
                    if (has_sign)
                    {
                        fail("A sign can only be used with a numeric value.", token);
                        return std::nullopt;
                    }
                    ++position_;
                    return SqlValue{SqlValueType::Boolean, token.lexeme};
                case TokenType::Null:
                    if (has_sign)
                    {
                        fail("A sign can only be used with a numeric value.", token);
                        return std::nullopt;
                    }
                    ++position_;
                    return SqlValue{SqlValueType::Null, {}};
                default:
                    fail(!has_sign
                             ? "Expected a number, string, TRUE, FALSE, or NULL."
                             : "Expected a numeric value after the sign.",
                         token);
                    return std::nullopt;
                }
            }

            std::optional<SqlCondition> parse_or_condition()
            {
                auto left = parse_and_condition();
                if (!left)
                {
                    return std::nullopt;
                }
                while (match(TokenType::Or))
                {
                    auto right = parse_and_condition();
                    if (!right)
                    {
                        return std::nullopt;
                    }
                    left = SqlCondition{SqlLogicalCondition{
                        SqlLogicalOperator::Or,
                        std::make_shared<const SqlCondition>(std::move(*left)),
                        std::make_shared<const SqlCondition>(std::move(*right))}};
                }
                return left;
            }

            std::optional<SqlCondition> parse_and_condition()
            {
                auto left = parse_condition_atom();
                if (!left)
                {
                    return std::nullopt;
                }
                while (match(TokenType::And))
                {
                    auto right = parse_condition_atom();
                    if (!right)
                    {
                        return std::nullopt;
                    }
                    left = SqlCondition{SqlLogicalCondition{
                        SqlLogicalOperator::And,
                        std::make_shared<const SqlCondition>(std::move(*left)),
                        std::make_shared<const SqlCondition>(std::move(*right))}};
                }
                return left;
            }

            std::optional<SqlCondition> parse_condition_atom()
            {
                if (match(TokenType::LeftParenthesis))
                {
                    auto condition = parse_or_condition();
                    if (!condition ||
                        !expect(TokenType::RightParenthesis,
                                "Expected ')' after the WHERE condition."))
                    {
                        return std::nullopt;
                    }
                    return condition;
                }

                auto column = parse_identifier("condition column");
                if (!column)
                {
                    return std::nullopt;
                }

                if (match(TokenType::Is))
                {
                    const bool is_not = match(TokenType::Not);
                    if (!expect(TokenType::Null, "Expected NULL after IS."))
                    {
                        return std::nullopt;
                    }
                    return SqlCondition{
                        SqlNullCondition{std::move(*column), is_not}};
                }

                SqlComparisonOperator operation;
                switch (peek().type)
                {
                case TokenType::Equal:
                    operation = SqlComparisonOperator::Equal;
                    break;
                case TokenType::NotEqual:
                    operation = SqlComparisonOperator::NotEqual;
                    break;
                case TokenType::Less:
                    operation = SqlComparisonOperator::Less;
                    break;
                case TokenType::Greater:
                    operation = SqlComparisonOperator::Greater;
                    break;
                case TokenType::LessEqual:
                    operation = SqlComparisonOperator::LessEqual;
                    break;
                case TokenType::GreaterEqual:
                    operation = SqlComparisonOperator::GreaterEqual;
                    break;
                default:
                    fail("Expected a comparison operator (=, !=, <, >, <=, or >=).",
                         peek());
                    return std::nullopt;
                }
                ++position_;

                auto value = parse_value();
                if (!value)
                {
                    return std::nullopt;
                }
                return SqlCondition{SqlComparisonCondition{
                    std::move(*column), operation, std::move(*value)}};
            }

            std::vector<Token> tokens_;
            std::size_t position_ = 0;
            std::optional<SqlParseError> error_;
        };

    } // namespace

    SqlParseResult parse_sql(std::string_view input)
    {
        TokenizeResult tokenized = tokenize(input);
        if (tokenized.error)
        {
            return {std::nullopt,
                    SqlParseError{std::move(tokenized.error->message),
                                  tokenized.error->offset, tokenized.error->line,
                                  tokenized.error->column}};
        }
        return Parser(std::move(tokenized.tokens)).run();
    }

} // namespace minidb::parser
