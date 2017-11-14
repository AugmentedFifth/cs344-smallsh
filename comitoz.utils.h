#pragma once

#include <errno.h>  // errno
#include <stdlib.h> // calloc, realloc
#include <stdio.h>  // fflush, ferror
#include <string.h> // strlen, strncat, strstr
#include <unistd.h> // getpid, STDOUT_FILENO, STDERR_FILENO, write


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

// Self-explanatory; same thing as `write_stdout()` but for stderr
int write_stderr(const char* str, size_t size)
{
    if (write(STDERR_FILENO, str, size) < 0)
    {
        return errno;
    }

    return 0;
}

// Self-explanatory; same thing as `fwrite_stdout()` but for stderr
int fwrite_stderr(const char* str, size_t size)
{
    int r = write_stderr(str, size);
    if (r != 0)
    {
        return r;
    }

    if (fflush(stderr) != 0)
    {
        return ferror(stderr);
    }

    return 0;
}

// Expand all instances of "$$" into the current PID, returning a pointer to
// a new string that must be `free()`d by the caller.
char* expand_pid(char* str)
{
    int str_len = strlen(str);

    int buf_cap = str_len + 1;
    char* buf = calloc(buf_cap, sizeof(char));
    int buf_len = 0;

    char pid_str[12];
    sprintf(pid_str, "%d", getpid());
    int pid_str_len = strlen(pid_str);

    char* substr_loc;
    char* str_pos = str;
    while ((substr_loc = strstr(str_pos, "$$")) != NULL)
    {
        int copy_size = substr_loc - str_pos;
        buf_len += copy_size + pid_str_len;
        if (buf_cap < buf_len + 1)
        {
            buf_cap = buf_len + 1;
            buf = realloc(buf, buf_cap * sizeof(char));
        }

        strncat(buf, str_pos, copy_size);
        strncat(buf, pid_str, pid_str_len);

        str_pos = substr_loc + strlen("$$");
    }

    buf_len += strlen(str_pos);
    if (buf_cap < buf_len + 1)
    {
        buf_cap = buf_len + 1;
        buf = realloc(buf, buf_cap * sizeof(char));
    }
    strcat(buf, str_pos);

    return buf;
}
