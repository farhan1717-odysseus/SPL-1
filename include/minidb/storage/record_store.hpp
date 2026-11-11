#pragma once

#include "minidb/storage/database_manager.hpp"
#include "minidb/storage/table_manager.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace minidb::storage {

using RecordValue =
    std::variant<std::monostate, std::int64_t, float, double, std::string,
                 bool>;
using Record = std::vector<RecordValue>;

struct RecordsResult {
    bool success = false;
    std::vector<Record> rows;
    std::string message;
};

struct RecordOperationResult {
    bool success = false;
    std::string message;
};

class RecordStore {
public:
    explicit RecordStore(const DatabaseManager& database_manager);

    RecordsResult read(std::string_view table) const;
    RecordOperationResult write(std::string_view table,
                                const std::vector<Record>& rows) const;

private:
    const DatabaseManager& database_manager_;
    TableManager table_manager_;
};

}  // namespace minidb::storage
