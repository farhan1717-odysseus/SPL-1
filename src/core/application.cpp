#include "minidb/core/application.hpp"

#include "minidb/core/cli.hpp"
#include "minidb/execution/command_executor.hpp"
#include "minidb/parser/statement.hpp"
#include "minidb/utils/diagnostic.hpp"

#include <ostream>
#include <string>
#include <string_view>

#ifndef MINIDB_VERSION
#define MINIDB_VERSION "0.1.0"
#endif

namespace minidb::core {
namespace {

void print_usage(std::ostream& output) {
    output << "Usage: minidb [--help] [--version]\n";
}

bool handle_command(std::string_view command, std::ostream& output,
                    std::ostream& error) {
    const parser::ParseResult parsed = parser::parse_command(command);
    if (parsed.diagnostic) {
        utils::write_diagnostic(error, *parsed.diagnostic);
        return true;
    }
    if (!parsed.statement) {
        return true;
    }

    const execution::CommandResult result =
        execution::execute_command(*parsed.statement);
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

    return run_cli(input, output, error, handle_command);
}

}  // namespace minidb::core
