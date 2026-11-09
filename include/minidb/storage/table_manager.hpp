#pragma once

#include "minidb/storage/database_manager.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minidb::storage {

enum class ColumnType {
    Int,
    Float,
    Double,
    Varchar,
    Text,
    Boolean
};

struct ColumnDefinition {
    std::string name;
    ColumnType type;
};

struct TableSchema {
    std::string name;
    std::vector<ColumnDefinition> columns;
};

struct TableOperationResult {
    bool success = false;
    std::string message;
};

struct TableListResult {
    bool success = false;
    std::vector<std::string> tables;
    std::string message;
};

struct TableSchemaResult {
    bool success = false;
    TableSchema schema;
    std::string message;
};

std::string_view column_type_name(ColumnType type) noexcept;
std::optional<ColumnType> parse_column_type(std::string_view name) noexcept;

class TableManager {
public:
    explicit TableManager(const DatabaseManager& database_manager);

    TableOperationResult create_table(
        std::string_view name,
        const std::vector<ColumnDefinition>& columns);
    TableOperationResult drop_table(std::string_view name);
    TableListResult show_tables() const;
    TableSchemaResult describe_table(std::string_view name) const;

private:
    const DatabaseManager& database_manager_;
};

}  // namespace minidb::storage
