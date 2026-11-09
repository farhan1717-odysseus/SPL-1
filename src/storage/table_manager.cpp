#include "minidb/storage/table_manager.hpp"
#include "storage_utils.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>
#include <system_error>
#include <utility>

namespace minidb::storage {
namespace {

using detail::is_identifier;
using detail::normalized_name;

constexpr const char* tables_directory_name = "tables";
constexpr const char* schema_file_name = "schema.meta";
constexpr const char* schema_signature = "MINIDB_TABLE_SCHEMA";
constexpr const char* schema_version = "1";

struct DirectoryResult {
    bool success = false;
    std::filesystem::path path;
    bool exists = false;
    std::string message;
};

struct SchemasResult {
    bool success = false;
    std::vector<TableSchema> schemas;
    std::string message;
};

bool is_missing_path_error(const std::error_code& error) {
    return error == std::errc::no_such_file_or_directory;
}

DirectoryResult tables_directory(const DatabaseManager& manager,
                                 bool create_if_missing) {
    const auto database_path = manager.selected_database_path();
    if (!database_path) {
        return {false, {}, false,
                "No database is selected. Use a database before managing "
                "tables."};
    }

    std::error_code error;
    const auto database_status =
        std::filesystem::symlink_status(*database_path, error);
    if (error || std::filesystem::is_symlink(database_status) ||
        !std::filesystem::is_directory(database_status)) {
        const std::string detail =
            error ? error.message() : "selected database is not a directory";
        return {false, {}, false, "Cannot access selected database: " + detail};
    }

    const auto path = *database_path / tables_directory_name;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error && !is_missing_path_error(error)) {
        return {false, {}, false,
                "Cannot inspect table directory: " + error.message()};
    }
    if (!error && status.type() != std::filesystem::file_type::not_found) {
        if (std::filesystem::is_symlink(status) ||
            !std::filesystem::is_directory(status)) {
            return {false, {}, false,
                    "The database tables path must be a regular directory."};
        }
        return {true, path, true, {}};
    }

    if (!create_if_missing) {
        return {true, path, false, {}};
    }

    error.clear();
    const bool created = std::filesystem::create_directory(path, error);
    if (error || !created) {
        const std::string detail =
            error ? error.message() : "path already exists";
        return {false, {}, false,
                "Cannot create table directory: " + detail};
    }
    return {true, path, true, {}};
}

bool valid_columns(const std::vector<ColumnDefinition>& columns,
                   std::string& message) {
    if (columns.empty()) {
        message = "A table must have at least one column.";
        return false;
    }

    std::set<std::string> names;
    for (const auto& column : columns) {
        if (!is_identifier(column.name)) {
            message = "Invalid column name '" + column.name +
                      "'. Use a letter or underscore first, followed by "
                      "letters, digits, or underscores.";
            return false;
        }
        if (!names.insert(normalized_name(column.name)).second) {
            message = "Duplicate column name: " + column.name;
            return false;
        }
        if (column_type_name(column.type).empty()) {
            message = "Unsupported column type for column '" + column.name +
                      "'.";
            return false;
        }
    }
    return true;
}

bool read_schema(const std::filesystem::path& table_path,
                 TableSchema& schema,
                 std::string& message) {
    const auto schema_path = table_path / schema_file_name;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(schema_path, error);
    if (error || std::filesystem::is_symlink(status) ||
        !std::filesystem::is_regular_file(status)) {
        const std::string detail =
            error ? error.message() : "schema file is missing or not regular";
        message = "Cannot access table schema: " + detail;
        return false;
    }

    std::ifstream input(schema_path, std::ios::binary);
    if (!input) {
        message = "Cannot open table schema.";
        return false;
    }

    std::string signature;
    std::string version;
    std::string name;
    std::string count_line;
    if (!std::getline(input, signature) || !std::getline(input, version) ||
        !std::getline(input, name) || !std::getline(input, count_line) ||
        signature != schema_signature || version != schema_version ||
        !is_identifier(name) ||
        normalized_name(name) !=
            normalized_name(table_path.filename().string())) {
        message = "Table schema has an unsupported or invalid format.";
        return false;
    }

    std::size_t column_count = 0;
    const auto count_result =
        std::from_chars(count_line.data(),
                        count_line.data() + count_line.size(), column_count);
    if (count_result.ec != std::errc{} ||
        count_result.ptr != count_line.data() + count_line.size() ||
        column_count == 0) {
        message = "Table schema has an invalid column count.";
        return false;
    }
    const auto schema_size = std::filesystem::file_size(schema_path, error);
    if (error || column_count > schema_size / 4) {
        message = error ? "Cannot inspect table schema size: " + error.message()
                        : "Table schema column count exceeds its file size.";
        return false;
    }

    TableSchema parsed_schema;
    parsed_schema.name = std::move(name);
    for (std::size_t index = 0; index < column_count; ++index) {
        std::string column_name;
        std::string type_name;
        if (!std::getline(input, column_name) ||
            !std::getline(input, type_name)) {
            message = "Table schema is incomplete.";
            return false;
        }
        const auto type = parse_column_type(type_name);
        if (!type) {
            message = "Table schema contains an unsupported column type.";
            return false;
        }
        parsed_schema.columns.push_back({std::move(column_name), *type});
    }

    std::string unexpected;
    if (std::getline(input, unexpected)) {
        message = "Table schema contains unexpected data.";
        return false;
    }
    if (!input.eof()) {
        message = "Cannot finish reading table schema.";
        return false;
    }
    if (!valid_columns(parsed_schema.columns, message)) {
        return false;
    }

    schema = std::move(parsed_schema);
    return true;
}

SchemasResult list_schemas(const DirectoryResult& directory) {
    if (!directory.success) {
        return {false, {}, directory.message};
    }
    if (!directory.exists) {
        return {true, {}, {}};
    }

    std::error_code error;
    std::filesystem::directory_iterator entry(directory.path, error);
    if (error) {
        return {false, {}, "Cannot list tables: " + error.message()};
    }

    SchemasResult result{true, {}, {}};
    std::set<std::string> names;
    const std::filesystem::directory_iterator end;
    while (entry != end) {
        const auto path = entry->path();
        const bool symbolic_link = entry->is_symlink(error);
        if (error || symbolic_link) {
            const std::string detail =
                error ? error.message() : "symbolic links are not allowed";
            return {false, {}, "Cannot inspect table entry: " + detail};
        }

        const bool is_directory = entry->is_directory(error);
        if (error || !is_directory) {
            const std::string detail =
                error ? error.message() : "unexpected non-directory entry";
            return {false, {}, "Invalid table entry: " + detail};
        }

        TableSchema schema;
        std::string message;
        if (!is_identifier(path.filename().string()) ||
            !read_schema(path, schema, message)) {
            if (message.empty()) {
                message = "Table directory has an invalid name.";
            }
            return {false, {}, "Invalid table '" +
                                   path.filename().string() + "': " + message};
        }
        if (!names.insert(normalized_name(schema.name)).second) {
            return {false, {}, "Duplicate table name found: " + schema.name};
        }
        result.schemas.push_back(std::move(schema));

        entry.increment(error);
        if (error) {
            return {false, {}, "Cannot continue listing tables: " +
                                   error.message()};
        }
    }

    std::sort(result.schemas.begin(), result.schemas.end(),
              [](const TableSchema& left, const TableSchema& right) {
                  return normalized_name(left.name) <
                         normalized_name(right.name);
              });
    return result;
}

TableOperationResult write_schema(const std::filesystem::path& table_path,
                                  const TableSchema& schema) {
    const auto temporary_path =
        table_path / (std::string(schema_file_name) + ".tmp");
    {
        std::ofstream output(temporary_path,
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            return {false, "Cannot create table schema."};
        }
        output << schema_signature << '\n'
               << schema_version << '\n'
               << schema.name << '\n'
               << schema.columns.size() << '\n';
        for (const auto& column : schema.columns) {
            output << column.name << '\n' << column_type_name(column.type)
                   << '\n';
        }
        output.flush();
        if (!output) {
            output.close();
            std::error_code cleanup_error;
            std::filesystem::remove(temporary_path, cleanup_error);
            const std::string cleanup_detail =
                cleanup_error ? " Cleanup failed: " + cleanup_error.message()
                              : "";
            return {false, "Cannot write table schema." + cleanup_detail};
        }
        output.close();
        if (!output) {
            std::error_code cleanup_error;
            std::filesystem::remove(temporary_path, cleanup_error);
            const std::string cleanup_detail =
                cleanup_error ? " Cleanup failed: " + cleanup_error.message()
                              : "";
            return {false, "Cannot finalize table schema." + cleanup_detail};
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary_path, table_path / schema_file_name,
                            error);
    if (error) {
        const std::string rename_message = error.message();
        std::error_code cleanup_error;
        std::filesystem::remove(temporary_path, cleanup_error);
        const std::string cleanup_detail =
            cleanup_error ? " Cleanup failed: " + cleanup_error.message() : "";
        return {false,
                "Cannot finalize table schema: " + rename_message +
                    cleanup_detail};
    }
    return {true, "Table created: " + schema.name};
}

}  // namespace

std::string_view column_type_name(ColumnType type) noexcept {
    switch (type) {
        case ColumnType::Int:
            return "INT";
        case ColumnType::Float:
            return "FLOAT";
        case ColumnType::Double:
            return "DOUBLE";
        case ColumnType::Varchar:
            return "VARCHAR";
        case ColumnType::Text:
            return "TEXT";
        case ColumnType::Boolean:
            return "BOOLEAN";
    }
    return {};
}

std::optional<ColumnType> parse_column_type(std::string_view name) noexcept {
    if (detail::equal_ascii_case_insensitive(name, "INT")) {
        return ColumnType::Int;
    }
    if (detail::equal_ascii_case_insensitive(name, "FLOAT")) {
        return ColumnType::Float;
    }
    if (detail::equal_ascii_case_insensitive(name, "DOUBLE")) {
        return ColumnType::Double;
    }
    if (detail::equal_ascii_case_insensitive(name, "VARCHAR")) {
        return ColumnType::Varchar;
    }
    if (detail::equal_ascii_case_insensitive(name, "TEXT")) {
        return ColumnType::Text;
    }
    if (detail::equal_ascii_case_insensitive(name, "BOOLEAN")) {
        return ColumnType::Boolean;
    }
    return std::nullopt;
}

TableManager::TableManager(const DatabaseManager& database_manager)
    : database_manager_(database_manager) {}

TableOperationResult TableManager::create_table(
    std::string_view name,
    const std::vector<ColumnDefinition>& columns) {
    if (!is_identifier(name)) {
        return {false, "Invalid table name."};
    }
    std::string validation_message;
    if (!valid_columns(columns, validation_message)) {
        return {false, std::move(validation_message)};
    }

    const auto directory = tables_directory(database_manager_, false);
    const auto current = list_schemas(directory);
    if (!current.success) {
        return {false, current.message};
    }
    const auto normalized = normalized_name(name);
    const auto existing = std::find_if(
        current.schemas.begin(), current.schemas.end(),
        [&](const TableSchema& schema) {
            return normalized_name(schema.name) == normalized;
        });
    if (existing != current.schemas.end()) {
        return {false, "Table already exists: " + std::string(name)};
    }

    const auto target_directory = tables_directory(database_manager_, true);
    if (!target_directory.success) {
        return {false, target_directory.message};
    }

    const auto table_path = target_directory.path / std::string(name);
    std::error_code error;
    if (!std::filesystem::create_directory(table_path, error) || error) {
        const std::string detail =
            error ? error.message() : "path already exists";
        return {false, "Cannot create table '" + std::string(name) +
                           "': " + detail};
    }

    const TableSchema schema{std::string(name), columns};
    const auto written = write_schema(table_path, schema);
    if (!written.success) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(table_path, cleanup_error);
        if (cleanup_error) {
            return {false, written.message +
                               " Cleanup failed: " + cleanup_error.message()};
        }
    }
    return written;
}

TableOperationResult TableManager::drop_table(std::string_view name) {
    if (!is_identifier(name)) {
        return {false, "Invalid table name."};
    }
    const auto directory = tables_directory(database_manager_, false);
    const auto current = list_schemas(directory);
    if (!current.success) {
        return {false, current.message};
    }

    const auto normalized = normalized_name(name);
    const auto found = std::find_if(
        current.schemas.begin(), current.schemas.end(),
        [&](const TableSchema& schema) {
            return normalized_name(schema.name) == normalized;
        });
    if (found == current.schemas.end()) {
        return {false, "Table does not exist: " + std::string(name)};
    }

    const auto table_path = directory.path / found->name;
    std::error_code error;
    const auto removed = std::filesystem::remove_all(table_path, error);
    if (error || removed == 0) {
        const std::string detail =
            error ? error.message() : "table directory was not removed";
        return {false, "Cannot drop table '" + found->name + "': " + detail};
    }
    return {true, "Table dropped: " + found->name};
}

TableListResult TableManager::show_tables() const {
    const auto current =
        list_schemas(tables_directory(database_manager_, false));
    if (!current.success) {
        return {false, {}, current.message};
    }

    TableListResult result{true, {}, {}};
    result.tables.reserve(current.schemas.size());
    for (const auto& schema : current.schemas) {
        result.tables.push_back(schema.name);
    }
    return result;
}

TableSchemaResult TableManager::describe_table(std::string_view name) const {
    if (!is_identifier(name)) {
        return {false, {}, "Invalid table name."};
    }

    const auto directory = tables_directory(database_manager_, false);
    const auto current = list_schemas(directory);
    if (!current.success) {
        return {false, {}, current.message};
    }

    const auto normalized = normalized_name(name);
    const auto found = std::find_if(
        current.schemas.begin(), current.schemas.end(),
        [&](const TableSchema& schema) {
            return normalized_name(schema.name) == normalized;
        });
    if (found == current.schemas.end()) {
        return {false, {}, "Table does not exist: " + std::string(name)};
    }
    return {true, *found, {}};
}

}  // namespace minidb::storage
