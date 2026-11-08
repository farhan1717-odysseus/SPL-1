#include "minidb/core/cli.hpp"

#include <iostream>
#include <string>

#ifndef MINIDB_VERSION
#define MINIDB_VERSION "0.1.0"
#endif

namespace minidb::core {

int run_cli(std::istream& input, std::ostream& output, std::ostream& error,
            CommandHandler handle_command) {
    output << "Mini Database Engine v" << MINIDB_VERSION << '\n'
           << "Type HELP for help or EXIT to quit.\n";

    std::string line;
    while (true) {
        output << "minidb> " << std::flush;
        if (!std::getline(input, line)) {
            break;
        }

        if (line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        if (!handle_command(line, output, error)) {
            output << "Goodbye.\n";
            break;
        }
    }

    return 0;
}

}  // namespace minidb::core
