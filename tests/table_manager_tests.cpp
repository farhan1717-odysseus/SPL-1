#include "minidb/storage/database_manager.hpp"
#include "minidb/storage/table_manager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("minidb_table_manager_tests_" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

bool test_table_lifecycle_and_persistence() {
    TemporaryDirectory temporary;
    minidb::storage::DatabaseManager databases(temporary.path() / "data");
    if (!expect(databases.initialize().success &&
                    databases.create_database("school").success,
                "The test database should be initialized and created.")) {
        return false;
    }

    minidb::storage::TableManager tables(databases);
    if (!expect(!tables.show_tables().success,
                "Table operations should require a selected database.") ||
        !expect(!tables.create_table("student", {{"id", minidb::storage::ColumnType::Int}})
                     .success,
                "Table creation should require a selected database.") ||
        !expect(databases.use_database("school").success,
                "The test database should be selected.") ||
        !expect(tables.show_tables().success &&
                    tables.show_tables().tables.empty(),
                "A new database should contain no tables.")) {
        return false;
    }

    using minidb::storage::ColumnDefinition;
    using minidb::storage::ColumnType;
    const std::vector<ColumnDefinition> columns{
        {"id", ColumnType::Int},       {"rating", ColumnType::Float},
        {"average", ColumnType::Double}, {"name", ColumnType::Varchar},
        {"notes", ColumnType::Text},   {"active", ColumnType::Boolean}};
    if (!expect(tables.create_table("student", columns).success,
                "A table with supported columns should be created.") ||
        !expect(!tables.create_table("STUDENT", columns).success,
                "Table names should be unique case-insensitively.") ||
        !expect(!tables.create_table("../student", columns).success,
                "Path traversal is not a valid table name.") ||
        !expect(!tables.create_table("student data", columns).success,
                "Table names cannot contain spaces.") ||
        !expect(!tables.create_table("empty", {}).success,
                "A table must contain at least one column.") ||
        !expect(!tables.create_table(
                     "duplicate",
                     {{"id", ColumnType::Int}, {"ID", ColumnType::Text}})
                     .success,
                "Duplicate column names should be rejected case-insensitively.") ||
        !expect(!tables.create_table(
                     "invalid_column",
                     {{"2id", ColumnType::Int}})
                     .success,
                "Invalid column identifiers should be rejected.") ||
        !expect(!tables.create_table(
                     "invalid_type",
                     {{"id", static_cast<ColumnType>(999)}})
                     .success,
                "Unsupported column types should be rejected.")) {
        return false;
    }

    if (!expect(tables.create_table(
                    "_archive",
                    {{"entry_id", ColumnType::Int},
                     {"description", ColumnType::Text}})
                    .success,
                "A second table should be created in the same database.")) {
        return false;
    }

    const auto listed = tables.show_tables();
    if (!expect(listed.success && listed.tables.size() == 2 &&
                    listed.tables[0] == "_archive" &&
                    listed.tables[1] == "student",
                "SHOW TABLES should return sorted table names.")) {
        return false;
    }

    const auto described = tables.describe_table("STUDENT");
    if (!expect(described.success && described.schema.name == "student" &&
                    described.schema.columns.size() == columns.size(),
                "DESCRIBE should return the stored schema case-insensitively.")) {
        return false;
    }
    for (std::size_t index = 0; index < columns.size(); ++index) {
        if (!expect(described.schema.columns[index].name == columns[index].name &&
                        described.schema.columns[index].type ==
                            columns[index].type,
                    "DESCRIBE should preserve each column name and type.")) {
            return false;
        }
    }
    if (!expect(!tables.describe_table("missing").success,
                "DESCRIBE should reject nonexistent tables.") ||
        !expect(!tables.drop_table("missing").success,
                "DROP should reject nonexistent tables.")) {
        return false;
    }

    const auto schema_path =
        temporary.path() / "data" / "school" / "tables" / "student" /
        "schema.meta";
    std::ifstream schema_file(schema_path, std::ios::binary);
    std::string signature;
    std::string version;
    std::string name;
    std::string column_count;
    if (!expect(static_cast<bool>(schema_file) &&
                    static_cast<bool>(std::getline(schema_file, signature)) &&
                    static_cast<bool>(std::getline(schema_file, version)) &&
                    static_cast<bool>(std::getline(schema_file, name)) &&
                    static_cast<bool>(std::getline(schema_file, column_count)) &&
                    signature == "MINIDB_TABLE_SCHEMA" && version == "1" &&
                    name == "student" && column_count == "6",
                "The table schema should use its versioned persistent format.")) {
        return false;
    }
    schema_file.close();

    minidb::storage::DatabaseManager restarted_databases(
        temporary.path() / "data");
    if (!expect(restarted_databases.initialize().success &&
                    restarted_databases.use_database("school").success,
                "The database should remain selectable after restart.")) {
        return false;
    }
    minidb::storage::TableManager restarted_tables(restarted_databases);
    const auto persisted = restarted_tables.describe_table("student");
    if (!expect(persisted.success &&
                    persisted.schema.columns.size() == columns.size(),
                "Table schemas should persist across manager restarts.")) {
        return false;
    }
    for (std::size_t index = 0; index < columns.size(); ++index) {
        if (!expect(
                persisted.schema.columns[index].name == columns[index].name &&
                    persisted.schema.columns[index].type == columns[index].type,
                "Persisted column names and types should be unchanged.")) {
            return false;
        }
    }

    if (!expect(restarted_databases.create_database("archive").success &&
                    restarted_databases.use_database("archive").success,
                "A second database should be selectable.") ||
        !expect(restarted_tables.show_tables().success &&
                    restarted_tables.show_tables().tables.empty(),
                "Tables must be isolated to their owning database.") ||
        !expect(restarted_databases.use_database("school").success,
                "The original database should be selectable again.")) {
        return false;
    }

    if (!expect(tables.drop_table("STUDENT").success,
                "DROP should match table names case-insensitively.") ||
        !expect(!std::filesystem::exists(schema_path),
                "DROP should remove the table schema.") ||
        !expect(tables.show_tables().success &&
                    tables.show_tables().tables.size() == 1,
                "SHOW TABLES should reflect dropped tables.")) {
        return false;
    }
    return true;
}

bool test_column_types_and_corrupt_schema() {
    using minidb::storage::ColumnType;
    if (!expect(minidb::storage::parse_column_type("int") == ColumnType::Int &&
                    minidb::storage::parse_column_type("FLOAT") ==
                        ColumnType::Float &&
                    minidb::storage::parse_column_type("double") ==
                        ColumnType::Double &&
                    minidb::storage::parse_column_type("Varchar") ==
                        ColumnType::Varchar &&
                    minidb::storage::parse_column_type("text") ==
                        ColumnType::Text &&
                    minidb::storage::parse_column_type("boolean") ==
                        ColumnType::Boolean &&
                    !minidb::storage::parse_column_type("BLOB").has_value(),
                "The type parser should recognize only supported types.")) {
        return false;
    }

    TemporaryDirectory temporary;
    minidb::storage::DatabaseManager databases(temporary.path() / "data");
    if (!expect(databases.initialize().success &&
                    databases.create_database("school").success &&
                    databases.use_database("school").success,
                "The test database should be ready.")) {
        return false;
    }

    minidb::storage::TableManager tables(databases);
    if (!expect(tables.create_table(
                    "student", {{"id", ColumnType::Int}})
                    .success,
                "The table should be created before corrupting its schema.")) {
        return false;
    }

    std::ofstream corrupt_schema(
        temporary.path() / "data" / "school" / "tables" / "student" /
            "schema.meta",
        std::ios::binary | std::ios::trunc);
    corrupt_schema << "invalid schema\n";
    corrupt_schema.close();

    return expect(!tables.show_tables().success,
                  "SHOW TABLES should report corrupt schemas.") &&
           expect(!tables.describe_table("student").success,
                  "DESCRIBE should report corrupt schemas.") &&
           expect(!tables.drop_table("student").success,
                  "DROP should not delete a table with corrupt schema metadata.");
}

}  // namespace

int main() {
    return test_table_lifecycle_and_persistence() &&
                   test_column_types_and_corrupt_schema()
               ? 0
               : 1;
}
