#pragma once

#include "minidb/parser/statement.hpp"

#include <string>

namespace minidb::execution {

struct CommandResult {
    std::string output;
    bool should_exit = false;
};

CommandResult execute_command(const parser::Statement& statement);

}  // namespace minidb::execution
