#include "minidb/core/application.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    return minidb::core::run_application(argc, argv, std::cin, std::cout,
                                         std::cerr);
}
