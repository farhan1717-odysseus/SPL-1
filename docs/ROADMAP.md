# Mini Database Engine: Development Roadmap

This roadmap is aligned to the six official requirements in
[PROPOSAL_ALIGNMENT.md](./PROPOSAL_ALIGNMENT.md). The repository currently
contains only a CLI/build scaffold; all database and password features remain
unimplemented. Keep the project buildable and test each milestone before
adding the next behavior.

## Milestone 0: Resolve design approvals

- Confirm how a database is created and selected and when its password is
  requested.
- Select a safe password KDF/provider and approve any required dependency.
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

## Milestone 2: Database catalog and password protection

- Implement database discovery, creation, listing, selection, and removal.
- Implement protected database setup and authentication using the approved
  password KDF/provider.
- Persist a salted password verifier and format metadata, never plaintext
  passwords.
- Test incorrect passwords, correct passwords, restart persistence, invalid
  names, and corrupt catalog/metadata.

**Exit criteria:** Multiple databases can be managed across restarts and
protected databases reject failed authentication.

## Milestone 3: Table schemas and persistent records

- Implement table creation, listing/description, loading, saving, and removal
  within the selected database.
- Define and document the versioned schema/record file format.
- Validate schema, type, record count, and file errors.
- Test multiple tables and persistence across fresh program/storage instances.

**Exit criteria:** A selected database can hold multiple tables with validated,
persistent schema and record data.

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

- Connect parsed statements to the database catalog and table storage.
- Implement insert, select/view, update, and delete with type and column
  validation.
- Implement the agreed simple filter behavior and safe, explicit behavior when
  `WHERE` is omitted.
- Return structured rows/statuses for CLI formatting.
- Test empty tables, zero/one/many matches, invalid columns/types, and writes
  surviving restart.

**Exit criteria:** The required SQL operations work on multiple tables and
multiple persistent databases.

## Milestone 6: CLI integration and acceptance

- Connect concealed password entry, database commands, SQL execution, help,
  and exit behavior in the CLI.
- Ensure user-level errors do not silently terminate a healthy session.
- Update README with build, run, password, database, SQL, and data-location
  instructions.
- Run a clean CMake build and the complete CTest suite in the intended
  environment.
- Demonstrate create/protect/select database, create multiple tables, CRUD,
  restart, authenticate, and verify persisted records.

**Exit criteria:** All six official requirements have reproducible automated or
documented end-to-end verification.

## Explicitly deferred

Foreign keys, joins, transactions, indexes, query optimization, advanced SQL,
networking, multi-user roles, and data-at-rest encryption are not core
milestones. Consider them only after core acceptance and explicit approval.
