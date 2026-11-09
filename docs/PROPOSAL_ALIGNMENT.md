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
| Databases | Standalone manager supports create/drop/show/use with versioned per-database metadata; not wired into CLI | Partial |
| Tables | Standalone `TableManager` creates, drops, lists, and describes tables in the selected database; schemas are persisted separately with supported column types | Partial; not wired to SQL/CLI |
| Record CRUD | No record model or execution | Not implemented |
| Persistence | Database directories and versioned metadata persist; table records have no format yet | Partial |
| Tests | Parser/CLI and database-manager tests, plus table-manager lifecycle/schema tests | Scaffold and storage coverage |
| Dependencies | Project constraint allows only the C++17 standard library; CMake has a GCC 8 standard-library filesystem compatibility link | Compliant; no third-party dependency |
| VS Code/Git | `.vscode/` is ignored; repository has Git metadata and a GitHub remote | VS Code configuration could be preserved better |

The existing source is small and purpose-specific: `core` runs the CLI,
`parser` recognizes the two shell commands, `execution` formats help/exit
results, `storage` prepares the data directory and manages named databases,
table schemas, and metadata, and `utils` prints diagnostics. The database and
table managers are not yet connected to SQL or the CLI; records and password
authentication are still unimplemented.

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
- Preserve and build on the new standalone `DatabaseManager`; integrate it only
  when the SQL/application pipeline is ready.
- Add a password-verification boundary only after agreeing how the official
  password requirement can be represented under the standard-library-only
  constraint. The current metadata stores only a signature, version, and
  database name. Keep authentication separate from SQL parsing and table
  serialization.
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
- The available MinGW 8.1 installation has a broken `std::filesystem` header.
  The storage helper now uses the standard C++17 filesystem API as required;
  build validation needs a working C++17 toolchain rather than platform-specific
  substitutes.
- C++17 has no password KDF, cryptographically secure random-number generator,
  or constant-time comparison API. With the no-external-library requirement,
  do not implement a custom cryptographic algorithm, use `std::hash`, or claim
  secure password hashing. A basic password gate is not equivalent to
  cryptographically secure password protection. Resolve the acceptable
  security level before implementing the password requirement.

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

1. Agree on the honest security level achievable for password protection while
   using only the C++17 standard library; also confirm password-entry UX,
   database command behavior, and minimum SQL grammar.
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

1. **Password security under dependency restriction:** the C++17 standard
   library does not provide a safe password KDF or cryptographic random source.
   The project must not invent cryptography or add a third-party dependency.
   Decide whether a clearly limited classroom password gate satisfies the
   proposal, or whether the no-external-library constraint can be changed.
2. **Password-entry UX:** confirm concealed interactive prompts for database
   creation/selection rather than SQL literals or command-line arguments, which
   can expose passwords through history or process listings.
3. **Database removal:** confirm whether `DROP DATABASE` requires a second
   confirmation because it removes the database's stored tables.
4. **SQL subset and types:** confirm the proposed commands and initial
   `INTEGER`, `REAL`, `TEXT`, and `BOOLEAN` types in `ARCHITECTURE.md`.
5. **Toolchain:** confirm the compiler/CMake versions to support. The available
   MinGW GCC 8.1 installation has a broken `std::filesystem` implementation,
   and CMake is unavailable in this environment. The project requires working
   standard-library filesystem support; it does not substitute platform APIs or
   download a replacement library.

No source files or existing runtime data were removed by this audit.
