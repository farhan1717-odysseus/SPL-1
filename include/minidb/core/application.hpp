#pragma once

#include <iosfwd>

namespace minidb::core {

int run_application(int argc, char* argv[], std::istream& input,
                    std::ostream& output, std::ostream& error);

}  // namespace minidb::core
