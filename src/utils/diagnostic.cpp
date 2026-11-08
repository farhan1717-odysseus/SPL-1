#include "minidb/utils/diagnostic.hpp"

#include <ostream>

namespace minidb::utils {

void write_diagnostic(std::ostream& output, const Diagnostic& diagnostic) {
    output << "Error: " << diagnostic.message << '\n';
}

}  // namespace minidb::utils
