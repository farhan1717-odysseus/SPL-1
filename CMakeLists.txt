cmake_minimum_required(VERSION 3.16)

project(MiniDatabaseEngine VERSION 0.1.0 LANGUAGES CXX)

add_library(minidb_engine
    src/core/application.cpp
    src/core/cli.cpp
    src/execution/command_executor.cpp
    src/parser/statement.cpp
    src/storage/database_directory.cpp
    src/utils/diagnostic.cpp
)

target_include_directories(minidb_engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(minidb_engine PUBLIC cxx_std_17)
target_compile_definitions(minidb_engine PRIVATE MINIDB_VERSION="${PROJECT_VERSION}")
set_target_properties(minidb_engine PROPERTIES CXX_EXTENSIONS OFF)

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
