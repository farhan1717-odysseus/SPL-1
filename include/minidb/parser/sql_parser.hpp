#pragma once

#include "minidb/parser/lexer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace minidb::parser {

enum class SqlDataType {
    Int,
    Float,
    Double,
    Varchar,
    Text,
    Boolean
};

struct SqlColumnDefinition {
    std::string name;
    SqlDataType type;
};

struct CreateDatabaseCommand {
    std::string name;
};

struct DropDatabaseCommand {
    std::string name;
};

struct ShowDatabasesCommand {};

struct UseDatabaseCommand {
    std::string name;
};

struct CreateTableCommand {
    std::string name;
    std::vector<SqlColumnDefinition> columns;
};

struct DropTableCommand {
    std::string name;
};

struct ShowTablesCommand {};

struct DescribeTableCommand {
    std::string name;
};

enum class SqlValueType {
    Integer,
    FloatingPoint,
    String,
    Boolean,
    Null
};

struct SqlValue {
    SqlValueType type;
    std::string value;
};

struct InsertCommand {
    std::string table;
    std::vector<SqlValue> values;
};

struct SqlCondition;

struct SelectCommand {
    std::string table;
    bool select_all = false;
    std::vector<std::string> columns;
    std::shared_ptr<const SqlCondition> condition;
};

enum class SqlComparisonOperator {
    Equal,
    NotEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual
};

struct SqlComparisonCondition {
    std::string column;
    SqlComparisonOperator operation;
    SqlValue value;
};

struct SqlNullCondition {
    std::string column;
    bool is_not;
};

enum class SqlLogicalOperator {
    And,
    Or
};

struct SqlLogicalCondition {
    SqlLogicalOperator operation;
    std::shared_ptr<const SqlCondition> left;
    std::shared_ptr<const SqlCondition> right;
};

struct SqlCondition {
    std::variant<SqlComparisonCondition, SqlNullCondition,
                 SqlLogicalCondition>
        expression;
};

struct UpdateCommand {
    std::string table;
    std::string column;
    SqlValue value;
    SqlCondition condition;
};

struct DeleteCommand {
    std::string table;
    SqlCondition condition;
};

using SqlCommand =
    std::variant<CreateDatabaseCommand, DropDatabaseCommand,
                 ShowDatabasesCommand, UseDatabaseCommand, CreateTableCommand,
                 DropTableCommand, ShowTablesCommand, DescribeTableCommand,
                 InsertCommand, SelectCommand, UpdateCommand, DeleteCommand>;

struct SqlParseError {
    std::string message;
    std::size_t offset;
    std::size_t line;
    std::size_t column;
};

struct SqlParseResult {
    std::optional<SqlCommand> command;
    std::optional<SqlParseError> error;
};

SqlParseResult parse_sql(std::string_view input);

}  // namespace minidb::parser
