#include <errno.h>
#include <oopdbms/oopdbms.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error/analyzer/main.h"
#include "error/executor/executor_error.h"
#include "error/tokenizer/main.h"
#include "interface/analyzer/main.h"
#include "interface/analyzer/statement.h"
#include "interface/executor/executor.h"
#include "interface/storage/database.h"
#include "interface/tokenizer/main.h"

struct oopdbms_database {
        struct database database;
        char* path;
};

struct oopdbms_result {
        enum oopdbms_result_kind kind;
        char* output;
        size_t output_size;
};

static char* copy_string(const char* source) {
        size_t length;
        char* copy;

        if (source == NULL) {
                return NULL;
        }
        length = strlen(source);
        copy   = malloc(length + 1);
        if (copy != NULL) {
                memcpy(copy, source, length + 1);
        }
        return copy;
}

static enum oopdbms_result_kind result_kind(
    const struct analyzer_statement* statement) {
        switch (statement->kind) {
                case ANALYZER_NEW:
                case ANALYZER_FIND:
                        return OOPDBMS_RESULT_OBJECTS;
                case ANALYZER_CALL:
                        return OOPDBMS_RESULT_VALUE;
                case ANALYZER_LIST_CLASSES:
                case ANALYZER_LIST_INTERFACES:
                case ANALYZER_DESCRIBE:
                        return OOPDBMS_RESULT_METADATA;
                default:
                        return OOPDBMS_RESULT_NONE;
        }
}

static oopdbms_status capture_begin(FILE** capture) {
        *capture = tmpfile();
        return *capture == NULL ? OOPDBMS_ERROR_IO : OOPDBMS_OK;
}

static oopdbms_status capture_end(FILE* capture, char** output,
                                  size_t* output_size) {
        long length;
        size_t read_count;
        bool read_failed;
        int close_result;

        if (fflush(capture) != 0 || fseek(capture, 0, SEEK_END) != 0 ||
            (length = ftell(capture)) < 0 || fseek(capture, 0, SEEK_SET) != 0) {
                fclose(capture);
                return OOPDBMS_ERROR_IO;
        }
        *output = malloc((size_t)length + 1);
        if (*output == NULL) {
                fclose(capture);
                return OOPDBMS_ERROR_OUT_OF_MEMORY;
        }
        read_count   = fread(*output, 1, (size_t)length, capture);
        read_failed  = read_count != (size_t)length || ferror(capture);
        close_result = fclose(capture);
        if (read_failed || close_result != 0) {
                free(*output);
                *output = NULL;
                return OOPDBMS_ERROR_IO;
        }
        (*output)[length] = '\0';
        *output_size      = (size_t)length;
        return OOPDBMS_OK;
}

oopdbms_status oopdbms_open(const char* path, unsigned int flags,
                            oopdbms_database** out_database) {
        oopdbms_database* handle;
        bool exists;

        if (out_database == NULL) {
                return OOPDBMS_ERROR_INVALID_ARGUMENT;
        }
        *out_database = NULL;
        if (path == NULL || path[0] == '\0' ||
            (flags & ~(OOPDBMS_OPEN_CREATE | OOPDBMS_OPEN_READ_ONLY)) != 0 ||
            ((flags & OOPDBMS_OPEN_CREATE) != 0 &&
             (flags & OOPDBMS_OPEN_READ_ONLY) != 0)) {
                return OOPDBMS_ERROR_INVALID_ARGUMENT;
        }
        exists = access(path, F_OK) == 0;
        if (!exists && errno != ENOENT) {
                return OOPDBMS_ERROR_IO;
        }
        if (!exists && (flags & OOPDBMS_OPEN_CREATE) == 0) {
                return OOPDBMS_ERROR_NOT_FOUND;
        }
        handle = calloc(1, sizeof *handle);
        if (handle == NULL) {
                return OOPDBMS_ERROR_OUT_OF_MEMORY;
        }
        handle->path = copy_string(path);
        if (handle->path == NULL) {
                free(handle);
                return OOPDBMS_ERROR_OUT_OF_MEMORY;
        }
        if (!database_init_format_mode(&handle->database, handle->path, !exists,
                                       (flags & OOPDBMS_OPEN_READ_ONLY) != 0)) {
                free(handle->path);
                free(handle);
                return exists ? OOPDBMS_ERROR_DATABASE : OOPDBMS_ERROR_IO;
        }
        *out_database = handle;
        return OOPDBMS_OK;
}

void oopdbms_close(oopdbms_database* database) {
        if (database == NULL) {
                return;
        }
        database_close(&database->database);
        free(database->path);
        free(database);
}

oopdbms_status oopdbms_execute(oopdbms_database* database, const char* command,
                               oopdbms_result** out_result) {
        char* arguments[2];
        struct tokenizer_error tokenizer_error;
        struct analyzer_error analyzer_error;
        struct tokenizer_tokens tokens;
        struct analyzer_statement statement;
        enum EXECUTOR_RESULT execution;
        oopdbms_result* result;
        oopdbms_status status;
        char* output       = NULL;
        size_t output_size = 0;
        FILE* capture;

        if (out_result == NULL) {
                return OOPDBMS_ERROR_INVALID_ARGUMENT;
        }
        *out_result = NULL;
        if (database == NULL || command == NULL) {
                return OOPDBMS_ERROR_INVALID_ARGUMENT;
        }
        arguments[0] = "oopdbmsc";
        arguments[1] = (char*)command;
        tokens       = i_tokenize(2, arguments, &tokenizer_error);
        if (tokenizer_error.type != TOKENIZER_OK) {
                return tokenizer_error.type == TOKENIZER_ERROR_OUT_OF_MEMORY
                           ? OOPDBMS_ERROR_OUT_OF_MEMORY
                           : OOPDBMS_ERROR_TOKENIZE;
        }
        statement = i_analyze(&tokens, &analyzer_error);
        i_tokenizer_free(&tokens);
        if (analyzer_error.type != ANALYZER_OK) {
                return analyzer_error.type == ANALYZER_ERROR_OUT_OF_MEMORY
                           ? OOPDBMS_ERROR_OUT_OF_MEMORY
                           : OOPDBMS_ERROR_PARSE;
        }
        status = capture_begin(&capture);
        if (status != OOPDBMS_OK) {
                i_analyzer_free(&statement);
                return status;
        }
        execution = i_execute_to(&database->database, &statement, capture);
        status    = capture_end(capture, &output, &output_size);
        if (status != OOPDBMS_OK || execution != EXECUTOR_OK) {
                free(output);
                i_analyzer_free(&statement);
                return status != OOPDBMS_OK
                           ? status
                           : (execution == EXECUTOR_ERROR_OUTPUT_FAILED
                                  ? OOPDBMS_ERROR_IO
                                  : OOPDBMS_ERROR_EXECUTE);
        }
        result = calloc(1, sizeof *result);
        if (result == NULL) {
                free(output);
                i_analyzer_free(&statement);
                return OOPDBMS_ERROR_OUT_OF_MEMORY;
        }
        result->kind        = result_kind(&statement);
        result->output      = output;
        result->output_size = output_size;
        i_analyzer_free(&statement);
        *out_result = result;
        return OOPDBMS_OK;
}

const char* oopdbms_status_string(oopdbms_status status) {
        static const char* const messages[] = {"success",
                                               "invalid argument",
                                               "out of memory",
                                               "not found",
                                               "I/O error",
                                               "invalid database",
                                               "tokenization error",
                                               "parse error",
                                               "execution error"};

        if (status < OOPDBMS_OK || status > OOPDBMS_ERROR_EXECUTE) {
                return "unknown status";
        }
        return messages[status];
}

enum oopdbms_result_kind oopdbms_result_kind_get(const oopdbms_result* result) {
        return result == NULL ? OOPDBMS_RESULT_NONE : result->kind;
}

const char* oopdbms_result_output(const oopdbms_result* result) {
        return result == NULL ? NULL : result->output;
}

size_t oopdbms_result_output_size(const oopdbms_result* result) {
        return result == NULL ? 0 : result->output_size;
}

void oopdbms_result_free(oopdbms_result* result) {
        if (result == NULL) {
                return;
        }
        free(result->output);
        free(result);
}
