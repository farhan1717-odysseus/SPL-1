# Mini Database Engine

A command-line Mini Database Engine project for SPL-1. The approved target is a
C++17 SQL database with password-protected, persistent databases, multiple
tables, and basic record operations. The current build provides an interactive command shell, a standalone
database manager, and a table manager for persistent table schemas within the
selected database. These managers are not connected to the CLI yet; password
authentication, SQL execution, and record persistence are not implemented.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler (for example, GCC, Clang, or MSVC)

The project uses only the C++17 standard library at runtime. It does not use
third-party libraries, frameworks, package managers, or downloaded build
dependencies. CMake is used only to configure and build the project.

## Build

For an x64 build with the Visual Studio 2026 MSVC toolchain, use the included
preset. It writes to `build-msvc-x64`, leaving any existing `build` directory
untouched:

```powershell
cmake --preset msvc-x64
cmake --build --preset msvc-x64 --config Debug
ctest --preset msvc-x64 -C Debug
```

From the repository root:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is written to the build directory (on Windows,
`build\Debug\minidb.exe` or `build\Release\minidb.exe` with multi-configuration
generators).

## Run

```powershell
.\build\minidb.exe
```

At this stage, the shell displays the application name/version, supports
`HELP`, `EXIT`, and `QUIT`, ignores empty input, and reports invalid commands
without ending the session. `DatabaseManager` stores each database in a
separate directory under its configured data root and writes versioned
`database.meta` metadata. `TableManager` creates, lists, describes, and drops
tables in the selected database; each table has a separate versioned schema
file supporting `INT`, `FLOAT`, `DOUBLE`, `VARCHAR`, `TEXT`, and `BOOLEAN`.
These manager APIs are not yet exposed as SQL commands. Password
authentication, SQL execution, and record persistence remain future work.
The `data/` directory is reserved for generated runtime database files and is
ignored by Git except for its directory marker.

## Project layout

- `include/minidb/` — public headers grouped by module
- `src/` — C++ implementations grouped into `core`, `parser`, `execution`,
  `storage`, and `utils`
- `tests/` — parser, command-line, database-manager, and table-manager tests
- `docs/ARCHITECTURE.md` — target module responsibilities and command proposal
- `docs/ROADMAP.md` — implementation milestones
- `docs/PROPOSAL_ALIGNMENT.md` — requirements audit and decisions needing approval
