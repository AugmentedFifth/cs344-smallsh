#pragma once

#include <errno.h>  // errno
#include <stdlib.h> // calloc, realloc
#include <stdio.h>  // fflush, ferror
#include <string.h> // strtok, strlen, strcpy, strcmp
#include <unistd.h> // write, STDOUT_FILENO


// `typedef`s
typedef enum {false, true} bool;


// Prints to stdout, in a reentrant way.
// Returns non-zero on failure.
int write_stdout(const char* str, size_t size)
{
    if (write(STDOUT_FILENO, str, size) < 0)
    {
        return errno;
    }

    return 0;
}

// Same thing as `write_stdout()`, but also flushes `stdout`.
int fwrite_stdout(const char* str, size_t size)
{
    int r = write_stdout(str, size);
    if (r != 0)
    {
        return r;
    }

    if (fflush(stdout) != 0)
    {
        return ferror(stdout);
    }

    return 0;
}

// Concatenates two paths, the first one absolute and the second one
// relative, yielding an absolute path. Call must `free()` the returned
// buffer.
//
// Returns `NULL` on error.
char* path_cat(char* abs, char* rel)
{
    int buf_str_len = strlen(abs);
    int buf_size = 2 * buf_str_len + 1;
    char* buf = calloc(buf_size, sizeof(char));
    strcpy(buf, abs);

    char* token = strtok(rel, "/");
    while (token != NULL)
    {
        if (strcmp(token, "..") == 0)
        {
            while (buf[buf_str_len - 1] != '/')
            {
                buf[buf_str_len - 1] = '\0';
                buf_str_len--;
            }

            if (buf_str_len > 1)
            {
                buf[buf_str_len - 1] = '\0';
                buf_str_len--;
            }
        }
        else if (strcmp(token, ".") != 0)
        {
            int token_size = strlen(token);
            if (buf_str_len + token_size + 1 > buf_size)
            {
                buf_size += 2 * token_size + 1;
                buf = realloc(buf, buf_size * sizeof(char));
            }

            strcat(buf, "/");
            strcat(buf, token);
        }

        token = strtok(NULL, "/");
    }

    return buf;
}

