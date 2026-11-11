#include "minidb/parser/sql_parser.hpp"

#include <iostream>
#include <string_view>
#include <variant>

namespace {

using namespace minidb::parser;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

template <typename Command>
bool expect_command(std::string_view input) {
    const auto result = parse_sql(input);
    return expect(result.command.has_value() && !result.error,
                  "Valid SQL should produce a command without an error.") &&
           expect(std::holds_alternative<Command>(*result.command),
                  "SQL should produce the expected command type.");
}

bool test_database_commands() {
    const auto create = parse_sql(" CREATE DATABASE school ; ");
    const auto drop = parse_sql("DROP DATABASE school");
    const auto use = parse_sql("UsE school;");
    return expect_command<CreateDatabaseCommand>("CREATE DATABASE school;") &&
           expect(std::get<CreateDatabaseCommand>(*create.command).name ==
                      "school",
                  "CREATE DATABASE should preserve its identifier.") &&
           expect_command<DropDatabaseCommand>("DROP DATABASE school") &&
           expect(std::get<DropDatabaseCommand>(*drop.command).name == "school",
                  "DROP DATABASE should preserve its identifier.") &&
           expect_command<ShowDatabasesCommand>("SHOW DATABASES;") &&
           expect_command<UseDatabaseCommand>("UsE school;") &&
           expect(std::get<UseDatabaseCommand>(*use.command).name == "school",
                  "USE should preserve its identifier.");
}

bool test_table_commands_and_column_types() {
    const auto result = parse_sql(
        "CREATE TABLE student (id INT, score FLOAT, average DOUBLE, "
        "name VARCHAR, notes TEXT, active BOOLEAN);");
    if (!expect(result.command && !result.error &&
                    std::holds_alternative<CreateTableCommand>(*result.command),
                "CREATE TABLE should produce a create-table command.")) {
        return false;
    }

    const auto& create = std::get<CreateTableCommand>(*result.command);
    return expect(create.name == "student" && create.columns.size() == 6,
                  "CREATE TABLE should retain its name and all columns.") &&
           expect(create.columns[0].name == "id" &&
                      create.columns[0].type == SqlDataType::Int &&
                      create.columns[1].type == SqlDataType::Float &&
                      create.columns[2].type == SqlDataType::Double &&
                      create.columns[3].type == SqlDataType::Varchar &&
                      create.columns[4].type == SqlDataType::Text &&
                      create.columns[5].type == SqlDataType::Boolean,
                  "CREATE TABLE should map every supported column type.") &&
           expect_command<DropTableCommand>("DROP TABLE student;") &&
           expect_command<ShowTablesCommand>("SHOW TABLES") &&
           expect_command<DescribeTableCommand>("DESCRIBE TABLE student;");
}

bool test_insert_select_commands() {
    const auto insert =
        parse_sql("INSERT INTO student VALUES (7, -2.5, 'Jo''s', TRUE, NULL);");
    const auto select_all = parse_sql("SELECT * FROM student;");
    const auto select_columns =
        parse_sql("SELECT id, name, average FROM student");
    const auto select_filtered =
        parse_sql("SELECT id FROM student WHERE score >= 90");

    if (!expect(insert.command && !insert.error &&
                    std::holds_alternative<InsertCommand>(*insert.command),
                "INSERT should produce an insert command.") ||
        !expect(select_all.command && !select_all.error &&
                    std::holds_alternative<SelectCommand>(*select_all.command),
                "SELECT * should produce a select command.") ||
        !expect(select_columns.command && !select_columns.error &&
                    std::holds_alternative<SelectCommand>(
                        *select_columns.command) &&
                    select_filtered.command && !select_filtered.error,
                "SELECT columns and WHERE should produce a select command.")) {
        return false;
    }

    const auto& insert_command = std::get<InsertCommand>(*insert.command);
    const auto& all_command = std::get<SelectCommand>(*select_all.command);
    const auto& columns_command =
        std::get<SelectCommand>(*select_columns.command);
    const auto& filtered_command =
        std::get<SelectCommand>(*select_filtered.command);
    return expect(insert_command.table == "student" &&
                      insert_command.values.size() == 5,
                  "INSERT should preserve its table and all values.") &&
           expect(insert_command.values[0].type == SqlValueType::Integer &&
                      insert_command.values[0].value == "7" &&
                      insert_command.values[1].type ==
                          SqlValueType::FloatingPoint &&
                      insert_command.values[1].value == "-2.5" &&
                      insert_command.values[2].type == SqlValueType::String &&
                      insert_command.values[2].value == "Jo's" &&
                      insert_command.values[3].type == SqlValueType::Boolean &&
                      insert_command.values[4].type == SqlValueType::Null,
                  "INSERT should preserve literal types and values.") &&
           expect(all_command.table == "student" && all_command.select_all &&
                      all_command.columns.empty(),
                  "SELECT * should be represented explicitly.") &&
           expect(columns_command.table == "student" &&
                      !columns_command.select_all &&
                      columns_command.columns.size() == 3 &&
                      columns_command.columns[0] == "id" &&
                      columns_command.columns[1] == "name" &&
                      columns_command.columns[2] == "average" &&
                      filtered_command.condition,
                  "SELECT should preserve columns and an optional condition.");
}

bool test_update_delete_and_conditions() {
    const auto update = parse_sql(
        "UPDATE student SET active = FALSE "
        "WHERE id >= 10 AND (score < 2.5 OR name != 'unknown');");
    const auto remove =
        parse_sql("DELETE FROM student WHERE id = -1 OR active = TRUE;");
    if (!expect(update.command && !update.error &&
                    std::holds_alternative<UpdateCommand>(*update.command),
                "UPDATE should produce an update command.") ||
        !expect(remove.command && !remove.error &&
                    std::holds_alternative<DeleteCommand>(*remove.command),
                "DELETE should produce a delete command.")) {
        return false;
    }

    const auto& update_command = std::get<UpdateCommand>(*update.command);
    const auto* outer_and =
        std::get_if<SqlLogicalCondition>(&update_command.condition.expression);
    if (!expect(update_command.table == "student" &&
                    update_command.column == "active" &&
                    update_command.value.type == SqlValueType::Boolean &&
                    update_command.value.value == "FALSE",
                "UPDATE should preserve its table, assignment, and value.") ||
        !expect(outer_and && outer_and->operation == SqlLogicalOperator::And,
                "AND should bind outside the parenthesized OR condition.")) {
        return false;
    }

    const auto* first_comparison =
        outer_and ? std::get_if<SqlComparisonCondition>(
                        &outer_and->left->expression)
                  : nullptr;
    const auto* parenthesized_or =
        outer_and ? std::get_if<SqlLogicalCondition>(
                        &outer_and->right->expression)
                  : nullptr;
    if (!expect(first_comparison &&
                    first_comparison->operation ==
                        SqlComparisonOperator::GreaterEqual &&
                    first_comparison->column == "id" &&
                    first_comparison->value.value == "10",
                "The first WHERE comparison should be represented.") ||
        !expect(parenthesized_or &&
                    parenthesized_or->operation == SqlLogicalOperator::Or,
                "Parenthesized OR conditions should be represented.")) {
        return false;
    }

    const auto* less_comparison =
        std::get_if<SqlComparisonCondition>(
            &parenthesized_or->left->expression);
    const auto* not_equal_comparison =
        std::get_if<SqlComparisonCondition>(
            &parenthesized_or->right->expression);
    const auto& delete_command = std::get<DeleteCommand>(*remove.command);
    const auto* delete_or =
        std::get_if<SqlLogicalCondition>(&delete_command.condition.expression);
    const auto* delete_left =
        delete_or ? std::get_if<SqlComparisonCondition>(
                        &delete_or->left->expression)
                  : nullptr;
    const auto* delete_right =
        delete_or ? std::get_if<SqlComparisonCondition>(
                        &delete_or->right->expression)
                  : nullptr;
    return expect(less_comparison &&
                      less_comparison->operation == SqlComparisonOperator::Less &&
                      less_comparison->value.value == "2.5" &&
                      not_equal_comparison &&
                      not_equal_comparison->operation ==
                          SqlComparisonOperator::NotEqual &&
                      not_equal_comparison->value.value == "unknown",
                  "Comparison operators and string values should be retained.") &&
           expect(delete_command.table == "student" && delete_or &&
                      delete_or->operation == SqlLogicalOperator::Or &&
                      delete_left &&
                      delete_left->operation == SqlComparisonOperator::Equal &&
                      delete_left->value.value == "-1" && delete_right &&
                      delete_right->value.type == SqlValueType::Boolean,
                  "DELETE should preserve its WHERE expression.");
}

bool test_syntax_errors() {
    const auto missing_create_target = parse_sql("CREATE school;");
    const auto missing_database_name = parse_sql("CREATE DATABASE;");
    const auto missing_show_target = parse_sql("SHOW TABLE;");
    const auto missing_describe_keyword = parse_sql("DESCRIBE student;");
    const auto missing_use_name = parse_sql("USE;");
    const auto missing_open = parse_sql("CREATE TABLE t id INT);");
    const auto missing_close = parse_sql("CREATE TABLE t (id INT;");
    const auto missing_type = parse_sql("CREATE TABLE t (id);");
    const auto trailing_comma = parse_sql("CREATE TABLE t (id INT,);");
    const auto empty_columns = parse_sql("CREATE TABLE t ();");
    const auto extra_input = parse_sql("SHOW TABLES; DROP TABLE t;");
    const auto missing_insert_values = parse_sql("INSERT INTO t (1, 2);");
    const auto missing_select_from = parse_sql("SELECT id student;");
    const auto missing_update_where = parse_sql("UPDATE t SET id = 1;");
    const auto missing_delete_where = parse_sql("DELETE FROM t;");
    const auto malformed_condition = parse_sql("DELETE FROM t WHERE id =;");
    const auto empty_insert = parse_sql("INSERT INTO t VALUES ();");
    const auto trailing_value_comma =
        parse_sql("INSERT INTO t VALUES (1,);");
    const auto select_null =
        parse_sql("SELECT id FROM t WHERE value IS NULL");
    const auto select_not_null =
        parse_sql("SELECT id FROM t WHERE value IS NOT NULL");
    bool parsed_is_null = false;
    bool parsed_is_not_null = false;
    if (select_null.command) {
        const auto* select = std::get_if<SelectCommand>(&*select_null.command);
        const auto* condition =
            select && select->condition
                ? std::get_if<SqlNullCondition>(
                      &select->condition->expression)
                : nullptr;
        parsed_is_null = condition && condition->column == "value" &&
                         !condition->is_not;
    }
    if (select_not_null.command) {
        const auto* select =
            std::get_if<SelectCommand>(&*select_not_null.command);
        const auto* condition =
            select && select->condition
                ? std::get_if<SqlNullCondition>(
                      &select->condition->expression)
                : nullptr;
        parsed_is_not_null = condition && condition->column == "value" &&
                             condition->is_not;
    }

    return expect(!missing_create_target.command &&
                      missing_create_target.error &&
                      missing_create_target.error->message.find(
                          "DATABASE or TABLE") != std::string::npos,
                  "Missing CREATE target should have a useful diagnostic.") &&
           expect(!missing_database_name.command &&
                      missing_database_name.error &&
                      missing_database_name.error->message.find(
                          "database identifier") != std::string::npos,
                  "Missing database name should be rejected.") &&
           expect(!missing_show_target.command && missing_show_target.error,
                  "Invalid SHOW target should be rejected.") &&
           expect(!missing_describe_keyword.command &&
                      missing_describe_keyword.error &&
                      missing_describe_keyword.error->message.find("TABLE") !=
                          std::string::npos,
                  "DESCRIBE must be followed by TABLE.") &&
           expect(!missing_use_name.command && missing_use_name.error,
                  "USE requires a database identifier.") &&
           expect(!missing_open.command && missing_open.error &&
                      missing_open.error->message.find("'('") !=
                          std::string::npos,
                  "CREATE TABLE requires an opening parenthesis.") &&
           expect(!missing_close.command && missing_close.error &&
                      missing_close.error->message.find("')'") !=
                          std::string::npos,
                  "CREATE TABLE requires a closing parenthesis.") &&
           expect(!missing_type.command && missing_type.error,
                  "Every column must have a supported type.") &&
           expect(!trailing_comma.command && trailing_comma.error,
                  "A trailing column comma should be rejected.") &&
           expect(!empty_columns.command && empty_columns.error,
                  "CREATE TABLE requires at least one column.") &&
           expect(!missing_insert_values.command && missing_insert_values.error,
                  "INSERT requires INTO, VALUES, and a value list.") &&
           expect(!missing_select_from.command && missing_select_from.error &&
                      missing_select_from.error->message.find("FROM") !=
                          std::string::npos,
                  "SELECT requires FROM and a table name.") &&
           expect(!missing_update_where.command && missing_update_where.error,
                  "UPDATE requires a WHERE condition.") &&
           expect(!missing_delete_where.command && missing_delete_where.error,
                  "DELETE requires a WHERE condition.") &&
           expect(!malformed_condition.command && malformed_condition.error,
                  "A comparison requires a value.") &&
           expect(!empty_insert.command && empty_insert.error,
                  "INSERT requires at least one value.") &&
           expect(!trailing_value_comma.command && trailing_value_comma.error,
                  "INSERT must reject a trailing value comma.") &&
           expect(parsed_is_null,
                  "IS NULL should parse as a null predicate.") &&
           expect(parsed_is_not_null,
                  "IS NOT NULL should parse as a null predicate.") &&
           expect(!extra_input.command && extra_input.error &&
                      extra_input.error->message.find("one SQL command") !=
                          std::string::npos,
                  "Input containing multiple commands should be rejected.");
}

bool test_lexical_and_position_errors() {
    const auto lexical_error = parse_sql("SHOW @");
    const auto syntax_error = parse_sql("CREATE DATABASE\n;");
    const auto missing_semicolon_error =
        parse_sql("SHOW DATABASES; unexpected");
    return expect(!lexical_error.command && lexical_error.error &&
                      lexical_error.error->message.find(
                          "Unexpected character") != std::string::npos &&
                      lexical_error.error->line == 1 &&
                      lexical_error.error->column == 6,
                  "Lexical diagnostics should be propagated with positions.") &&
           expect(!syntax_error.command && syntax_error.error &&
                      syntax_error.error->line == 2 &&
                      syntax_error.error->column == 1,
                  "Syntax errors should include line and column.") &&
           expect(!missing_semicolon_error.command &&
                      missing_semicolon_error.error,
                  "Unexpected trailing tokens should be rejected.");
}

}  // namespace

int main() {
    if (!test_database_commands() || !test_table_commands_and_column_types() ||
        !test_insert_select_commands() ||
        !test_update_delete_and_conditions() ||
        !test_syntax_errors() || !test_lexical_and_position_errors()) {
        return 1;
    }
    return 0;
}
