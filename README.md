# oopdbmsc

`oopdbmsc` is an embedded, POSIX C99 library for
[OOPDBMS](https://github.com/KrzysztofMarciniak/oopdbms), an object-oriented
NoSQL database. Link the library into a C program, open an `.oopdb` file, and
execute OOPDBMS commands directly—no database server or `oopdbms` subprocess
is required.

## Integration Example

**An example usage of this tool:** [example-login-c-stack](https://github.com/KrzysztofMarciniak/example-login-c-stack).

## Start here

Build the library and run a narrated, end-to-end example:

```sh
make
make quick_start
```

`make quick_start` builds and runs [`examples/main.c`](examples/main.c). It
recreates `build/quick-start.oopdb`, then shows how to connect, define a
schema, create objects, query and update them, call a stored function, inspect
metadata, and close the database.

Choose a different throwaway database path with:

```sh
make quick_start QUICK_START_DB=/tmp/my-demo.oopdb
```

> `quick_start` removes its target database before running. Do not set
> `QUICK_START_DB` to a database whose contents you want to keep.

## Add it to a C program

`oopdbmsc` links to the installed OOPDBMS engine library. Build and install
OOPDBMS first:

```sh
git clone https://github.com/KrzysztofMarciniak/oopdbms.git
make -C oopdbms install PREFIX=/usr/local
```

For a non-system prefix, set `PKG_CONFIG_PATH` to
`$HOME/.local/lib/pkgconfig` when building `oopdbmsc`.

Install the library and its public header:

```sh
make install PREFIX=/usr/local
```

Compile an application against the installed shared library:

```sh
cc -std=c99 -I/usr/local/include app.c -L/usr/local/lib -loopdbmsc \
  $(pkg-config --libs oopdbms-engine) \
  -Wl,-rpath,/usr/local/lib -o app
```

For static linking, name the archive directly:

```sh
cc -std=c99 -I/usr/local/include app.c /usr/local/lib/liboopdbmsc.a \
  $(pkg-config --static --libs oopdbms-engine) -o app
```

The complete API is declared in
[`include/oopdbms/oopdbms.h`](include/oopdbms/oopdbms.h).

## Connect to a database

`oopdbms_open(path, flags, &database)` returns an opaque handle. Always close
a successful handle with `oopdbms_close(database)`.

| Situation | Call | Behavior |
| --- | --- | --- |
| Create when missing, otherwise open existing data | `oopdbms_open(path, OOPDBMS_OPEN_CREATE, &database)` | Initializes a new database only when `path` does not exist. It **does not** recreate an existing database. |
| Open existing data | `oopdbms_open(path, 0, &database)` | Fails with `OOPDBMS_ERROR_NOT_FOUND` if the file does not exist. Use this for normal application startup after initialization. |
| Inspect existing data without writes | `oopdbms_open(path, OOPDBMS_OPEN_READ_ONLY, &database)` | Opens an existing database and rejects mutating commands. |

`OOPDBMS_OPEN_CREATE` and `OOPDBMS_OPEN_READ_ONLY` cannot be combined.

```c
#include <oopdbms/oopdbms.h>
#include <stdio.h>

int main(void) {
        oopdbms_database *database;
        oopdbms_status status;

        status = oopdbms_open("library.oopdb", 0, &database);
        if (status != OOPDBMS_OK) {
                fprintf(stderr, "open failed: %s\n",
                        oopdbms_status_string(status));
                return 1;
        }

        /* Use database here. */
        oopdbms_close(database);
        return 0;
}
```

To initialize a new application database, replace `0` with
`OOPDBMS_OPEN_CREATE`. Subsequent launches should normally use `0` so a typo
in the database path does not silently create a separate database.

## Execute commands and read results

Pass one complete OOPDBMS command string to `oopdbms_execute`. On success, the
function allocates a result that the caller must release with
`oopdbms_result_free`.

```c
static int run(oopdbms_database *database, const char *command) {
        oopdbms_result *result;
        oopdbms_status status;

        status = oopdbms_execute(database, command, &result);
        if (status != OOPDBMS_OK) {
                fprintf(stderr, "%s: %s\n", command,
                        oopdbms_status_string(status));
                return 0;
        }

        printf("%s", oopdbms_result_output(result));
        oopdbms_result_free(result);
        return 1;
}

/* For a newly opened database: */
run(database, "CREATE CLASS Person (name text, age integer)");
run(database, "NEW Person (name = \"Ada\", age = 36)");
run(database, "FIND Person WHERE age >= 18");
```

The command text uses the same quoting rules as the upstream command-line
tool. Quote text values with `"..."`; use backslash escapes when needed.

### Result kinds and output

Every successful command returns a result, including commands that do not
print anything. `oopdbms_result_output(result)` is a NUL-terminated,
caller-read-only text buffer and `oopdbms_result_output_size(result)` excludes
its NUL terminator.

| Result kind | Produced by | Output |
| --- | --- | --- |
| `OOPDBMS_RESULT_NONE` | Schema changes, `SET`, `DELETE`, function definition changes | Empty |
| `OOPDBMS_RESULT_OBJECTS` | `NEW`, `FIND` | New object identity or matching object records |
| `OOPDBMS_RESULT_VALUE` | `CALL` | The returned scalar value |
| `OOPDBMS_RESULT_METADATA` | `LIST CLASSES`, `LIST INTERFACES`, `DESCRIBE` | Schema or database metadata |

Use `oopdbms_result_kind_get(result)` when output category matters. The output
format is text intended for display; keep the result alive while reading it,
then call `oopdbms_result_free(result)`.

## Command reference

Use these commands as the `command` argument of `oopdbms_execute`.

### Define and change schemas

```text
CREATE CLASS <name> [EXTENDS <class>] [IMPLEMENTS <interface>[, ...]] (<field> <type>[, ...])
DROP CLASS <name>
ALTER CLASS <name> ADD FIELD <field> <type>
ALTER CLASS <name> DROP FIELD <field>
ALTER CLASS <name> RENAME FIELD <old> <new>
ALTER CLASS <name> RENAME <new>
ALTER CLASS <name> EXTENDS <class>
ALTER CLASS <name> IMPLEMENTS <interface>

CREATE INTERFACE <name> (<field> <type>[, ...])
DROP INTERFACE <name>
```

Example:

```c
run(database, "CREATE INTERFACE Named (name text)");
run(database, "CREATE CLASS Person IMPLEMENTS Named (name text, age integer)");
run(database, "CREATE CLASS Employee EXTENDS Person (salary float)");
```

### Store and retrieve objects

```text
NEW <class> (<field> = <value>[, ...])
FIND <class> [WHERE <condition>]
SET <class> <id> (<field> = <value>[, ...])
DELETE <class> <id>
```

Example:

```c
run(database, "NEW Person (name = \"Ada Lovelace\", age = 36)");
run(database, "FIND Person WHERE age >= 18");
run(database, "SET Person 1 (age = 37)");
run(database, "DELETE Person 1");
```

### Define and call functions

```text
CREATE FUNCTION <class>.<name> (<arg> <type>[, ...]) RETURNS <type> AS <expression>
DROP FUNCTION <class>.<name>
CALL <class>.<name> (<value>[, ...]) ON <id>
```

Example:

```c
run(database,
    "CREATE FUNCTION Person.years_until (target integer) "
    "RETURNS integer AS target - age");
run(database, "CALL Person.years_until (100) ON 1");
```

### Inspect a database

```text
LIST CLASSES
LIST INTERFACES
DESCRIBE <class>
```

Example:

```c
run(database, "DESCRIBE Person");
run(database, "LIST CLASSES");
run(database, "LIST INTERFACES");
```

### Values and types

| Type | Command value example |
| --- | --- |
| `integer` | `36` |
| `float` | `19.5` |
| `text` | `"Ada Lovelace"` |
| `bool` | `true` or `false` |
| `pointer <class>` | `Person.1` |

A `pointer <class>` stores a reference to an object of that class. For
example, after creating `Person.1`, a `Book` can use `author = Person.1`.

## Errors and ownership

Every fallible API call returns `oopdbms_status`. Convert it to a stable
display string with `oopdbms_status_string(status)`.

| Status | Meaning |
| --- | --- |
| `OOPDBMS_ERROR_INVALID_ARGUMENT` | A required pointer, path, or flag combination is invalid. |
| `OOPDBMS_ERROR_OUT_OF_MEMORY` | Allocation failed. |
| `OOPDBMS_ERROR_NOT_FOUND` | An existing database was requested but the path is absent. |
| `OOPDBMS_ERROR_IO` | A file or result-capture operation failed. |
| `OOPDBMS_ERROR_DATABASE` | The existing file is not a valid supported OOPDBMS database. |
| `OOPDBMS_ERROR_TOKENIZE` / `OOPDBMS_ERROR_PARSE` | The command string is not valid OOPDBMS syntax. |
| `OOPDBMS_ERROR_EXECUTE` | A syntactically valid command could not be applied. |

Ownership rules:

- The library owns the opaque `oopdbms_database`; release it with
  `oopdbms_close`.
- The library owns each successful `oopdbms_result`; release it with
  `oopdbms_result_free`.
- Result output is valid until its result is freed.
- `oopdbms_close(NULL)` and `oopdbms_result_free(NULL)` are safe.

## Constraints

- The current build targets POSIX C99 systems.
- A database handle is not safe for simultaneous use from multiple threads.
- Calls to `oopdbms_execute` must be serialized within a process because the
  embedded engine captures command output through the process stdout stream.
- There is no server, connection string, or network listener: the `path`
  passed to `oopdbms_open` is the database.

## Development

```sh
make          # Build liboopdbmsc.a and liboopdbmsc.so
make test     # Run the API suite against both artifacts
make clean    # Remove generated root build artifacts
```

The build discovers `oopdbms-engine` with pkg-config and compiles only the
adapter in this repository. Updating OOPDBMS no longer requires copying its
source into `oopdbmsc`; install the desired OOPDBMS release and rebuild this
library against its engine package.
