#include "minidb/execution/command_executor.hpp"

namespace minidb::execution {

CommandResult execute_command(const parser::Statement& statement) {
    if (std::holds_alternative<parser::HelpStatement>(statement)) {
        return {
            "Mini Database Engine command shell\n"
            "  HELP       Show this help message\n"
            "  EXIT, QUIT End the session\n"
            "\n"
            "SQL commands will be added in later development milestones.\n",
            false};
    }

    return {"", true};
}

}  // namespace minidb::execution
