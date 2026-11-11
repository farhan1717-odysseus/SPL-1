cmake_minimum_required(VERSION 3.16)

project(MiniDatabaseEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXSourceCompiles)

if(DEFINED CMAKE_REQUIRED_LIBRARIES)
    set(_minidb_had_required_libraries TRUE)
    set(_minidb_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    # GCC 8 ships std::filesystem in this standard-library compatibility unit.
    # This is part of the selected C++ toolchain, not a third-party dependency.
    list(APPEND CMAKE_REQUIRED_LIBRARIES stdc++fs)
endif()

check_cxx_source_compiles([=[
    #include <filesystem>

    int main() {
        std::error_code error;
        const auto path = std::filesystem::current_path(error);
        return error || path.empty();
    }
]=] MINIDB_HAS_CXX17_FILESYSTEM)

if(_minidb_had_required_libraries)
    set(CMAKE_REQUIRED_LIBRARIES "${_minidb_saved_required_libraries}")
else()
    unset(CMAKE_REQUIRED_LIBRARIES)
endif()
unset(_minidb_had_required_libraries)
unset(_minidb_saved_required_libraries)

if(NOT MINIDB_HAS_CXX17_FILESYSTEM)
    message(FATAL_ERROR
        "The selected C++17 toolchain cannot compile and link std::filesystem. "
        "Use a compiler with a working C++17 standard library; no external "
        "filesystem dependency or fallback is used.")
endif()

add_library(minidb_engine
    src/core/application.cpp
    src/core/cli.cpp
    src/execution/command_executor.cpp
    src/execution/sql_session.cpp
    src/parser/lexer.cpp
    src/parser/sql_parser.cpp
    src/parser/statement.cpp
    src/storage/database_directory.cpp
    src/storage/database_manager.cpp
    src/storage/record_store.cpp
    src/storage/table_manager.cpp
    src/utils/diagnostic.cpp
)

target_include_directories(minidb_engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(minidb_engine PUBLIC cxx_std_17)
target_compile_definitions(minidb_engine PRIVATE MINIDB_VERSION="${PROJECT_VERSION}")
set_target_properties(minidb_engine PROPERTIES CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(minidb_engine PUBLIC stdc++fs)
endif()

if(MSVC)
    target_compile_options(minidb_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(minidb_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(minidb src/main.cpp)
target_link_libraries(minidb PRIVATE minidb_engine)
set_target_properties(minidb PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(minidb PRIVATE /W4 /permissive-)
else()
    target_compile_options(minidb PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
