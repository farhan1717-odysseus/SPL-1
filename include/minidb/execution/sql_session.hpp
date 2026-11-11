#pragma once

#include "minidb/execution/command_executor.hpp"
#include "minidb/parser/sql_parser.hpp"
#include "minidb/storage/database_manager.hpp"
#include "minidb/storage/record_store.hpp"
#include "minidb/storage/table_manager.hpp"

#include <filesystem>

namespace minidb::execution {

class SqlSession {
public:
    explicit SqlSession(std::filesystem::path data_directory);

    storage::DatabaseOperationResult initialize();
    CommandResult execute(const parser::SqlCommand& command);

private:
    storage::DatabaseManager database_manager_;
    storage::TableManager table_manager_;
    storage::RecordStore record_store_;
};

}  // namespace minidb::execution
