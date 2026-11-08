# Official Proposal Alignment Audit

## Audit scope

This audit covers the repository contents created during the previous setup and
scaffolding work, including the architecture and roadmap documents. No database
feature implementation was performed as part of this audit.

The project is Mini Database Engine. The approved technology choices are C++17,
SQL, VS Code, and Git/GitHub. The approved core scope is exactly password
protection, multiple databases, multiple tables per database, record CRUD,
basic SQL operations, and persistence across application restarts.

## Official requirements

1. Users can protect databases with a password.
2. Users can create and manage multiple databases.
3. Users can create multiple tables within a database.
4. Users can insert, view, update, and delete records.
5. Users can perform basic database operations using SQL commands.
6. Databases and records remain available after the application closes.

Foreign keys, `JOIN`, transactions, indexing, and advanced SQL are not mandatory.

## Existing implementation status

| Area | Current repository state | Requirement status |
|---|---|---|
| Language/build | CMake project requests C++17; sources are divided by module | Foundation only |
| CLI | Starts a line-oriented shell; supports help and exit/quit | Partial shell |
| SQL | Recognizes a few shell words and reports SQL as unimplemented | Not implemented |
| Passwords | No password prompt, verifier, or authentication | Not implemented |
| Databases | Creates one configured data directory; no named database catalog | Not implemented |
| Tables | No schemas or table operations | Not implemented |
| Record CRUD | No record model or execution | Not implemented |
| Persistence | Data directory can be created; no database/record file format | Not implemented |
| Tests | Parser smoke test plus CLI help/version/data-directory CTest entries | Scaffold only |
| Dependencies | No third-party runtime dependency is currently declared | Currently dependency-free |
| VS Code/Git | `.vscode/` is ignored; repository has Git metadata and a GitHub remote | VS Code configuration could be preserved better |

The existing source is small and purpose-specific: `core` runs the CLI,
`parser` recognizes the two shell commands, `execution` formats help/exit
results, `storage` prepares a directory, and `utils` prints diagnostics. It
does not yet contain a database engine.

## Comparison and required architectural adjustments

### Preserve

- C++17 and the CMake/CTest foundation.
- The CLI entry point and separation between core, parser, execution, storage,
  and utilities.
- Existing smoke tests, README build instructions, and runtime-data ignore
  behavior.
- The principle of adding files only when useful behavior is implemented.

### Adjust

- Replace the former single-database assumption with a root catalog containing
  multiple named databases and a selected-database context.
- Add a password-verification boundary. Store a salted KDF verifier in database
  metadata, not a plaintext password. Keep password verification separate from
  SQL parsing and table serialization.
- Model multiple table schemas and records within each database.
- Extend the SQL parser and executor for database/table management and record
  CRUD. Preserve quoted literal contents; the current whole-command uppercase
  normalization is only suitable for shell commands.
- Persist both catalog metadata and table data in a documented versioned local
  format.
- Add tests for authentication, multiple databases/tables, SQL behavior, and
  restart persistence.
- Add a matching `security/` source/header module when implementation begins;
  do not add an empty placeholder now.
- Stop ignoring `.vscode/` wholesale. Share useful VS Code build/debug
  configuration if added, and ignore only local or generated state.

### Avoid unnecessary complexity

- Do not add a generic query planner, plugin system, repository framework, or
  class for every diagram box.
- Do not add catalog/table/security files before their behavior is ready.
- Do not introduce foreign keys, joins, transactions, indexes, or advanced SQL
  as acceptance requirements.
- The current storage directory helper manually walks path components because
  the available MinGW 8.1 standard library failed to compile its
  `std::filesystem` header. This is a portability workaround, not a desirable
  storage architecture; validate the intended C++17 toolchain before deciding
  whether to replace it with `std::filesystem`.
- The existing no-third-party-dependency position conflicts with secure
  password KDF support in the C++17 standard library, which has no password KDF.
  Do not solve this by inventing a KDF or using a fast hash. Select an approved
  platform crypto provider or vetted dependency before implementing password
  protection.

## Core features and optional extensions

### Core (required for acceptance)

- Password-protected database creation and authentication.
- Multiple persistent databases and a way to list, select, and manage them.
- Multiple tables per database.
- Basic SQL commands for database/table management and insert, view, update,
  and delete operations.
- Persistence of database metadata, table schemas, and records across restart.

### Optional / not required

- Foreign keys and relationships.
- `JOIN`.
- Transactions and crash recovery.
- Indexes and query optimization.
- Advanced SQL beyond the agreed basic command subset.
- Network access, multi-user roles, or encryption of data at rest.

Optional work must not block or be silently folded into core acceptance.

## Recommended implementation sequence

1. Approve the password KDF/provider, password-entry UX, database command
   behavior, and minimum SQL grammar.
2. Preserve the C++17 build and define only shared types needed by the first
   database/storage work.
3. Implement and test a persistent multi-database catalog and protected
   database authentication.
4. Implement multiple persistent table schemas and records inside a selected
   database.
5. Implement a tokenizer/parser for the agreed basic SQL subset, keeping
   keyword handling separate from quoted values.
6. Implement SQL execution for database/table operations and record CRUD.
7. Integrate the CLI, verify restart persistence and incorrect-password
   rejection, then update user documentation and run full tests.

The detailed milestone exit criteria are in [ROADMAP.md](./ROADMAP.md); module
boundaries and proposed commands are in [ARCHITECTURE.md](./ARCHITECTURE.md).

## Current folder structure

```text
SPL-1/
├── .gitignore
├── CMakeLists.txt
├── README.md
├── data/
│   └── .gitkeep
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PROPOSAL_ALIGNMENT.md
│   └── ROADMAP.md
├── include/minidb/
│   ├── core/application.hpp
│   ├── execution/command_executor.hpp
│   ├── parser/statement.hpp
│   ├── storage/database_directory.hpp
│   └── utils/diagnostic.hpp
├── src/
│   ├── core/application.cpp
│   ├── execution/command_executor.cpp
│   ├── main.cpp
│   ├── parser/statement.cpp
│   ├── storage/database_directory.cpp
│   └── utils/diagnostic.cpp
└── tests/
    ├── CMakeLists.txt
    └── parser_tests.cpp
```

Build output and temporary test data may also exist locally under the ignored
`build/` directory; they are generated artifacts, not project source.

## Decisions needing approval before feature implementation

1. **Password KDF/provider:** choose a vetted KDF and how it is supplied
   (platform crypto API or an approved dependency). The C++17 standard library
   alone is insufficient for safe password verification.
2. **Password-entry UX:** confirm concealed interactive prompts for database
   creation/selection rather than SQL literals or command-line arguments, which
   can expose passwords through history or process listings.
3. **Database removal:** confirm whether `DROP DATABASE` requires a second
   confirmation because it removes the database's stored tables.
4. **SQL subset and types:** confirm the proposed commands and initial
   `INTEGER`, `REAL`, `TEXT`, and `BOOLEAN` types in `ARCHITECTURE.md`.
5. **Toolchain:** confirm the compiler/CMake versions to support. The previously
   available MinGW GCC 8.1 could compile the scaffold after avoiding its broken
   `std::filesystem` header, but CMake was unavailable in that environment.

No source files or existing runtime data were removed by this audit.
