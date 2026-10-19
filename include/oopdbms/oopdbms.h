#ifndef OOPDBMS_OOPDBMS_H
#define OOPDBMS_OOPDBMS_H

#include <stddef.h>

#if defined(_WIN32) && defined(OOPDBMS_BUILDING_LIBRARY)
#define OOPDBMS_API __declspec(dllexport)
#elif defined(_WIN32)
#define OOPDBMS_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define OOPDBMS_API __attribute__((visibility("default")))
#else
#define OOPDBMS_API
#endif

#define OOPDBMS_VERSION_MAJOR 0
#define OOPDBMS_VERSION_MINOR 1
#define OOPDBMS_VERSION_PATCH 0

/**
 * Opaque database handle. Each handle owns one open database file.
 *
 * Handles are not safe for simultaneous use by multiple threads. A process
 * must also serialize calls to oopdbms_execute because result output is
 * captured from the embedded engine's stdout stream.
 */
typedef struct oopdbms_database oopdbms_database;

/** Owned output produced by a successfully executed command. */
typedef struct oopdbms_result oopdbms_result;

/** API operation status. */
typedef enum oopdbms_status {
        OOPDBMS_OK = 0,
        OOPDBMS_ERROR_INVALID_ARGUMENT,
        OOPDBMS_ERROR_OUT_OF_MEMORY,
        OOPDBMS_ERROR_NOT_FOUND,
        OOPDBMS_ERROR_IO,
        OOPDBMS_ERROR_DATABASE,
        OOPDBMS_ERROR_TOKENIZE,
        OOPDBMS_ERROR_PARSE,
        OOPDBMS_ERROR_EXECUTE
} oopdbms_status;

/** Open behavior flags. */
typedef enum oopdbms_open_flags {
        /** Create and initialize the database when path does not exist. */
        OOPDBMS_OPEN_CREATE = 1u << 0,
        /** Reject mutating commands and open the database read-only. */
        OOPDBMS_OPEN_READ_ONLY = 1u << 1
} oopdbms_open_flags;

/** Category of command output. */
typedef enum oopdbms_result_kind {
        OOPDBMS_RESULT_NONE = 0,
        OOPDBMS_RESULT_OBJECTS,
        OOPDBMS_RESULT_VALUE,
        OOPDBMS_RESULT_METADATA
} oopdbms_result_kind;

/**
 * Open an OOPDBMS database.
 *
 * `OOPDBMS_OPEN_CREATE` creates a new database only if `path` does not
 * already exist. `OOPDBMS_OPEN_READ_ONLY` cannot be combined with CREATE.
 * On success, the caller owns `*out_database` and must close it.
 */
OOPDBMS_API oopdbms_status oopdbms_open(const char* path, unsigned int flags,
                                        oopdbms_database** out_database);

/** Close a database and release its resources. Accepts NULL. */
OOPDBMS_API void oopdbms_close(oopdbms_database* database);

/**
 * Execute one complete documented OOPDBMS command.
 *
 * The command string is parsed with the same quoting and syntax rules as the
 * upstream command-line client. On success, the caller owns `*out_result` and
 * must release it with oopdbms_result_free(). The output is empty for commands
 * that do not print a value.
 */
OOPDBMS_API oopdbms_status oopdbms_execute(oopdbms_database* database,
                                           const char* command,
                                           oopdbms_result** out_result);

/** Return a stable, non-NULL explanation for an API status. */
OOPDBMS_API const char* oopdbms_status_string(oopdbms_status status);

/** Return the category of output in a result. */
OOPDBMS_API oopdbms_result_kind
oopdbms_result_kind_get(const oopdbms_result* result);

/** Return the NUL-terminated command output, or NULL for a NULL result. */
OOPDBMS_API const char* oopdbms_result_output(const oopdbms_result* result);

/** Return the number of output bytes, excluding the terminating NUL. */
OOPDBMS_API size_t oopdbms_result_output_size(const oopdbms_result* result);

/** Release a result returned by oopdbms_execute(). Accepts NULL. */
OOPDBMS_API void oopdbms_result_free(oopdbms_result* result);

#endif
