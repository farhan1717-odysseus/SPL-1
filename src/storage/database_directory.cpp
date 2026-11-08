#include "minidb/storage/database_directory.hpp"

#include <cerrno>
#include <cctype>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

namespace minidb::storage {
namespace {

bool is_directory(const std::string& path) {
#ifdef _WIN32
    struct _stat information {};
    return _stat(path.c_str(), &information) == 0 &&
           (information.st_mode & _S_IFDIR) != 0;
#else
    struct stat information {};
    return stat(path.c_str(), &information) == 0 &&
           S_ISDIR(information.st_mode);
#endif
}

bool create_directory(const std::string& path, std::string& detail) {
#ifdef _WIN32
    const int result = _mkdir(path.c_str());
#else
    const int result = mkdir(path.c_str(), 0777);
#endif
    const int creation_error = errno;
    if (result == 0 || is_directory(path)) {
        return true;
    }

    detail = std::strerror(creation_error);
    return false;
}

bool is_separator(char character) {
    return character == '/' || character == '\\';
}

std::optional<utils::Diagnostic> create_directories(
    const std::string& directory) {
    if (directory.empty()) {
        return utils::Diagnostic{"The data directory path cannot be empty."};
    }

    std::string current;
    std::size_t position = 0;
    if (directory.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(directory[0])) &&
        directory[1] == ':') {
        current = directory.substr(0, 2);
        position = 2;
        if (position < directory.size() && is_separator(directory[position])) {
            current += directory[position++];
        }
    } else if (is_separator(directory[0])) {
        current += directory[position++];
    }

    while (position < directory.size()) {
        while (position < directory.size() && is_separator(directory[position])) {
            ++position;
        }
        const std::size_t end = directory.find_first_of("/\\", position);
        const std::size_t length =
            end == std::string::npos ? std::string::npos : end - position;
        const std::string segment = directory.substr(position, length);
        if (!segment.empty()) {
            if (!current.empty() && !is_separator(current.back())) {
                current += '/';
            }
            current += segment;
            std::string detail;
            if (!create_directory(current, detail)) {
                return utils::Diagnostic{"Cannot create data directory '" +
                                         current + "': " + detail};
            }
        }
        if (end == std::string::npos) {
            break;
        }
        position = end;
    }

    return std::nullopt;
}

}  // namespace

std::optional<utils::Diagnostic> prepare_database_directory(
    const std::string& directory) {
    return create_directories(directory);
}

}  // namespace minidb::storage
