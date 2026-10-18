# oopdbmsc

`oopdbmsc` is an embedded C99 library for the
[OOPDBMS](https://github.com/KrzysztofMarciniak/oopdbms) object-oriented
database format and command language. It links the database engine directly;
it does not start the `oopdbms` executable.

The public API is in [`include/oopdbms/oopdbms.h`](include/oopdbms/oopdbms.h).
It opens a database, executes any documented OOPDBMS command as command text,
and returns the command output in an owned result object.

```c
#include <oopdbms/oopdbms.h>
#include <stdio.h>

int main(void) {
        oopdbms_database *database;
        oopdbms_result *result;

        if (oopdbms_open("example.oopdb", OOPDBMS_OPEN_CREATE, &database) !=
            OOPDBMS_OK) {
                return 1;
        }
        if (oopdbms_execute(database,
                            "CREATE CLASS Person (name text, age integer)",
                            &result) == OOPDBMS_OK) {
                printf("%s", oopdbms_result_output(result));
                oopdbms_result_free(result);
        }
        oopdbms_close(database);
        return 0;
}
```

Build both library artifacts and run the API tests:

```sh
make
make test
```

Run a complete, fresh database walkthrough:

```sh
make quick_start
```

This compiles and runs [`examples/main.c`](examples/main.c), recreating
`build/quick-start.oopdb` each time. Use another path with
`make quick_start QUICK_START_DB=/path/to/demo.oopdb`.
The walkthrough prints the corresponding public API call and explains how to
open a database, define schemas, insert/query/update objects, call a stored
function, inspect metadata, and close the handle. Read the source alongside
the output to adapt the same lifecycle to an application.

Install headers plus `liboopdbmsc.a` and `liboopdbmsc.so`:

```sh
make install PREFIX=/usr/local
```

The embedded engine is vendored at
[`vendor/oopdbms`](vendor/oopdbms) from upstream commit
`f067881eb52b7abfafa474b6edad908b927b11f0`. It remains covered by its
BSD-3-Clause license; see [`vendor/oopdbms/LICENSE`](vendor/oopdbms/LICENSE)
and [`vendor/oopdbms/UPSTREAM.md`](vendor/oopdbms/UPSTREAM.md).
