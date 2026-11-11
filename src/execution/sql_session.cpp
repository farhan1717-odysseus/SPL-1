#include "minidb/execution/sql_session.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace minidb::execution {
namespace {

using parser::SqlComparisonCondition;
using parser::SqlComparisonOperator;
using parser::SqlCondition;
using parser::SqlDataType;
using parser::SqlLogicalCondition;
using parser::SqlLogicalOperator;
using parser::SqlNullCondition;
using parser::SqlValue;
using parser::SqlValueType;
using storage::ColumnDefinition;
using storage::ColumnType;
using storage::Record;
using storage::RecordValue;
using storage::TableSchema;

template <typename... Functions>
struct Overloaded : Functions... {
    using Functions::operator()...;
};
template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;

char ascii_lower(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character + ('a' - 'A'));
    }
    return character;
}

bool same_identifier(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> find_column(const TableSchema& schema,
                                       std::string_view name) {
    for (std::size_t index = 0; index < schema.columns.size(); ++index) {
        if (same_identifier(schema.columns[index].name, name)) {
            return index;
        }
    }
    return std::nullopt;
}

std::string data_type_name(ColumnType type) {
    switch (type) {
        case ColumnType::Int: return "INT";
        case ColumnType::Float: return "FLOAT";
        case ColumnType::Double: return "DOUBLE";
        case ColumnType::Varchar: return "VARCHAR";
        case ColumnType::Text: return "TEXT";
        case ColumnType::Boolean: return "BOOLEAN";
    }
    return "UNKNOWN";
}

bool convert_value(const SqlValue& input, ColumnType column_type,
                   RecordValue& output, std::string& message) {
    if (input.type == SqlValueType::Null) {
        output = std::monostate{};
        return true;
    }

    switch (column_type) {
        case ColumnType::Int:
            if (input.type == SqlValueType::Integer) {
                std::int64_t value = 0;
                const auto parsed = std::from_chars(
                    input.value.data(), input.value.data() + input.value.size(),
                    value);
                if (parsed.ec == std::errc{} &&
                    parsed.ptr == input.value.data() + input.value.size()) {
                    output = value;
                    return true;
                }
            }
            break;
        case ColumnType::Float:
        case ColumnType::Double:
            if (input.type == SqlValueType::Integer ||
                input.type == SqlValueType::FloatingPoint) {
                double value = 0.0;
                const auto parsed = std::from_chars(
                    input.value.data(), input.value.data() + input.value.size(),
                    value, std::chars_format::general);
                if (parsed.ec == std::errc{} &&
                    parsed.ptr == input.value.data() + input.value.size() &&
                    std::isfinite(value)) {
                    if (column_type == ColumnType::Float) {
                        if (std::abs(value) >
                            static_cast<double>(
                                (std::numeric_limits<float>::max)())) {
                            break;
                        }
                        output = static_cast<float>(value);
                    } else {
                        output = value;
                    }
                    return true;
                }
            }
            break;
        case ColumnType::Varchar:
        case ColumnType::Text:
            if (input.type == SqlValueType::String) {
                output = input.value;
                return true;
            }
            break;
        case ColumnType::Boolean:
            if (input.type == SqlValueType::Boolean) {
                output = same_identifier(input.value, "TRUE");
                return true;
            }
            break;
    }

    message = "Value '" + input.value + "' is not valid for " +
              data_type_name(column_type) + ".";
    return false;
}

struct BoundComparison {
    std::size_t column_index;
    SqlComparisonOperator operation;
    RecordValue value;
};

struct BoundNullTest {
    std::size_t column_index;
    bool is_not;
};

struct BoundCondition;

struct BoundLogical {
    SqlLogicalOperator operation;
    std::shared_ptr<const BoundCondition> left;
    std::shared_ptr<const BoundCondition> right;
};

struct BoundCondition {
    std::variant<BoundComparison, BoundNullTest, BoundLogical> expression;
};

std::optional<BoundCondition> bind_condition(const SqlCondition& condition,
                                             const TableSchema& schema,
                                             std::string& message) {
    if (const auto* comparison =
            std::get_if<SqlComparisonCondition>(&condition.expression)) {
        const auto column = find_column(schema, comparison->column);
        if (!column) {
            message = "Unknown column in WHERE condition: " +
                      comparison->column;
            return std::nullopt;
        }
        RecordValue value;
        if (!convert_value(comparison->value, schema.columns[*column].type,
                           value, message)) {
            message = "Invalid WHERE value for column '" +
                      schema.columns[*column].name + "': " + message;
            return std::nullopt;
        }
        return BoundCondition{BoundComparison{*column, comparison->operation,
                                              std::move(value)}};
    }

    if (const auto* null_test =
            std::get_if<SqlNullCondition>(&condition.expression)) {
        const auto column = find_column(schema, null_test->column);
        if (!column) {
            message = "Unknown column in WHERE condition: " +
                      null_test->column;
            return std::nullopt;
        }
        return BoundCondition{BoundNullTest{*column, null_test->is_not}};
    }

    const auto* logical =
        std::get_if<SqlLogicalCondition>(&condition.expression);
    if (!logical || !logical->left || !logical->right) {
        message = "Invalid WHERE condition.";
        return std::nullopt;
    }
    auto left = bind_condition(*logical->left, schema, message);
    if (!left) {
        return std::nullopt;
    }
    auto right = bind_condition(*logical->right, schema, message);
    if (!right) {
        return std::nullopt;
    }
    return BoundCondition{BoundLogical{
        logical->operation,
        std::make_shared<const BoundCondition>(std::move(*left)),
        std::make_shared<const BoundCondition>(std::move(*right))}};
}

enum class TruthValue { False, True, Unknown };

int compare_values(const RecordValue& left, const RecordValue& right) {
    const auto numeric_value = [](const RecordValue& value)
        -> std::optional<long double> {
        if (const auto* number = std::get_if<std::int64_t>(&value)) {
            return static_cast<long double>(*number);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return static_cast<long double>(*number);
        }
        if (const auto* number = std::get_if<double>(&value)) {
            return static_cast<long double>(*number);
        }
        return std::nullopt;
    };
    const auto left_number = numeric_value(left);
    const auto right_number = numeric_value(right);
    if (left_number && right_number) {
        return *left_number < *right_number ? -1
               : *left_number > *right_number ? 1
                                            : 0;
    }
    if (const auto* left_text = std::get_if<std::string>(&left)) {
        const auto& right_text = std::get<std::string>(right);
        return *left_text < right_text ? -1 : (*left_text > right_text ? 1 : 0);
    }
    const bool left_boolean = std::get<bool>(left);
    const bool right_boolean = std::get<bool>(right);
    return left_boolean == right_boolean ? 0 : (left_boolean ? 1 : -1);
}

bool compare_result(int comparison, SqlComparisonOperator operation) {
    switch (operation) {
        case SqlComparisonOperator::Equal: return comparison == 0;
        case SqlComparisonOperator::NotEqual: return comparison != 0;
        case SqlComparisonOperator::Less: return comparison < 0;
        case SqlComparisonOperator::Greater: return comparison > 0;
        case SqlComparisonOperator::LessEqual: return comparison <= 0;
        case SqlComparisonOperator::GreaterEqual: return comparison >= 0;
    }
    return false;
}

TruthValue evaluate_condition(const BoundCondition& condition,
                              const Record& row) {
    if (const auto* comparison =
            std::get_if<BoundComparison>(&condition.expression)) {
        const auto& value = row[comparison->column_index];
        if (std::holds_alternative<std::monostate>(value) ||
            std::holds_alternative<std::monostate>(comparison->value)) {
            return TruthValue::Unknown;
        }
        return compare_result(compare_values(value, comparison->value),
                              comparison->operation)
                   ? TruthValue::True
                   : TruthValue::False;
    }
    if (const auto* null_test =
            std::get_if<BoundNullTest>(&condition.expression)) {
        const bool is_null = std::holds_alternative<std::monostate>(
            row[null_test->column_index]);
        return is_null != null_test->is_not ? TruthValue::True
                                            : TruthValue::False;
    }

    const auto& logical = std::get<BoundLogical>(condition.expression);
    const TruthValue left = evaluate_condition(*logical.left, row);
    const TruthValue right = evaluate_condition(*logical.right, row);
    if (logical.operation == SqlLogicalOperator::And) {
        if (left == TruthValue::False || right == TruthValue::False) {
            return TruthValue::False;
        }
        return left == TruthValue::True && right == TruthValue::True
                   ? TruthValue::True
                   : TruthValue::Unknown;
    }
    if (left == TruthValue::True || right == TruthValue::True) {
        return TruthValue::True;
    }
    return left == TruthValue::False && right == TruthValue::False
               ? TruthValue::False
               : TruthValue::Unknown;
}

std::string format_value(const RecordValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        return "NULL";
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*integer);
    }
    if (const auto* real = std::get_if<double>(&value)) {
        char buffer[128];
        const auto converted = std::to_chars(
            buffer, buffer + sizeof(buffer), *real, std::chars_format::general);
        if (converted.ec != std::errc{}) {
            throw std::runtime_error("Cannot format a DOUBLE value.");
        }
        return std::string(buffer, converted.ptr);
    }
    if (const auto* real = std::get_if<float>(&value)) {
        char buffer[64];
        const auto converted = std::to_chars(
            buffer, buffer + sizeof(buffer), *real, std::chars_format::general);
        if (converted.ec != std::errc{}) {
            throw std::runtime_error("Cannot format a FLOAT value.");
        }
        return std::string(buffer, converted.ptr);
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return *text;
    }
    return std::get<bool>(value) ? "TRUE" : "FALSE";
}

std::string escape_cell(std::string_view value) {
    std::string escaped;
    for (const char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '|': escaped += "\\|"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

std::string format_query(const std::vector<std::string>& columns,
                         const std::vector<Record>& rows,
                         const std::vector<std::size_t>& indexes) {
    std::vector<std::vector<std::string>> cells;
    cells.reserve(rows.size());
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    for (const auto& column : columns) {
        widths.push_back(escape_cell(column).size());
    }
    for (const auto& row : rows) {
        std::vector<std::string> row_cells;
        row_cells.reserve(indexes.size());
        for (std::size_t index = 0; index < indexes.size(); ++index) {
            row_cells.push_back(escape_cell(format_value(row[indexes[index]])));
            widths[index] = (std::max)(widths[index], row_cells.back().size());
        }
        cells.push_back(std::move(row_cells));
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < columns.size(); ++index) {
        if (index != 0) output << " | ";
        output << escape_cell(columns[index])
               << std::string(widths[index] - escape_cell(columns[index]).size(),
                              ' ');
    }
    output << '\n';
    for (std::size_t index = 0; index < columns.size(); ++index) {
        if (index != 0) output << "-+-";
        output << std::string(widths[index], '-');
    }
    output << '\n';
    for (const auto& row_cells : cells) {
        for (std::size_t index = 0; index < indexes.size(); ++index) {
            if (index != 0) output << " | ";
            output << row_cells[index]
                   << std::string(widths[index] - row_cells[index].size(), ' ');
        }
        output << '\n';
    }
    output << rows.size() << (rows.size() == 1 ? " row\n" : " rows\n");
    return output.str();
}

CommandResult failure(std::string message) {
    return {"Error: " + std::move(message) + "\n", false};
}

std::vector<std::size_t> all_column_indexes(const TableSchema& schema) {
    std::vector<std::size_t> indexes;
    indexes.reserve(schema.columns.size());
    for (std::size_t index = 0; index < schema.columns.size(); ++index) {
        indexes.push_back(index);
    }
    return indexes;
}

}  // namespace

SqlSession::SqlSession(std::filesystem::path data_directory)
    : database_manager_(std::move(data_directory)),
      table_manager_(database_manager_), record_store_(database_manager_) {}

storage::DatabaseOperationResult SqlSession::initialize() {
    return database_manager_.initialize();
}

CommandResult SqlSession::execute(const parser::SqlCommand& command) {
    return std::visit(
        Overloaded{
            [this](const parser::CreateDatabaseCommand& item) {
                const auto result =
                    database_manager_.create_database(item.name);
                return result.success ? CommandResult{result.message + "\n"}
                                      : failure(result.message);
            },
            [this](const parser::DropDatabaseCommand& item) {
                const auto result = database_manager_.drop_database(item.name);
                return result.success ? CommandResult{result.message + "\n"}
                                      : failure(result.message);
            },
            [this](const parser::ShowDatabasesCommand&) {
                const auto result = database_manager_.show_databases();
                if (!result.success) return failure(result.message);
                std::ostringstream output;
                output << "Databases:\n";
                for (const auto& database : result.databases) {
                    output << "  " << database << '\n';
                }
                if (result.databases.empty()) output << "  (none)\n";
                return CommandResult{output.str()};
            },
            [this](const parser::UseDatabaseCommand& item) {
                const auto result = database_manager_.use_database(item.name);
                return result.success ? CommandResult{result.message + "\n"}
                                      : failure(result.message);
            },
            [this](const parser::CreateTableCommand& item) {
                std::vector<ColumnDefinition> columns;
                columns.reserve(item.columns.size());
                for (const auto& column : item.columns) {
                    ColumnType type = ColumnType::Int;
                    switch (column.type) {
                        case SqlDataType::Int: type = ColumnType::Int; break;
                        case SqlDataType::Float: type = ColumnType::Float; break;
                        case SqlDataType::Double: type = ColumnType::Double; break;
                        case SqlDataType::Varchar:
                            type = ColumnType::Varchar;
                            break;
                        case SqlDataType::Text: type = ColumnType::Text; break;
                        case SqlDataType::Boolean:
                            type = ColumnType::Boolean;
                            break;
                        default:
                            return failure("Unsupported column type.");
                    }
                    columns.push_back({column.name, type});
                }
                const auto result =
                    table_manager_.create_table(item.name, columns);
                return result.success ? CommandResult{result.message + "\n"}
                                      : failure(result.message);
            },
            [this](const parser::DropTableCommand& item) {
                const auto result = table_manager_.drop_table(item.name);
                return result.success ? CommandResult{result.message + "\n"}
                                      : failure(result.message);
            },
            [this](const parser::ShowTablesCommand&) {
                const auto result = table_manager_.show_tables();
                if (!result.success) return failure(result.message);
                std::ostringstream output;
                output << "Tables:\n";
                for (const auto& table : result.tables) {
                    output << "  " << table << '\n';
                }
                if (result.tables.empty()) output << "  (none)\n";
                return CommandResult{output.str()};
            },
            [this](const parser::DescribeTableCommand& item) {
                const auto result = table_manager_.describe_table(item.name);
                if (!result.success) return failure(result.message);
                std::ostringstream output;
                output << "Table " << result.schema.name << ":\n";
                for (const auto& column : result.schema.columns) {
                    output << "  " << column.name << ' '
                           << data_type_name(column.type)
                           << '\n';
                }
                return CommandResult{output.str()};
            },
            [this](const parser::InsertCommand& item) {
                const auto schema_result =
                    table_manager_.describe_table(item.table);
                if (!schema_result.success) return failure(schema_result.message);
                const auto& schema = schema_result.schema;
                if (item.values.size() != schema.columns.size()) {
                    return failure("INSERT value count does not match the "
                                   "table schema.");
                }
                Record row;
                row.reserve(item.values.size());
                for (std::size_t index = 0; index < item.values.size(); ++index) {
                    RecordValue value;
                    std::string message;
                    if (!convert_value(item.values[index],
                                       schema.columns[index].type, value,
                                       message)) {
                        return failure("Column '" + schema.columns[index].name +
                                       "': " + message);
                    }
                    row.push_back(std::move(value));
                }
                auto records = record_store_.read(item.table);
                if (!records.success) return failure(records.message);
                records.rows.push_back(std::move(row));
                const auto written = record_store_.write(item.table, records.rows);
                if (!written.success) return failure(written.message);
                return CommandResult{"1 row inserted.\n"};
            },
            [this](const parser::SelectCommand& item) {
                const auto schema_result =
                    table_manager_.describe_table(item.table);
                if (!schema_result.success) return failure(schema_result.message);
                const auto& schema = schema_result.schema;
                std::vector<std::size_t> indexes;
                std::vector<std::string> columns;
                if (item.select_all) {
                    indexes = all_column_indexes(schema);
                    for (const auto& column : schema.columns) {
                        columns.push_back(column.name);
                    }
                } else {
                    for (const auto& name : item.columns) {
                        const auto index = find_column(schema, name);
                        if (!index) {
                            return failure("Unknown selected column: " + name);
                        }
                        indexes.push_back(*index);
                        columns.push_back(schema.columns[*index].name);
                    }
                }
                const auto records = record_store_.read(item.table);
                if (!records.success) return failure(records.message);
                std::vector<Record> selected_rows;
                if (item.condition) {
                    std::string message;
                    auto condition =
                        bind_condition(*item.condition, schema, message);
                    if (!condition) return failure(message);
                    for (const auto& row : records.rows) {
                        if (evaluate_condition(*condition, row) ==
                            TruthValue::True) {
                            selected_rows.push_back(row);
                        }
                    }
                } else {
                    selected_rows = records.rows;
                }
                return CommandResult{
                    format_query(columns, selected_rows, indexes)};
            },
            [this](const parser::UpdateCommand& item) {
                const auto schema_result =
                    table_manager_.describe_table(item.table);
                if (!schema_result.success) return failure(schema_result.message);
                const auto& schema = schema_result.schema;
                const auto target_index = find_column(schema, item.column);
                if (!target_index) {
                    return failure("Unknown update column: " + item.column);
                }
                RecordValue value;
                std::string message;
                if (!convert_value(item.value,
                                   schema.columns[*target_index].type, value,
                                   message)) {
                    return failure("Column '" + schema.columns[*target_index].name +
                                   "': " + message);
                }
                auto condition =
                    bind_condition(item.condition, schema, message);
                if (!condition) return failure(message);
                auto records = record_store_.read(item.table);
                if (!records.success) return failure(records.message);
                std::size_t affected = 0;
                for (auto& row : records.rows) {
                    if (evaluate_condition(*condition, row) ==
                        TruthValue::True) {
                        row[*target_index] = value;
                        ++affected;
                    }
                }
                if (affected != 0) {
                    const auto written =
                        record_store_.write(item.table, records.rows);
                    if (!written.success) return failure(written.message);
                }
                return CommandResult{"Query OK, " + std::to_string(affected) +
                                     (affected == 1 ? " row updated.\n"
                                                    : " rows updated.\n")};
            },
            [this](const parser::DeleteCommand& item) {
                const auto schema_result =
                    table_manager_.describe_table(item.table);
                if (!schema_result.success) return failure(schema_result.message);
                std::string message;
                auto condition =
                    bind_condition(item.condition, schema_result.schema, message);
                if (!condition) return failure(message);
                auto records = record_store_.read(item.table);
                if (!records.success) return failure(records.message);
                const auto old_size = records.rows.size();
                records.rows.erase(
                    std::remove_if(records.rows.begin(), records.rows.end(),
                                   [&](const Record& row) {
                                       return evaluate_condition(*condition, row) ==
                                              TruthValue::True;
                                   }),
                    records.rows.end());
                const auto affected = old_size - records.rows.size();
                if (affected != 0) {
                    const auto written =
                        record_store_.write(item.table, records.rows);
                    if (!written.success) return failure(written.message);
                }
                return CommandResult{"Query OK, " + std::to_string(affected) +
                                     (affected == 1 ? " row deleted.\n"
                                                    : " rows deleted.\n")};
            }},
        command);
}

}  // namespace minidb::execution
