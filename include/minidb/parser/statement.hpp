#pragma once

#include "minidb/utils/diagnostic.hpp"

#include <optional>
#include <string_view>
#include <variant>

namespace minidb::parser {

struct HelpStatement {};
struct ExitStatement {};

using Statement = std::variant<HelpStatement, ExitStatement>;

struct ParseResult {
    std::optional<Statement> statement;
    std::optional<utils::Diagnostic> diagnostic;
};

ParseResult parse_command(std::string_view input);

}  // namespace minidb::parser
