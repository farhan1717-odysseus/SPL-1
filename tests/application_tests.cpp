#include "minidb/core/application.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main() {
    char executable[] = "minidb";
    char* arguments[] = {executable};
    std::istringstream input(
        "\n  \nHELP\ninvalid command\nINSERT INTO t VALUES (3, 'abc\n"
        "QUIT\nHELP\n");
    std::ostringstream output;
    std::ostringstream error;

    const int exit_code = minidb::core::run_application(
        1, arguments, input, output, error);
    if (exit_code != 0) {
        std::cerr << "The application did not exit successfully.\n";
        return 1;
    }

    const std::string displayed = output.str();
    const std::size_t help_output =
        displayed.find("Show this help message");
    if (displayed.find("Mini Database Engine v0.1.0") == std::string::npos ||
        displayed.find("minidb> ") == std::string::npos ||
        displayed.find("Mini Database Engine command shell") ==
            std::string::npos ||
        displayed.find("Goodbye.") == std::string::npos ||
        help_output == std::string::npos ||
        displayed.find("Show this help message", help_output + 1) !=
            std::string::npos) {
        std::cerr << "The CLI banner, loop, or exit behavior is incorrect.\n";
        return 1;
    }

    if (error.str() !=
        "Error: Unknown command. Enter HELP for available commands.\n"
        "Error: Unterminated string literal. (line 1, column 26)\n") {
        std::cerr << "Invalid input was not handled as expected.\n";
        return 1;
    }

    std::istringstream exit_input("EXIT\nHELP\n");
    std::ostringstream exit_output;
    std::ostringstream exit_error;
    if (minidb::core::run_application(1, arguments, exit_input, exit_output,
                                      exit_error) != 0 ||
        exit_output.str().find("Goodbye.") == std::string::npos ||
        exit_output.str().find("Show this help message") !=
            std::string::npos ||
        !exit_error.str().empty()) {
        std::cerr << "EXIT did not stop the command loop cleanly.\n";
        return 1;
    }

    return 0;
}
