#pragma once

#include <iosfwd>
#include <string_view>

namespace minidb::core {

using CommandHandler = bool (*)(std::string_view command, std::ostream& output,
                                std::ostream& error);

int run_cli(std::istream& input, std::ostream& output, std::ostream& error,
            CommandHandler handle_command);

}  // namespace minidb::core
