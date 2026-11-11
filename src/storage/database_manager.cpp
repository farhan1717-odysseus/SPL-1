#include "minidb/storage/database_manager.hpp"
#include "minidb/storage/database_directory.hpp"
#include "storage_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <set>
#include <system_error>
#include <utility>

namespace minidb::storage
{
    namespace
    {

        using detail::is_identifier;
        using detail::normalized_name;

        constexpr const char *metadata_file_name = "database.meta";
        constexpr const char *metadata_signature = "MINIDB_DATABASE";
        constexpr const char *metadata_version = "1";

        struct MetadataReadResult
        {
            bool success = false;
            std::string name;
            std::string message;
        };

        MetadataReadResult read_metadata(const std::filesystem::path &directory)
        {
            const std::filesystem::path metadata_path =
                directory / metadata_file_name;
            std::error_code error;
            const auto status = std::filesystem::symlink_status(metadata_path, error);
            if (error)
            {
                return {false, {}, "Cannot inspect database metadata: " + error.message()};
            }
            if (status.type() == std::filesystem::file_type::not_found)
            {
                return {false, {}, "Database metadata is missing."};
            }
            if (std::filesystem::is_symlink(status) ||
                !std::filesystem::is_regular_file(status))
            {
                return {false, {}, "Database metadata must be a regular, non-symlink file."};
            }

            std::ifstream input(metadata_path);
            if (!input)
            {
                return {false, {}, "Cannot open database metadata."};
            }

            std::string signature;
            std::string version;
            std::string name;
            std::string unexpected;
            if (!std::getline(input, signature) || !std::getline(input, version) ||
                !std::getline(input, name) || std::getline(input, unexpected))
            {
                return {false, {}, "Database metadata has an invalid format."};
            }
            if (signature != metadata_signature || version != metadata_version ||
                !is_identifier(name))
            {
                return {false, {}, "Database metadata has an unsupported or invalid "
                                   "format."};
            }
            if (normalized_name(name) !=
                normalized_name(directory.filename().string()))
            {
                return {false, {}, "Database metadata name does not match its "
                                   "directory."};
            }

            return {true, std::move(name), {}};
        }

        DatabaseListResult list_databases(const std::filesystem::path &root)
        {
            DatabaseListResult result;
            const auto failure = [](std::string message)
            {
                return DatabaseListResult{false, {}, std::move(message)};
            };
            std::error_code error;
            std::filesystem::directory_iterator entry(root, error);
            if (error)
            {
                return failure("Cannot list databases: " + error.message());
            }

            const std::filesystem::directory_iterator end;
            std::set<std::string> database_names;
            while (entry != end)
            {
                const auto path = entry->path();
                const bool symbolic_link = entry->is_symlink(error);
                if (error)
                {
                    return failure("Cannot inspect data directory entry: " +
                                   error.message());
                }

                if (!symbolic_link)
                {
                    const bool is_directory = entry->is_directory(error);
                    if (error)
                    {
                        return failure("Cannot inspect data directory entry: " +
                                       error.message());
                    }

                    if (is_directory && is_identifier(path.filename().string()))
                    {
                        const std::filesystem::path metadata_path =
                            path / metadata_file_name;
                        const auto metadata_status =
                            std::filesystem::symlink_status(metadata_path, error);
                        if (error == std::errc::no_such_file_or_directory)
                        {
                            error.clear();
                        }
                        if (error)
                        {
                            return failure("Cannot inspect database metadata: " +
                                           error.message());
                        }

                        if (metadata_status.type() !=
                            std::filesystem::file_type::not_found)
                        {
                            const MetadataReadResult metadata = read_metadata(path);
                            if (metadata.success &&
                                !database_names
                                     .insert(normalized_name(metadata.name))
                                     .second)
                            {
                                return failure("Duplicate database name found: " +
                                               metadata.name);
                            }
                            if (metadata.success)
                            {
                                result.databases.push_back(metadata.name);
                            }
                        }
                    }
                }

                entry.increment(error);
                if (error)
                {
                    return failure("Cannot continue listing databases: " +
                                   error.message());
                }
            }

            std::sort(result.databases.begin(), result.databases.end(),
                      [](const std::string &left, const std::string &right)
                      {
                          return normalized_name(left) < normalized_name(right);
                      });
            result.success = true;
            return result;
        }

        std::optional<std::filesystem::path> find_database_directory(
            const std::filesystem::path &root, std::string_view name,
            std::string &message)
        {
            std::error_code error;
            std::filesystem::directory_iterator entry(root, error);
            if (error)
            {
                message = "Cannot search databases: " + error.message();
                return std::nullopt;
            }

            const auto wanted = normalized_name(name);
            const std::filesystem::directory_iterator end;
            while (entry != end)
            {
                const auto path = entry->path();
                const bool symbolic_link = entry->is_symlink(error);
                if (error)
                {
                    message = "Cannot inspect data directory entry: " +
                              error.message();
                    return std::nullopt;
                }
                if (!symbolic_link &&
                    normalized_name(path.filename().string()) == wanted)
                {
                    const bool is_directory = entry->is_directory(error);
                    if (error)
                    {
                        message = "Cannot inspect database directory: " +
                                  error.message();
                        return std::nullopt;
                    }
                    if (is_directory)
                    {
                        return path;
                    }
                }
                entry.increment(error);
                if (error)
                {
                    message = "Cannot continue searching databases: " +
                              error.message();
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }

        DatabaseOperationResult require_initialized(bool initialized)
        {
            if (!initialized)
            {
                return {false, "Database manager has not been initialized."};
            }
            return {true, {}};
        }

    } // namespace

    DatabaseManager::DatabaseManager(std::filesystem::path data_directory)
        : data_directory_(std::move(data_directory)) {}

    DatabaseOperationResult DatabaseManager::initialize()
    {
        if (data_directory_.empty())
        {
            return {false, "The data directory path cannot be empty."};
        }

        if (const auto diagnostic =
                prepare_database_directory(data_directory_.string()))
        {
            return {false, diagnostic->message};
        }

        std::error_code error;
        data_directory_ = std::filesystem::absolute(data_directory_, error);
        if (error)
        {
            return {false, "Cannot resolve data directory: " + error.message()};
        }

        data_directory_ = data_directory_.lexically_normal();
        initialized_ = true;
        return {true, {}};
    }

    DatabaseOperationResult DatabaseManager::create_database(
        std::string_view name)
    {
        const auto ready = require_initialized(initialized_);
        if (!ready.success)
        {
            return ready;
        }
        if (!is_identifier(name))
        {
            return {false, "Invalid database name. Use a letter or underscore "
                           "first, followed by letters, digits, or underscores."};
        }

        const DatabaseListResult current = list_databases(data_directory_);
        if (!current.success)
        {
            return {false, current.message};
        }
        const std::string normalized = normalized_name(name);
        if (std::any_of(current.databases.begin(), current.databases.end(),
                        [&](const std::string &existing)
                        {
                            return normalized_name(existing) == normalized;
                        }))
        {
            return {false, "Database already exists: " + std::string(name)};
        }

        const std::filesystem::path database_path =
            data_directory_ / std::string(name);
        std::error_code error;
        const bool created = std::filesystem::create_directory(database_path, error);
        if (error || !created)
        {
            const std::string detail =
                error ? error.message() : "the path already exists";
            return {false, "Cannot create database '" + std::string(name) +
                               "': " + detail};
        }

        const std::filesystem::path temporary_metadata =
            database_path / (std::string(metadata_file_name) + ".tmp");
        const auto remove_new_database = [&database_path]()
        {
            std::error_code cleanup_error;
            std::filesystem::remove_all(database_path, cleanup_error);
            return cleanup_error;
        };
        {
            std::ofstream output(temporary_metadata, std::ios::binary);
            if (!output)
            {
                const std::error_code cleanup_error = remove_new_database();
                const std::string cleanup_detail =
                    cleanup_error ? " Cleanup failed: " + cleanup_error.message()
                                  : "";
                return {false, "Cannot create database metadata for '" +
                                   std::string(name) + "'." + cleanup_detail};
            }
            output << metadata_signature << '\n'
                   << metadata_version << '\n'
                   << name << '\n';
            output.flush();
            if (!output)
            {
                output.close();
                const std::error_code cleanup_error = remove_new_database();
                const std::string cleanup_detail =
                    cleanup_error ? " Cleanup failed: " + cleanup_error.message()
                                  : "";
                return {false, "Cannot write database metadata for '" +
                                   std::string(name) + "'." + cleanup_detail};
            }
            output.close();
            if (!output)
            {
                const std::error_code cleanup_error = remove_new_database();
                const std::string cleanup_detail =
                    cleanup_error ? " Cleanup failed: " + cleanup_error.message()
                                  : "";
                return {false, "Cannot finalize database metadata for '" +
                                   std::string(name) + "'." + cleanup_detail};
            }
        }

        std::filesystem::rename(temporary_metadata,
                                database_path / metadata_file_name, error);
        if (error)
        {
            const std::string rename_detail = error.message();
            const std::error_code cleanup_error = remove_new_database();
            const std::string cleanup_detail =
                cleanup_error ? " Cleanup failed: " + cleanup_error.message() : "";
            return {false, "Cannot finalize database metadata for '" +
                               std::string(name) + "': " + rename_detail +
                               cleanup_detail};
        }

        return {true, "Database created: " + std::string(name)};
    }

    DatabaseOperationResult DatabaseManager::drop_database(std::string_view name)
    {
        const auto ready = require_initialized(initialized_);
        if (!ready.success)
        {
            return ready;
        }
        if (!is_identifier(name))
        {
            return {false, "Invalid database name."};
        }

        const std::string normalized = normalized_name(name);
        const auto current = list_databases(data_directory_);
        std::filesystem::path database_path;
        if (current.success)
        {
            const auto found = std::find_if(
                current.databases.begin(), current.databases.end(),
                [&](const std::string &existing)
                {
                    return normalized_name(existing) == normalized;
                });
            if (found != current.databases.end())
            {
                database_path = data_directory_ / *found;
            }
        }
        else
        {
            return {false, current.message};
        }
        if (database_path.empty())
        {
            std::string search_error;
            const auto found =
                find_database_directory(data_directory_, name, search_error);
            if (!search_error.empty())
            {
                return {false, std::move(search_error)};
            }
            if (!found)
            {
                return {false, "Database does not exist: " + std::string(name)};
            }
            database_path = *found;
        }

        const std::string database_name = database_path.filename().string();
        std::error_code error;
        const std::uintmax_t removed =
            std::filesystem::remove_all(database_path, error);
        if (error || removed == 0)
        {
            const std::string detail =
                error ? error.message() : "database directory was not removed";
            return {false, "Cannot drop database '" + database_name + "': " + detail};
        }

        if (selected_database_ &&
            normalized_name(*selected_database_) == normalized)
        {
            selected_database_.reset();
        }
        return {true, "Database dropped: " + database_name};
    }

    DatabaseListResult DatabaseManager::show_databases() const
    {
        const auto ready = require_initialized(initialized_);
        if (!ready.success)
        {
            return {false, {}, ready.message};
        }
        return list_databases(data_directory_);
    }

    DatabaseOperationResult DatabaseManager::use_database(std::string_view name)
    {
        const auto ready = require_initialized(initialized_);
        if (!ready.success)
        {
            return ready;
        }
        if (!is_identifier(name))
        {
            return {false, "Invalid database name."};
        }

        const DatabaseListResult current = list_databases(data_directory_);
        if (!current.success)
        {
            return {false, current.message};
        }
        const std::string normalized = normalized_name(name);
        const auto found = std::find_if(
            current.databases.begin(), current.databases.end(),
            [&](const std::string &existing)
            {
                return normalized_name(existing) == normalized;
            });
        if (found == current.databases.end())
        {
            return {false, "Database does not exist: " + std::string(name)};
        }

        selected_database_ = *found;
        return {true, "Using database: " + *found};
    }

    const std::optional<std::string> &
    DatabaseManager::selected_database() const noexcept
    {
        return selected_database_;
    }

    std::optional<std::filesystem::path>
    DatabaseManager::selected_database_path() const
    {
        if (!initialized_ || !selected_database_)
        {
            return std::nullopt;
        }
        return data_directory_ / *selected_database_;
    }

} // namespace minidb::storage
