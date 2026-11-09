#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minidb::storage {

struct DatabaseOperationResult {
    bool success = false;
    std::string message;
};

struct DatabaseListResult {
    bool success = false;
    std::vector<std::string> databases;
    std::string message;
};

class DatabaseManager {
public:
    explicit DatabaseManager(std::filesystem::path data_directory);

    DatabaseOperationResult initialize();
    DatabaseOperationResult create_database(std::string_view name);
    DatabaseOperationResult drop_database(std::string_view name);
    DatabaseListResult show_databases() const;
    DatabaseOperationResult use_database(std::string_view name);

    const std::optional<std::string>& selected_database() const noexcept;
    std::optional<std::filesystem::path> selected_database_path() const;

private:
    std::filesystem::path data_directory_;
    std::optional<std::string> selected_database_;
    bool initialized_ = false;
};

}  // namespace minidb::storage
