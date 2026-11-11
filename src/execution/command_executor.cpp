#include "minidb/execution/command_executor.hpp"

namespace minidb::execution {

CommandResult execute_command(const parser::Statement& statement) {
    if (std::holds_alternative<parser::HelpStatement>(statement)) {
        return {
            "Mini Database Engine command shell\n"
            "  HELP       Show this help message\n"
            "  EXIT, QUIT End the session\n"
            "\n"
            "SQL commands:\n"
            "  CREATE/DROP DATABASE, SHOW DATABASES, USE\n"
            "  CREATE/DROP TABLE, SHOW TABLES, DESCRIBE TABLE\n"
            "  INSERT INTO, SELECT, UPDATE, DELETE\n"
            "Data is stored in the ./data directory.\n",
            false};
    }

    return {"", true};
}

}  // namespace minidb::execution
