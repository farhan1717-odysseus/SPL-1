#include "minidb/storage/database_manager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("minidb_database_manager_tests_" + std::to_string(suffix));
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

bool test_database_lifecycle() {
    TemporaryDirectory temporary;
    minidb::storage::DatabaseManager manager(temporary.path() / "data");
    if (!expect(manager.initialize().success,
                "The manager should initialize its data directory.") ||
        !expect(manager.show_databases().success &&
                    manager.show_databases().databases.empty(),
                "A new data directory should contain no databases.")) {
        return false;
    }

    if (!expect(manager.create_database("school").success,
                "The manager should create a database.") ||
        !expect(!manager.create_database("school").success,
                "Duplicate database names should be rejected.") ||
        !expect(!manager.create_database("School").success,
                "Database names should be unique case-insensitively.") ||
        !expect(!manager.create_database("../outside").success,
                "Path traversal is not a valid database name.") ||
        !expect(!manager.create_database("2nd_database").success,
                "Database names must start with a letter or underscore.") ||
        !expect(!manager.create_database("has space").success,
                "Database names cannot contain spaces.")) {
        return false;
    }

    const auto metadata_path =
        temporary.path() / "data" / "school" / "database.meta";
    std::ifstream metadata(metadata_path, std::ios::binary);
    std::string signature;
    std::string version;
    std::string name;
    if (!expect(static_cast<bool>(metadata),
                "Database metadata should be created.") ||
        !expect(static_cast<bool>(std::getline(metadata, signature)) &&
                    static_cast<bool>(std::getline(metadata, version)) &&
                    static_cast<bool>(std::getline(metadata, name)) &&
                    signature == "MINIDB_DATABASE" && version == "1" &&
                    name == "school",
                "Database metadata should contain signature, version, and name.")) {
        return false;
    }
    metadata.close();

    if (!expect(manager.create_database("_archive").success,
                "Underscore-prefixed database names should be valid.")) {
        return false;
    }
    const auto listed = manager.show_databases();
    if (!expect(listed.success && listed.databases.size() == 2 &&
                    listed.databases[0] == "_archive" &&
                    listed.databases[1] == "school",
                "SHOW DATABASES should return sorted database names.")) {
        return false;
    }

    if (!expect(!manager.use_database("missing").success,
                "USE should reject nonexistent databases.") ||
        !expect(!manager.selected_database().has_value(),
                "A failed USE should not select a database.") ||
        !expect(manager.use_database("SCHOOL").success,
                "USE should match database names case-insensitively.") ||
        !expect(manager.selected_database() == "school",
                "The selected database should preserve its stored name.")) {
        return false;
    }

    const auto selected_path = manager.selected_database_path();
    if (!expect(selected_path.has_value() &&
                    *selected_path ==
                        std::filesystem::absolute(temporary.path() / "data" /
                                                  "school"),
                "The selected database path should be available.")) {
        return false;
    }

    minidb::storage::DatabaseManager restarted(temporary.path() / "data");
    if (!expect(restarted.initialize().success,
                "A new manager should initialize against existing data.") ||
        !expect(restarted.show_databases().success &&
                    restarted.show_databases().databases.size() == 2,
                "Databases should remain available after manager restart.") ||
        !expect(restarted.use_database("school").success,
                "An existing database should remain selectable after restart.")) {
        return false;
    }

    if (!expect(!manager.drop_database("missing").success,
                "DROP should reject nonexistent databases.") ||
        !expect(manager.drop_database("SCHOOL").success,
                "DROP should match database names case-insensitively.") ||
        !expect(!manager.selected_database().has_value(),
                "Dropping the selected database should clear selection.") ||
        !expect(!std::filesystem::exists(metadata_path),
                "DROP should remove the database directory and metadata.") ||
        !expect(manager.show_databases().databases.size() == 1,
                "SHOW DATABASES should reflect a dropped database.")) {
        return false;
    }

    return true;
}

bool test_uninitialized_and_corrupt_metadata() {
    TemporaryDirectory temporary;
    minidb::storage::DatabaseManager manager(temporary.path() / "data");
    if (!expect(!manager.create_database("school").success,
                "Operations before initialization should be rejected.") ||
        !expect(!manager.show_databases().success,
                "Listing before initialization should be rejected.")) {
        return false;
    }

    if (!expect(manager.initialize().success &&
                    manager.create_database("_valid").success &&
                    manager.create_database("school").success,
                "The test database should be created.")) {
        return false;
    }

    std::ofstream metadata(temporary.path() / "data" / "school" /
                               "database.meta",
                           std::ios::binary | std::ios::trunc);
    metadata << "not valid metadata\n";
    metadata.close();

    const auto listed = manager.show_databases();
    return expect(listed.success && listed.databases.size() == 1 &&
                      listed.databases.front() == "_valid",
                  "Corrupt metadata should not block listing valid databases.") &&
           expect(!manager.use_database("school").success,
                  "A database with corrupt metadata must not be selected.") &&
           expect(manager.create_database("after_corruption").success,
                  "Corrupt metadata should not block creating another database.") &&
           expect(manager.drop_database("school").success &&
                      !std::filesystem::exists(temporary.path() / "data" /
                                               "school"),
                  "An explicitly named database with corrupt metadata should "
                  "be removable.") &&
           expect(!manager.create_database("CON").success &&
                      !manager.create_database("nul").success &&
                      !manager.create_database("COM1").success,
                  "Windows reserved device names should not be database names.");
}

}  // namespace

int main() {
    return test_database_lifecycle() &&
                   test_uninitialized_and_corrupt_metadata()
               ? 0
               : 1;
}
