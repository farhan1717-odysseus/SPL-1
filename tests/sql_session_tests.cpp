#include "minidb/execution/sql_session.hpp"
#include "minidb/parser/sql_parser.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("minidb_sql_session_tests_" + std::to_string(suffix));
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

minidb::execution::CommandResult execute(minidb::execution::SqlSession& session,
                                         std::string_view sql) {
    const auto parsed = minidb::parser::parse_sql(sql);
    if (!parsed.command) {
        return {"Parse failed: " +
                    (parsed.error ? parsed.error->message : "unknown error"),
                false};
    }
    return session.execute(*parsed.command);
}

bool test_sql_crud_and_restart() {
    TemporaryDirectory temporary;
    const auto data_path = temporary.path() / "data";
    minidb::execution::SqlSession session(data_path);
    if (!expect(session.initialize().success,
                "The SQL session should initialize its data directory.") ||
        !expect(execute(session, "CREATE DATABASE school").output.find(
                    "Database created") != std::string::npos,
                "CREATE DATABASE should execute through the SQL session.") ||
        !expect(execute(session, "USE school").output.find(
                    "Using database") != std::string::npos,
                "USE should select the database for subsequent commands.") ||
        !expect(execute(session,
                        "CREATE TABLE student (id INT, name TEXT, score "
                        "DOUBLE, active BOOLEAN)")
                    .output.find("Table created") != std::string::npos,
                "CREATE TABLE should persist the table schema.") ||
        !expect(execute(session,
                        "INSERT INTO student VALUES (1, 'Ada', 98.5, TRUE)")
                    .output.find("1 row inserted") != std::string::npos,
                "INSERT should append a typed record.") ||
        !expect(execute(session,
                        "INSERT INTO student VALUES (+2, 'Lin', +85.5, FALSE)")
                    .output.find("1 row inserted") != std::string::npos,
                "INSERT should support multiple records.")) {
        return false;
    }

    const auto selected =
        execute(session, "SELECT name, score FROM student WHERE score >= 90");
    if (!expect(selected.output.find("Ada") != std::string::npos &&
                    selected.output.find("98.5") != std::string::npos &&
                    selected.output.find("Lin") == std::string::npos &&
                    selected.output.find("1 row") != std::string::npos,
                "SELECT should filter rows and project requested columns.") ||
        !expect(execute(session,
                        "UPDATE student SET active = TRUE WHERE id = 2 AND "
                        "(score >= 80 OR name = 'other')")
                    .output.find("1 row updated") != std::string::npos,
                "UPDATE should apply a boolean condition.") ||
        !expect(execute(session,
                        "DELETE FROM student WHERE id = 1 OR id = 99")
                    .output.find("1 row deleted") != std::string::npos,
                "DELETE should remove only matching records.")) {
        return false;
    }

    const auto invalid_type =
        execute(session, "INSERT INTO student VALUES ('bad', 'X', 1, TRUE)");
    const auto invalid_column =
        execute(session, "SELECT missing FROM student");
    if (!expect(invalid_type.output.find("not valid for INT") !=
                    std::string::npos,
                "Record type mismatches should be reported without writing.") ||
        !expect(invalid_column.output.find("Unknown selected column") !=
                    std::string::npos,
                "Unknown columns should be reported clearly.")) {
        return false;
    }

    minidb::execution::SqlSession restarted(data_path);
    if (!expect(restarted.initialize().success &&
                    execute(restarted, "USE school").output.find(
                    "Using database") != std::string::npos,
                "A fresh session should be able to select persisted data.")) {
        return false;
    }
    const auto persisted = execute(restarted, "SELECT * FROM student");
    return expect(persisted.output.find("Lin") != std::string::npos &&
                      persisted.output.find("85.5") != std::string::npos &&
                      persisted.output.find("TRUE") != std::string::npos &&
                      persisted.output.find("Ada") == std::string::npos &&
                      persisted.output.find("1 row") != std::string::npos,
                  "Updated and deleted records should persist across sessions.");
}

bool test_float_nulls_and_aligned_output() {
    TemporaryDirectory temporary;
    minidb::execution::SqlSession session(temporary.path() / "data");
    if (!expect(session.initialize().success &&
                    execute(session, "CREATE DATABASE values_db").output.find(
                        "Database created") != std::string::npos &&
                    execute(session, "USE values_db").output.find(
                        "Using database") != std::string::npos &&
                    execute(session,
                            "CREATE TABLE measurements (f FLOAT, d DOUBLE, "
                            "note TEXT)")
                            .output.find("Table created") != std::string::npos &&
                    execute(session,
                            "INSERT INTO measurements VALUES (+0.1, +2.5, "
                            "NULL)")
                            .output.find("1 row inserted") !=
                        std::string::npos &&
                    execute(session,
                            "INSERT INTO measurements VALUES (10.25, 30.75, "
                            "'longer')")
                            .output.find("1 row inserted") !=
                        std::string::npos &&
                    execute(session,
                            "INSERT INTO measurements VALUES (1.25, 4.5, "
                            "'C:\\')")
                            .output.find("1 row inserted") !=
                        std::string::npos,
                "FLOAT, DOUBLE, and NULL values should insert successfully.")) {
        return false;
    }

    const auto all = execute(session, "SELECT * FROM measurements");
    const auto nulls =
        execute(session, "SELECT note FROM measurements WHERE note IS NULL");
    const auto not_null =
        execute(session, "SELECT note FROM measurements WHERE note IS NOT NULL");
    const auto delete_null =
        execute(session, "DELETE FROM measurements WHERE note IS NULL");
    const auto after_delete = execute(session, "SELECT * FROM measurements");
    if (!expect(all.output.find("0.1") != std::string::npos &&
                      all.output.find("0.100000001") == std::string::npos &&
                      all.output.find("2.5") != std::string::npos &&
                      all.output.find("30.75") != std::string::npos &&
                      all.output.find("10.25") != std::string::npos &&
                      all.output.find("C:\\") != std::string::npos,
                  "Floating-point results should use concise values.") &&
           expect(all.output.find("10.25 | 30.75 | longer") !=
                      std::string::npos,
                  "Result columns should align to the widest cell.") &&
           expect(nulls.output.find("NULL") != std::string::npos &&
                      nulls.output.find("1 row") != std::string::npos,
                  "IS NULL should match NULL records.") &&
           expect(not_null.output.find("longer") != std::string::npos &&
                      not_null.output.find("C:\\") != std::string::npos &&
                      not_null.output.find("2 rows") != std::string::npos,
                  "IS NOT NULL should match non-NULL records.") &&
           expect(delete_null.output.find("1 row deleted") != std::string::npos &&
                      after_delete.output.find("NULL") == std::string::npos &&
                      after_delete.output.find("2 rows") != std::string::npos,
                  "DELETE should remove rows matched by IS NULL.")) {
        return false;
    }

    if (!expect(execute(session, "CREATE TABLE legacy (f FLOAT, d DOUBLE)")
                    .output.find("Table created") != std::string::npos,
                "The legacy-format table should be created.")) {
        return false;
    }
    const auto legacy_file =
        temporary.path() / "data" / "values_db" / "tables" / "legacy" /
        "records.dat";
    {
        std::ofstream output(legacy_file, std::ios::binary | std::ios::trunc);
        const std::string legacy_float = "0.10000000000000001";
        const std::string legacy_double = "2.5";
        output << "MINIDB_RECORDS\n1\n1\nROW 2\n"
               << "F " << legacy_float.size() << '\n'
               << legacy_float << '\n'
               << "F " << legacy_double.size() << '\n'
               << legacy_double << '\n';
    }
    const auto legacy_result = execute(session, "SELECT * FROM legacy");
    return expect(legacy_result.output.find("0.1 | 2.5") !=
                      std::string::npos,
                  "Version 1 record data should remain readable with FLOAT "
                  "values converted to their declared type.");
}

bool test_corrupt_record_bounds() {
    TemporaryDirectory temporary;
    const auto data_path = temporary.path() / "data";
    minidb::execution::SqlSession session(data_path);
    if (!expect(session.initialize().success &&
                    execute(session, "CREATE DATABASE corrupt").output.find(
                        "Database created") != std::string::npos &&
                    execute(session, "USE corrupt").output.find("Using database") !=
                        std::string::npos &&
                    execute(session, "CREATE TABLE t (value TEXT)")
                            .output.find("Table created") != std::string::npos,
                "The corrupt-record test table should be created.")) {
        return false;
    }
    const auto records =
        data_path / "corrupt" / "tables" / "t" / "records.dat";
    const auto empty_table = execute(session, "SELECT * FROM t");
    {
        std::ofstream output(records, std::ios::binary | std::ios::trunc);
        output << "MINIDB_RECORDS\n2\n18446744073709551615\n";
    }
    const auto huge_count = execute(session, "SELECT * FROM t");
    {
        std::ofstream output(records, std::ios::binary | std::ios::trunc);
        output << "MINIDB_RECORDS\n2\n1\nROW 1\nS 18446744073709551615\n";
    }
    const auto huge_value = execute(session, "SELECT * FROM t");
    return expect(empty_table.output.find("0 rows") != std::string::npos,
                  "An empty table should be readable before corruption.") &&
           expect(huge_count.output.find("invalid or impossible row count") !=
                      std::string::npos,
                  "A huge row count should return an error instead of "
                  "allocating uncontrollably.") &&
           expect(huge_value.output.find("invalid or incomplete value") !=
                      std::string::npos,
                  "A huge value length should return an error instead of "
                  "allocating uncontrollably.");
}

bool test_table_and_database_commands() {
    TemporaryDirectory temporary;
    minidb::execution::SqlSession session(temporary.path() / "data");
    if (!expect(session.initialize().success, "Session initialization failed.") ||
        !expect(execute(session, "SHOW DATABASES").output.find("(none)") !=
                    std::string::npos,
                "SHOW DATABASES should report an empty catalog.") ||
        !expect(execute(session, "CREATE DATABASE archive").output.find(
                    "Database created") != std::string::npos,
                "A second database should be creatable.") ||
        !expect(execute(session, "USE archive").output.find(
                    "Using database") != std::string::npos,
                "A second database should be selectable.") ||
        !expect(execute(session, "CREATE TABLE item (id INT, label VARCHAR)")
                    .output.find("Table created") != std::string::npos,
                "Tables should be creatable through SQL.") ||
        !expect(execute(session, "SHOW TABLES").output.find("item") !=
                    std::string::npos,
                "SHOW TABLES should list created tables.") ||
        !expect(execute(session, "DESCRIBE TABLE item").output.find("label VARCHAR") !=
                    std::string::npos,
                "DESCRIBE TABLE should display its schema.") ||
        !expect(execute(session, "DROP TABLE item").output.find(
                    "Table dropped") != std::string::npos,
                "DROP TABLE should remove the schema and its records.") ||
        !expect(execute(session, "DROP DATABASE archive").output.find(
                    "Database dropped") != std::string::npos,
                "DROP DATABASE should work through SQL.")) {
        return false;
    }
    return true;
}

}  // namespace

int main() {
    return test_sql_crud_and_restart() &&
                   test_table_and_database_commands() &&
                   test_float_nulls_and_aligned_output() &&
                   test_corrupt_record_bounds()
               ? 0
               : 1;
}
