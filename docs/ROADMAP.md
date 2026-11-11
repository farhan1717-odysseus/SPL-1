# Mini Database Engine: Development Roadmap

This roadmap is aligned to the six official requirements in
[PROPOSAL_ALIGNMENT.md](./PROPOSAL_ALIGNMENT.md). The interactive CLI now
executes database/table statements and basic persistent record CRUD. Password
authentication remains unimplemented; keep the project buildable and test each
milestone as behavior is added.

## Milestone 0: Resolve design approvals

- Confirm how a database is created and selected and when its password is
  requested.
- Agree on the security limitations of password protection under the
  standard-library-only constraint. Do not add a provider or invent a KDF.
- Agree on the minimum SQL command grammar and initial scalar types.
- Confirm the intended data-root and database naming behavior.

**Exit criteria:** Password handling and file/catalog assumptions are documented
before persistent format or security code is written.

## Milestone 1: Prepare the foundation

- Keep CMake configured for C++17 and preserve the existing modular layout.
- Correct the parser architecture so SQL keywords are recognized without
  changing quoted literals.
- Define shared diagnostics, values, schemas, identifiers, and result types only
  as they become necessary.
- Add or update tests to reflect the approved decisions.

**Exit criteria:** Clean build and tests pass; the code has clear boundaries for
parsing, execution, storage, and password verification.

## Milestone 2: Database catalog and approved password gate

- Extend the existing manager as needed when authentication and CLI integration
  are designed. Its create/drop/show/use behavior and versioned metadata are
  already implemented and covered by lifecycle tests.
- Implement only the password behavior explicitly approved for a
  standard-library-only classroom project.
- Document its limitations; do not claim cryptographic security or use a custom
  KDF, `std::hash`, or plaintext storage as secure password protection.
- Test incorrect passwords, correct passwords, restart persistence, invalid
  names, and corrupt catalog/metadata.

**Exit criteria:** Multiple databases can be managed across restarts and the
approved password behavior rejects failed authentication. The database-
directory and metadata foundation is present; password protection remains
blocked on approval of an honest security level.

## Milestone 3: Table schemas and persistent records

- Completed: implement table creation, listing/description, and removal within
  the selected database, with a separate versioned schema file per table.
- Completed: validate table/column identifiers, duplicate columns, supported
  types, and schema metadata.
- Completed: test multiple tables, database isolation, schema persistence
  across fresh manager instances, and corrupt metadata.
- Completed: store typed rows in a versioned record file and validate row
  counts, value types, and record-file errors.
- Completed: test record persistence across fresh sessions.

**Exit criteria:** A selected database can hold multiple tables with validated,
persistent schemas and record data. Schema storage and its lifecycle tests are
implemented; record persistence remains outstanding.

## Milestone 4: Basic SQL parser

- Tokenize identifiers, keywords, punctuation, operators, numbers, booleans,
  and quoted text.
- Parse only the approved database/table and CRUD statements into typed
  statements.
- Reject malformed or unsupported syntax with clear diagnostics before a
  database write.
- Test valid syntax, invalid syntax, casing, and quoted values.

**Exit criteria:** Every core command has a parser test; no advanced SQL is
required.

## Milestone 5: SQL execution and record CRUD

- Completed: connect parsed statements to database, table, and record storage.
- Completed: execute insert, select, update, and delete with type and column
  validation.
- Completed: apply WHERE filters and return formatted query rows/statuses.
- Completed: test CRUD, filters, invalid values/columns, and persistence across
  restart.

**Exit criteria:** The required SQL operations work on multiple tables and
multiple persistent databases.

## Milestone 6: CLI integration and acceptance

- Completed: connect database commands, SQL execution, help, and exit behavior
  in the CLI.
- Remaining: concealed password entry and authentication require an approved
  standard-library-only security approach.
- Ensure user-level errors do not silently terminate a healthy session.
- Completed: update README with build, run, database, SQL, and data-location
  instructions.
- Run a clean CMake build and the complete CTest suite in the intended
  environment.
- Demonstrate create/select database, create tables, CRUD, restart, and verify
  persisted records. Password protection and authentication remain blocked.

**Exit criteria:** All six official requirements have reproducible automated or
documented end-to-end verification.

## Explicitly deferred

Foreign keys, joins, transactions, indexes, query optimization, advanced SQL,
networking, multi-user roles, and data-at-rest encryption are not core
milestones. Consider them only after core acceptance and explicit approval.
