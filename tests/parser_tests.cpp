#include "minidb/parser/statement.hpp"

#include <iostream>
#include <string_view>
#include <variant>

namespace {

bool expect_statement(std::string_view input, bool expect_help) {
    const auto result = minidb::parser::parse_command(input);
    if (!result.statement || result.diagnostic) {
        return false;
    }
    return expect_help
               ? std::holds_alternative<minidb::parser::HelpStatement>(
                     *result.statement)
               : std::holds_alternative<minidb::parser::ExitStatement>(
                     *result.statement);
}

}  // namespace

int main() {
    if (!expect_statement(" help ; ", true) ||
        !expect_statement("EXIT", false) ||
        !expect_statement("quit;", false)) {
        std::cerr << "A supported shell command was not parsed correctly.\n";
        return 1;
    }

    const auto blank = minidb::parser::parse_command(" \t ");
    if (blank.statement || blank.diagnostic) {
        std::cerr << "Blank input should be ignored.\n";
        return 1;
    }

    const auto unsupported = minidb::parser::parse_command(
        "SELECT * FROM students;");
    if (!unsupported.diagnostic ||
        unsupported.diagnostic->message !=
            "Unknown command. Enter HELP for available commands.") {
        std::cerr << "Unknown commands should produce a clear diagnostic.\n";
        return 1;
    }

    return 0;
}
