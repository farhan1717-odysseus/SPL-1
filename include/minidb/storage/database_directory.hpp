#pragma once

#include "minidb/utils/diagnostic.hpp"

#include <optional>
#include <string>

namespace minidb::storage {

std::optional<utils::Diagnostic> prepare_database_directory(
    const std::string& directory);

}  // namespace minidb::storage
