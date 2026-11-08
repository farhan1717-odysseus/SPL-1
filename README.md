# Mini Database Engine

A command-line Mini Database Engine project for SPL-1. The approved target is a
C++17 SQL database with password-protected, persistent databases, multiple
tables, and basic record operations. The current build provides an interactive
command shell; database, authentication, SQL execution, and record persistence
are not implemented yet.

## Requirements

- CMake 3.16 or newer
- A C++17 compiler (for example, GCC, Clang, or MSVC)

The project has no third-party runtime dependencies.

## Build

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
without ending the session. The approved SQL and database features will be
added in later milestones. The `data/` directory is reserved for generated
runtime database files and is ignored by Git except for its directory marker.

## Project layout

- `include/minidb/` — public headers grouped by module
- `src/` — C++ implementations grouped into `core`, `parser`, `execution`,
  `storage`, and `utils`
- `tests/` — parser and command-line smoke tests
- `docs/ARCHITECTURE.md` — target module responsibilities and command proposal
- `docs/ROADMAP.md` — implementation milestones
- `docs/PROPOSAL_ALIGNMENT.md` — requirements audit and decisions needing approval
