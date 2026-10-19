#include <oopdbms/oopdbms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int check_status(oopdbms_status actual, const char* command) {
        if (actual == OOPDBMS_OK) {
                return 1;
        }
        fprintf(stderr, "%s: %s\n", command, oopdbms_status_string(actual));
        return 0;
}

static int execute(oopdbms_database* database, const char* command,
                   oopdbms_result_kind expected_kind, const char* expected) {
        oopdbms_result* result;
        int ok;

        if (!check_status(oopdbms_execute(database, command, &result),
                          command)) {
                return 0;
        }
        ok = oopdbms_result_kind_get(result) == expected_kind &&
             (expected == NULL ||
              strstr(oopdbms_result_output(result), expected) != NULL);
        if (!ok) {
                fprintf(stderr, "unexpected result for: %s\n", command);
        }
        oopdbms_result_free(result);
        return ok;
}

int main(void) {
        char path[] = "/tmp/oopdbmsc-test-XXXXXX";
        oopdbms_database* database;
        oopdbms_result* result;
        int fd;
        int ok;

        fd = mkstemp(path);
        if (fd < 0 || close(fd) != 0 || unlink(path) != 0) {
                return 1;
        }
        if (!check_status(oopdbms_open(path, OOPDBMS_OPEN_CREATE, &database),
                          "open")) {
                return 1;
        }
        ok =
            execute(database, "CREATE INTERFACE Named (name text)",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "CREATE INTERFACE Disposable (code integer)",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "DROP INTERFACE Disposable", OOPDBMS_RESULT_NONE,
                    NULL) &&
            execute(
                database,
                "CREATE CLASS Person IMPLEMENTS Named (name text, age integer)",
                OOPDBMS_RESULT_NONE, NULL) &&
            execute(database,
                    "CREATE CLASS Book (title text, price float, author "
                    "pointer Person)",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Book ADD FIELD pages integer",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Book RENAME FIELD pages page_count",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Book DROP FIELD page_count",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Book RENAME Novel",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Novel EXTENDS Person",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "ALTER CLASS Novel IMPLEMENTS Named",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database,
                    "CREATE FUNCTION Person.age_plus (years integer) RETURNS "
                    "integer AS age + years",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "NEW Person (name = \"Ada\", age = 36)",
                    OOPDBMS_RESULT_OBJECTS, "Person.1") &&
            execute(database,
                    "NEW Novel (name = \"Book\", age = 1, title = \"Demo\", "
                    "price = 1.5, author = Person.1)",
                    OOPDBMS_RESULT_OBJECTS, "Novel.1") &&
            execute(database, "FIND Person WHERE age >= 36",
                    OOPDBMS_RESULT_OBJECTS, "Person.1") &&
            execute(database, "SET Person 1 (age = 37)", OOPDBMS_RESULT_NONE,
                    NULL) &&
            execute(database, "CALL Person.age_plus (3) ON 1",
                    OOPDBMS_RESULT_VALUE, "40") &&
            execute(database, "LIST CLASSES", OOPDBMS_RESULT_METADATA,
                    "Person") &&
            execute(database, "LIST INTERFACES", OOPDBMS_RESULT_METADATA,
                    "Named") &&
            execute(database, "DESCRIBE Person", OOPDBMS_RESULT_METADATA,
                    "Person") &&
            execute(database, "DELETE Novel 1", OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "DROP FUNCTION Person.age_plus",
                    OOPDBMS_RESULT_NONE, NULL) &&
            execute(database, "DROP CLASS Novel", OOPDBMS_RESULT_NONE, NULL);
        oopdbms_close(database);
        if (!ok) {
                unlink(path);
                return 1;
        }
        if (!check_status(oopdbms_open(path, OOPDBMS_OPEN_READ_ONLY, &database),
                          "reopen read-only")) {
                unlink(path);
                return 1;
        }
        ok = execute(database, "FIND Person WHERE age = 37",
                     OOPDBMS_RESULT_OBJECTS, "Person.1") &&
             oopdbms_execute(database, "NEW Person (name = \"Grace\", age = 1)",
                             &result) == OOPDBMS_ERROR_EXECUTE;
        oopdbms_close(database);
        if (!ok || !check_status(oopdbms_open(path, 0, &database), "reopen")) {
                unlink(path);
                return 1;
        }
        ok = oopdbms_execute(database, "CREATE CLASS", &result) ==
                 OOPDBMS_ERROR_PARSE &&
             execute(database, "FIND Person", OOPDBMS_RESULT_OBJECTS,
                     "Person.1");
        oopdbms_close(database);
        unlink(path);
        return ok ? 0 : 1;
}
