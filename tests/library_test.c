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

static int expect_status(oopdbms_status actual, oopdbms_status expected,
                         const char* operation) {
        if (actual == expected) {
                return 1;
        }
        fprintf(stderr, "%s: expected %s, received %s\n", operation,
                oopdbms_status_string(expected), oopdbms_status_string(actual));
        return 0;
}

static int check_result_cleared(oopdbms_result* result, const char* operation) {
        if (result == NULL) {
                return 1;
        }
        fprintf(stderr, "%s: result output was not cleared\n", operation);
        return 0;
}

static int expect_open_failure(const char* path, unsigned int flags,
                               oopdbms_database* existing,
                               oopdbms_status expected, const char* operation) {
        oopdbms_database* output = existing;

        if (!expect_status(oopdbms_open(path, flags, &output), expected,
                           operation)) {
                return 0;
        }
        if (output != NULL) {
                fprintf(stderr, "%s: output handle was not cleared\n",
                        operation);
                return 0;
        }
        return 1;
}

static int test_open_failures(oopdbms_database* existing) {
        char path[] = "/tmp/oopdbmsc-open-test-XXXXXX";
        FILE* file;
        int fd;
        int ok = 1;

        fd = mkstemp(path);
        if (fd < 0 || close(fd) != 0 || unlink(path) != 0) {
                fprintf(stderr, "could not prepare open-failure test path\n");
                return 0;
        }
        ok =
            expect_open_failure(NULL, 0, existing,
                                OOPDBMS_ERROR_INVALID_ARGUMENT, "null path") &&
            expect_open_failure("", 0, existing, OOPDBMS_ERROR_INVALID_ARGUMENT,
                                "empty path") &&
            expect_open_failure(path, 4, existing,
                                OOPDBMS_ERROR_INVALID_ARGUMENT,
                                "unknown open flag") &&
            expect_open_failure(
                path, OOPDBMS_OPEN_CREATE | OOPDBMS_OPEN_READ_ONLY, existing,
                OOPDBMS_ERROR_INVALID_ARGUMENT, "conflicting open flags") &&
            expect_open_failure(path, 0, existing, OOPDBMS_ERROR_NOT_FOUND,
                                "missing database");
        if (oopdbms_open(path, 0, NULL) != OOPDBMS_ERROR_INVALID_ARGUMENT) {
                fprintf(stderr, "null output handle was not rejected\n");
                ok = 0;
        }

        file = fopen(path, "wb");
        if (file == NULL) {
                unlink(path);
                fprintf(stderr, "could not prepare malformed database test\n");
                return 0;
        }
        if (fwrite("bad", 1, 3, file) != 3) {
                fclose(file);
                unlink(path);
                fprintf(stderr, "could not write malformed database fixture\n");
                return 0;
        }
        if (fclose(file) != 0) {
                unlink(path);
                fprintf(stderr, "could not close malformed database fixture\n");
                return 0;
        }
        if (!expect_open_failure(path, 0, existing, OOPDBMS_ERROR_DATABASE,
                                 "malformed database")) {
                ok = 0;
        }
        if (unlink(path) != 0) {
                fprintf(stderr,
                        "could not remove malformed database fixture\n");
                ok = 0;
        }
        return ok;
}

static int test_execute_failures(oopdbms_database* database) {
        oopdbms_result* result = NULL;
        oopdbms_result* output;
        int ok = 1;

        if (!check_status(oopdbms_execute(database, "LIST CLASSES", &result),
                          "prepare execute-failure test")) {
                return 0;
        }
        output = result;
        ok = expect_status(oopdbms_execute(NULL, "LIST CLASSES", &output),
                           OOPDBMS_ERROR_INVALID_ARGUMENT, "null database") &&
             check_result_cleared(output, "null database") && ok;
        output = result;
        ok = expect_status(oopdbms_execute(database, NULL, &output),
                           OOPDBMS_ERROR_INVALID_ARGUMENT, "null command") &&
             check_result_cleared(output, "null command") && ok;
        ok = expect_status(oopdbms_execute(database, "LIST CLASSES", NULL),
                           OOPDBMS_ERROR_INVALID_ARGUMENT,
                           "null result output") &&
             ok;
        oopdbms_result_free(result);
        return ok;
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
        if (!test_open_failures(database) || !test_execute_failures(database)) {
                oopdbms_close(database);
                unlink(path);
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
