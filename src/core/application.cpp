#include "minidb/core/application.hpp"

#include "minidb/core/cli.hpp"
#include "minidb/execution/command_executor.hpp"
#include "minidb/execution/sql_session.hpp"
#include "minidb/parser/sql_parser.hpp"
#include "minidb/parser/statement.hpp"
#include "minidb/utils/diagnostic.hpp"

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>

#ifndef MINIDB_VERSION
#define MINIDB_VERSION "0.1.0"
#endif

namespace minidb::core {
namespace {

bool starts_sql_statement(std::string_view command) {
    std::size_t offset = command.find_first_not_of(" \t\r\n");
    if (offset == std::string_view::npos) {
        return false;
    }
    const std::size_t word_start = offset;
    while (offset < command.size()) {
        const char character = command[offset];
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') || character == '_')) {
            break;
        }
        ++offset;
    }
    const std::string_view first_word =
        command.substr(word_start, offset - word_start);
    constexpr std::string_view sql_words[] = {
        "CREATE", "DROP", "SHOW", "DESCRIBE", "USE",
        "INSERT", "SELECT", "UPDATE", "DELETE"};
    for (const auto word : sql_words) {
        if (first_word.size() != word.size()) {
            continue;
        }
        bool matches = true;
        for (std::size_t index = 0; index < word.size(); ++index) {
            char character = first_word[index];
            if (character >= 'a' && character <= 'z') {
                character = static_cast<char>(character - ('a' - 'A'));
            }
            if (character != word[index]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

void print_usage(std::ostream& output) {
    output << "Usage: minidb [--help] [--version]\n";
}

bool handle_command(std::string_view command, std::ostream& output,
                    std::ostream& error,
                    execution::SqlSession& session) {
    const parser::ParseResult parsed = parser::parse_command(command);
    if (parsed.statement) {
        const execution::CommandResult result =
            execution::execute_command(*parsed.statement);
        output << result.output;
        return !result.should_exit;
    }

    const parser::SqlParseResult sql = parser::parse_sql(command);
    if (sql.error) {
        if (!starts_sql_statement(command) && parsed.diagnostic) {
            utils::write_diagnostic(error, *parsed.diagnostic);
            return true;
        }
        utils::write_diagnostic(
            error, utils::Diagnostic{sql.error->message + " (line " +
                                     std::to_string(sql.error->line) +
                                     ", column " +
                                     std::to_string(sql.error->column) + ")"});
        return true;
    }
    if (!sql.command) {
        if (parsed.diagnostic) {
            utils::write_diagnostic(error, *parsed.diagnostic);
        }
        return true;
    }
    const execution::CommandResult result = session.execute(*sql.command);
    output << result.output;
    return !result.should_exit;
}

bool parse_options(int argc, char* argv[], bool& show_help, bool& show_version,
                   std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            show_help = true;
        } else if (argument == "--version") {
            show_version = true;
        } else {
            error = "Unknown option: " + argument;
            return false;
        }
    }
    return true;
}

}  // namespace

int run_application(int argc, char* argv[], std::istream& input,
                    std::ostream& output, std::ostream& error) {
    bool show_help = false;
    bool show_version = false;
    std::string option_error;
    if (!parse_options(argc, argv, show_help, show_version, option_error)) {
        utils::write_diagnostic(error, utils::Diagnostic{option_error});
        print_usage(error);
        return 2;
    }

    if (show_help) {
        print_usage(output);
        return 0;
    }
    if (show_version) {
        output << "Mini Database Engine " << MINIDB_VERSION << '\n';
        return 0;
    }

    std::error_code path_error;
    const auto data_directory =
        std::filesystem::current_path(path_error) / "data";
    if (path_error) {
        utils::write_diagnostic(
            error, utils::Diagnostic{"Cannot determine the working directory: " +
                                     path_error.message()});
        return 1;
    }

    execution::SqlSession session(data_directory);
    const auto initialized = session.initialize();
    if (!initialized.success) {
        utils::write_diagnostic(error, utils::Diagnostic{initialized.message});
        return 1;
    }

    return run_cli(input, output, error,
                   [&session](std::string_view command, std::ostream& command_output,
                              std::ostream& command_error) {
                       return handle_command(command, command_output,
                                             command_error, session);
                   });
}

}  // namespace minidb::core
