#pragma once

#include <iosfwd>
#include <string>

namespace minidb::utils {

struct Diagnostic {
    std::string message;
};

void write_diagnostic(std::ostream& output, const Diagnostic& diagnostic);

}  // namespace minidb::utils
