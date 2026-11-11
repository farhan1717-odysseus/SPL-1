# Supported SQL Grammar

The SQL parser converts input into typed command objects. It validates syntax
only; it does not execute commands or access database storage.

## Statements

```ebnf
statement           = create_database
                    | drop_database
                    | show_databases
                    | use_database
                    | create_table
                    | drop_table
                    | show_tables
                    | describe_table
                    | insert
                    | select
                    | update
                    | delete ;

create_database     = "CREATE" "DATABASE" identifier ;
drop_database       = "DROP" "DATABASE" identifier ;
show_databases      = "SHOW" "DATABASES" ;
use_database        = "USE" identifier ;

create_table        = "CREATE" "TABLE" identifier
                      "(" column_definition
                      { "," column_definition } ")" ;
column_definition   = identifier column_type ;
column_type         = "INT" | "FLOAT" | "DOUBLE"
                    | "VARCHAR" | "TEXT" | "BOOLEAN" ;
drop_table          = "DROP" "TABLE" identifier ;
show_tables         = "SHOW" "TABLES" ;
describe_table      = "DESCRIBE" "TABLE" identifier ;

insert              = "INSERT" "INTO" identifier "VALUES"
                      "(" literal { "," literal } ")" ;
select              = "SELECT" ( "*" | identifier
                      { "," identifier } )
                      "FROM" identifier [ "WHERE" condition ] ;
update              = "UPDATE" identifier "SET" identifier "=" literal
                      "WHERE" condition ;
delete              = "DELETE" "FROM" identifier
                      "WHERE" condition ;

condition           = or_condition ;
or_condition        = and_condition { "OR" and_condition } ;
and_condition       = condition_atom { "AND" condition_atom } ;
condition_atom      = comparison | "(" condition ")" ;
comparison          = identifier comparison_operator literal
                    | identifier "IS" [ "NOT" ] "NULL" ;
comparison_operator = "=" | "!=" | "<" | ">" | "<=" | ">=" ;

literal             = [ "+" | "-" ] number
                    | string
                    | "TRUE"
                    | "FALSE"
                    | "NULL" ;
number              = integer | floating_point ;
```

Each input contains exactly one statement. A single trailing semicolon is
optional. Keywords are case-insensitive; identifiers preserve their input
spelling. Whitespace, including newlines, may appear between grammar elements.
The interactive CLI accepts one complete statement per input line.

Identifiers begin with an ASCII letter or underscore and continue with ASCII
letters, digits, or underscores. Reserved keywords cannot be used as
identifiers. Windows reserved device names (`CON`, `NUL`, `COM1`, and the other
reserved device names) are also rejected. `VARCHAR` currently has no length
parameter.

The tokenizer recognizes single-quoted strings, numbers, and comparison
operators. INSERT, UPDATE, and WHERE expressions accept numeric, string,
boolean, and NULL literals. Strings use single quotes; a quote inside a string
is escaped by doubling it (`'it''s'`). Backslashes are ordinary string
characters. UPDATE and DELETE require WHERE conditions; this is intentional to
avoid accidental table-wide modifications. UPDATE supports one assigned
column, and INSERT supplies one row of values without an explicit column list.
SELECT supports `*` or a comma-separated list of columns with an optional
WHERE clause. Conditions support comparisons combined with `AND` and `OR`;
`AND` binds more tightly than `OR`, and parentheses can group conditions.
`IS NULL` and `IS NOT NULL` test null values; comparisons using `= NULL` or
other operators with NULL evaluate as unknown and do not match. Column-to-column
comparisons and other expressions are not supported.

The database and table listings omit entries whose metadata is invalid so one
corrupt database or table does not prevent normal operations on valid entries.
An explicitly named corrupt database or table can be removed with `DROP`.

## Examples

Each example below is a separate SQL input.

```sql
CREATE DATABASE school;
USE school
CREATE TABLE student (
    id INT,
    name VARCHAR,
    average DOUBLE,
    active BOOLEAN
);
INSERT INTO student VALUES (1, 'Ada', 99.5, TRUE);
SELECT * FROM student;
SELECT id, name FROM student;
UPDATE student SET active = FALSE WHERE id = 1;
DELETE FROM student WHERE id = 1;
SHOW TABLES;
DESCRIBE TABLE student;
DROP TABLE student;
SHOW DATABASES;
DROP DATABASE school;
```
