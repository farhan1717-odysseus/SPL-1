#include "minidb/storage/database_directory.hpp"

#include <filesystem>

namespace minidb::storage {

std::optional<utils::Diagnostic> prepare_database_directory(
    const std::string& directory) {
    if (directory.empty()) {
        return utils::Diagnostic{"The data directory path cannot be empty."};
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return utils::Diagnostic{"Cannot create data directory '" + directory +
                                 "': " + error.message()};
    }

    if (!std::filesystem::is_directory(directory, error) || error) {
        const std::string detail =
            error ? error.message() : "path is not a directory";
        return utils::Diagnostic{"Invalid data directory '" + directory +
                                 "': " + detail};
    }

    return std::nullopt;
}

}  // namespace minidb::storage
