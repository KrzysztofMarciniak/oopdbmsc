#include <oopdbms/oopdbms.h>

#include <stdio.h>

static int execute(oopdbms_database *database, const char *command) {
        oopdbms_result *result;
        oopdbms_status status;
        const char *output;

        printf("  command = %s\n", command);
        printf("  oopdbms_execute(database, command, &result)\n");
        status = oopdbms_execute(database, command, &result);
        if (status != OOPDBMS_OK) {
                fprintf(stderr, "  error: %s\n", oopdbms_status_string(status));
                return 0;
        }
        output = oopdbms_result_output(result);
        if (output[0] != '\0') {
                printf("%s", output);
                if (output[oopdbms_result_output_size(result) - 1] != '\n') {
                        putchar('\n');
                }
        }
        oopdbms_result_free(result);
        return 1;
}

int main(int argc, char **argv) {
        const char *path;
        oopdbms_database *database;
        oopdbms_status status;
        int success;

        if (argc != 2) {
                fprintf(stderr, "usage: %s DATABASE_PATH\n", argv[0]);
                return 2;
        }
        path = argv[1];
        status = oopdbms_open(path, OOPDBMS_OPEN_CREATE, &database);
        if (status != OOPDBMS_OK) {
                fprintf(stderr, "cannot open %s: %s\n", path,
                        oopdbms_status_string(status));
                return 1;
        }

        printf("OOPDBMS C library quick start\n");
        printf("Database: %s\n", path);

        printf("\n1. Connect to a database\n");
        printf("   oopdbms_open(\"%s\", OOPDBMS_OPEN_CREATE, &database)\n",
               path);
        printf("   The CREATE flag initializes a database when it is absent.\n");

        printf("\n2. Define an interface, classes, and a method\n");
        success =
            execute(database, "CREATE INTERFACE Named (name text)") &&
            execute(database,
                    "CREATE CLASS Person IMPLEMENTS Named (name text, age integer)") &&
            execute(database,
                    "CREATE CLASS Book (title text, price float, author pointer Person)") &&
            execute(database,
                    "CREATE FUNCTION Person.years_until (target integer) RETURNS integer AS target - age") &&
            execute(database, "NEW Person (name = \"Ada Lovelace\", age = 36)") &&
            execute(database,
                    "NEW Book (title = \"Notes on the Analytical Engine\", price = 19.5, author = Person.1)");

        if (success) {
                printf("\n3. Read matching objects and call a method\n");
                success =
                    execute(database, "FIND Book WHERE price < 20") &&
                    execute(database, "CALL Person.years_until (100) ON 1");
        }
        if (success) {
                printf("\n4. Update an object, then read the stored value\n");
                success =
                    execute(database, "SET Book 1 (price = 14.5)") &&
                    execute(database, "FIND Book");
        }
        if (success) {
                printf("\n5. Inspect the schema and database metadata\n");
                success =
                    execute(database, "DESCRIBE Person") &&
                    execute(database, "LIST CLASSES");
        }

        printf("\n6. Close the database\n");
        printf("   oopdbms_close(database)\n");
        oopdbms_close(database);
        if (success) {
                printf("\nDone. Reopen the same path with oopdbms_open() to "
                       "use the persisted data.\n");
        }
        return success ? 0 : 1;
}
