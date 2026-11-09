# Mini Database Engine: Technical Architecture

## Purpose and status

This document describes the target architecture for the official SPL-1 project
proposal. It is a design, not a claim that the features have already been
implemented.

The current program is a C++17 command-line shell with `HELP`, `EXIT`, and
`QUIT`, plus standalone database and table managers. `DatabaseManager` creates,
lists, selects, and drops named databases with versioned metadata.
`TableManager` creates, lists, describes, and drops tables in the selected
database and persists each table's versioned column schema. These managers are
not yet connected to the CLI. Password authentication, SQL execution, and
record persistence are not implemented.

## Official core requirements

The implementation must provide:

1. Password protection for databases.
2. Creation and management of multiple databases.
3. Multiple tables in a database.
4. Record insertion, viewing, updating, and deletion.
5. Basic SQL commands for database operations.
6. Local persistence across application restarts.

The selected implementation language is C++17, with VS Code as the development
environment and Git/GitHub for version control. The project constraint is
strict: use only the C++ standard library at runtime, with CMake as the build
system. No third-party libraries, package managers, or downloaded frameworks
are permitted.

## Design principles

- Keep the implementation understandable to a university student.
- Keep each module responsible for one concern and testable through its public
  interface.
- Use only the C++ standard library; do not add third-party dependencies.
- Treat user input, malformed database files, and filesystem failures as
  explicit errors.
- Do not store database passwords in plaintext or describe a weak general
  purpose hash as password protection.
- Keep the required SQL subset small. Foreign keys, joins, transactions,
  indexes, and advanced SQL are not required for acceptance.

## Proposed module boundaries

The existing top-level module layout is a useful starting point. Add a
`security` module because password protection is a required feature. Add source
and header files only when their behavior is ready to implement; the tree below
is a target, not a request to create empty placeholders.

```text
SPL-1/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── data/                          # Runtime database files; ignored by Git
├── docs/
│   ├── ARCHITECTURE.md
│   ├── PROPOSAL_ALIGNMENT.md
│   └── ROADMAP.md
├── include/minidb/
│   ├── core/                      # CLI/application lifecycle
│   ├── execution/                 # SQL statement semantics
│   ├── parser/                    # Tokens, grammar, statement types
│   ├── security/                  # Future password gate; limitations documented
│   ├── storage/
│   │   ├── database_manager.hpp   # Named databases and metadata
│   │   └── table_manager.hpp      # Selected-database table schemas
│   └── utils/                     # Shared diagnostics and small helpers
├── src/
│   ├── core/
│   ├── execution/
│   ├── parser/
│   ├── security/
│   ├── storage/
│   │   ├── database_manager.cpp
│   │   └── table_manager.cpp
│   ├── utils/
│   └── main.cpp
└── tests/
    ├── CMakeLists.txt
    ├── parser_tests.cpp
    ├── database_manager_tests.cpp
    ├── table_manager_tests.cpp
    ├── security_tests.cpp
    ├── storage_tests.cpp
    └── execution_tests.cpp
```

Test files should be added alongside real behavior. A small project may combine
closely related tests rather than making one executable per source file.

## Module responsibilities

### Core (`core/`)

- Parse application options and run the command-line REPL.
- Coordinate authentication, parsing, execution, and results.
- Keep terminal input and output out of storage and SQL semantics.
- Route sensitive password entry through a concealed prompt, subject to the
  terminal/platform decision recorded in `PROPOSAL_ALIGNMENT.md`.

### Parser (`parser/`)

- Tokenize SQL input while preserving literal contents and source locations.
- Parse the supported SQL subset into typed statement structures.
- Recognize keywords without uppercasing or otherwise changing quoted data.
- Report unsupported syntax before execution or persistent changes.

The current parser is only a small command recognizer. It is not a SQL parser
and must not be extended by uppercasing complete SQL input, since that would
change case-sensitive text literals.

### Execution (`execution/`)

- Check statements against the selected database and table schemas.
- Implement database/table commands and the required record CRUD operations.
- Return structured results for the CLI to display.
- Depend on parser, storage, and security interfaces, not direct terminal or
  filesystem calls.

### Storage (`storage/`)

- `DatabaseManager` owns database directory discovery, creation, selection,
  deletion, and database metadata under the configured local data root.
- Use `std::filesystem` for directory and path operations. Database identifiers
  are restricted to ASCII letters/underscores followed by ASCII letters,
  digits, or underscores, so names cannot escape the configured data root.
- Table and column identifiers use the same ASCII naming rule and
  case-insensitive uniqueness behavior.
- Each database directory contains a `database.meta` file with a format
  signature, version, and canonical database name. Unsupported or corrupt
  metadata is reported, not silently treated as a valid database.
- `TableManager` stores each table under
  `<selected-database>/tables/<table-name>/schema.meta`. Each schema file has a
  format signature, version, table name, column count, and ordered column
  name/type pairs. Supported types are `INT`, `FLOAT`, `DOUBLE`, `VARCHAR`,
  `TEXT`, and `BOOLEAN`; identifiers and duplicate columns are validated.
- Record storage and SQL execution are future work. Keep future table records
  within their table directories and validate schemas and records using
  documented file formats.
- No password or password verifier is currently stored. Do not extend metadata
  for password protection until the security limitations of a standard-library-
  only implementation are explicitly accepted.
- Never store a recoverable plaintext password.
- Report I/O, corrupt-file, and unsupported-format errors explicitly.
- Use safe file-replacement practices for individual writes. This is an
  implementation reliability measure, not a requirement for multi-statement
  transactions or crash recovery.

### Security (`security/`)

- This module is not implemented. C++17 does not provide a password KDF,
  cryptographically secure random-number generator, or constant-time comparison
  facility.
- Do not invent a cryptographic algorithm or present `std::hash`, an ordinary
  checksum, or reversible obfuscation as secure password protection.
- Keep any future authentication state scoped to the current process/session.

Under the no-external-library constraint, the project cannot honestly provide
cryptographically secure password verification using only portable C++17. A
simple application password gate may be possible, but any standard-library-only
storage/comparison approach has security limitations and must not be described
as secure password hashing. Database password protection also does not, by
itself, encrypt data at rest. The acceptable interpretation of the official
password requirement therefore needs approval before that feature is
implemented.

### Utilities (`utils/`)

- Share small diagnostics and formatting helpers used by multiple modules.
- Avoid turning one-off behavior into generic frameworks.

### Build, tests, and runtime data

- CMake configures C++17 executable and test targets without downloading or
  finding third-party dependencies.
- On GCC versions below 9, CMake links `stdc++fs`, the compiler's own
  standard-library compatibility unit needed by `std::filesystem`; it is not a
  third-party library. Other toolchains use their C++17 standard-library
  filesystem implementation. At configure time, CMake compile-links a small
  filesystem probe and stops with an explicit toolchain error if the selected
  standard library is broken or incomplete.
- VS Code workspace configuration may be committed when it helps reproduce the
  build; `.vscode/` should not be ignored wholesale.
- Tests use temporary directories and do not depend on committed user data.
- The `data/` directory contains runtime databases and is ignored except for
  its directory marker.

## Dependency diagram

```mermaid
flowchart TD
    User[User] --> CLI[Core CLI / REPL]
    CLI --> Parser[SQL parser]
    CLI --> Auth[Security / password verifier]
    Parser --> AST[Typed statement]
    AST --> Exec[Statement execution]
    Exec --> Catalog[Database catalog]
    Exec --> Tables[Table schemas and records]
    Catalog --> Store[Local file storage]
    Tables --> Store
    Store --> Disk[(Data root: databases and tables)]
    Auth --> Store
    Exec --> Result[Structured result or diagnostic]
    Result --> CLI
    Auth --> Constraint[No standard C++17 password KDF]
    Auth --> Result
```

The diagram shows logical dependencies, not a requirement to create a class for
every box. Portable C++17 does not supply a password KDF. Do not add a
third-party provider or dependency; the security limitation must be accepted
and documented before password behavior is implemented.

## Database and table organization

Use one configured root directory for multiple named databases. The current
manager stores each database in a directory named by its validated identifier.
Its metadata file contains a signature, format version, and database name. A
future password implementation can extend the versioned metadata format. Table
files will be added within database directories later. The current layout is:

```text
data/
├── students_db/
│   └── database.meta
└── library_db/
    └── database.meta
```

Names must be validated before they are mapped to paths. `DatabaseManager`
discovers only directories with valid metadata and reports corrupt metadata
instead of silently treating it as a valid database. Table files should
eventually record their schema and rows. Final record encodings should be
documented before table-storage implementation.

The database catalog and records must reload on a later application run.
Authentication must be required before a protected database is selected for
ordinary operations. The exact policy for listing names, changing passwords,
and handling an unprotected database remains a project decision; no extra
account or role system is implied.

## Minimum SQL command proposal

This list is a practical interpretation of “basic database operations” and is
subject to review. Keywords are case-insensitive; quoted text values are
preserved. One statement per input line is sufficient for the initial CLI.

| Requirement | Proposed command |
|---|---|
| Database creation | `CREATE DATABASE name;` (then concealed password setup) |
| Database listing | `SHOW DATABASES;` |
| Database selection | `USE name;` (then concealed password prompt) |
| Database removal | `DROP DATABASE name;` |
| Table listing | `SHOW TABLES;` |
| Table description | `DESCRIBE table_name;` |
| Table creation | `CREATE TABLE name (column type, ...);` |
| Table removal | `DROP TABLE name;` |
| Insert record | `INSERT INTO name VALUES (...);` |
| View records | `SELECT * FROM name [WHERE column operator value];` |
| Update records | `UPDATE name SET column = value [WHERE column operator value];` |
| Delete records | `DELETE FROM name [WHERE column operator value];` |
| Session help/exit | `HELP;`, `EXIT;`, `QUIT;` |

Candidate initial scalar types are `INTEGER`, `REAL`, `TEXT`, and `BOOLEAN`.
Candidate comparison operators are `=`, `!=`, `<`, `<=`, `>`, and `>=`.
Omitting `WHERE` from `UPDATE` or `DELETE` affects all records and must be
explained in help output. The exact grammar, identifier rules, password prompt
flow, and whether database drop requires an extra confirmation are review
decisions, not facts already implemented.

## Out of scope for core acceptance

Do not treat any of these as required:

- foreign keys or other relational constraints beyond what the agreed table
  model needs;
- `JOIN`;
- transactions, crash recovery, or concurrent clients;
- indexes or query optimization;
- advanced SQL such as subqueries, aggregates, compound predicates, or
  stored procedures;
- encryption of data at rest, user accounts, roles, or network access.

Any extension needs explicit scope agreement and must not delay the six core
requirements.
