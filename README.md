# Mini Database Engine

A command-line Mini Database Engine project for SPL-1. It provides an
interactive SQL shell with persistent databases, table schemas, and basic
record operations. The project uses C++17 and only the standard library.

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

Build the whole project with CMake; compiling `src/main.cpp` alone does not
include the engine implementation. From the repository root, run:

```powershell
.\build-msvc-x64\Debug\minidb.exe
```

For Git Bash, the equivalent executable path is:

```bash
./build-msvc-x64/Debug/minidb.exe
```

The CLI stores runtime data in a `data\` directory under the current working
directory. The directory is generated at runtime and ignored by Git.

## Supported SQL

Enter one statement per input line; a trailing semicolon is optional.

```sql
CREATE DATABASE school;
USE school;
CREATE TABLE student (id INT, name TEXT, score DOUBLE, active BOOLEAN);
INSERT INTO student VALUES (1, 'Ada', 98.5, TRUE);
SELECT * FROM student;
SELECT name, score FROM student WHERE score >= 90;
UPDATE student SET active = FALSE WHERE id = 1;
DELETE FROM student WHERE id = 1;
SHOW TABLES;
DESCRIBE TABLE student;
```

The shell also supports `SHOW DATABASES`, `DROP TABLE`, `DROP DATABASE`,
`HELP`, `EXIT`, and `QUIT`. Tables and typed records persist across program
restarts. Supported column types are `INT`, `FLOAT`, `DOUBLE`, `VARCHAR`,
`TEXT`, and `BOOLEAN`; basic `WHERE` comparisons can be combined with `AND`,
`OR`, and parentheses. See [docs/SQL_GRAMMAR.md](docs/SQL_GRAMMAR.md) for
syntax details.

Password authentication is not implemented. Do not treat this engine as
providing access control or as suitable for sensitive data.

## Project layout

- `include/minidb/` — public headers grouped by module
- `src/` — C++ implementations grouped into `core`, `parser`, `execution`,
  `storage`, and `utils`
- `tests/` — parser, command-line, database-manager, and table-manager tests
- `docs/ARCHITECTURE.md` — target module responsibilities and command proposal
- `docs/ROADMAP.md` — implementation milestones
- `docs/PROPOSAL_ALIGNMENT.md` — requirements audit and decisions needing approval
