#include "minidb/parser/statement.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace minidb::parser {
namespace {

std::string normalize_command(std::string_view input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }

    const auto last = input.find_last_not_of(" \t\r\n");
    input = input.substr(first, last - first + 1);
    if (!input.empty() && input.back() == ';') {
        input.remove_suffix(1);
        const auto command_end = input.find_last_not_of(" \t\r\n");
        if (command_end == std::string_view::npos) {
            return {};
        }
        input = input.substr(0, command_end + 1);
    }

    std::string normalized(input);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    return normalized;
}

}  // namespace

ParseResult parse_command(std::string_view input) {
    const std::string command = normalize_command(input);
    if (command.empty()) {
        return {};
    }
    if (command == "HELP") {
        return {Statement{HelpStatement{}}, std::nullopt};
    }
    if (command == "EXIT" || command == "QUIT") {
        return {Statement{ExitStatement{}}, std::nullopt};
    }

    const std::string message =
        command.rfind("SELECT ", 0) == 0 || command.rfind("CREATE ", 0) == 0 ||
                command.rfind("INSERT ", 0) == 0 ||
                command.rfind("UPDATE ", 0) == 0 ||
                command.rfind("DELETE ", 0) == 0 ||
                command.rfind("DROP ", 0) == 0
            ? "SQL statements are not implemented yet."
            : "Unknown command. Enter HELP for available commands.";
    return {std::nullopt, utils::Diagnostic{message}};
}

}  // namespace minidb::parser
