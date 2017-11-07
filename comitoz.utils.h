#include <errno.h>  // errno
#include <stdio.h>  // fflush, ferror
#include <unistd.h> // write, STDOUT_FILENO


// `typedef`s
typedef enum {false, true} bool;


// Prints to stdout AND flushes the buffer, in a reentrant way.
// Returns non-zero on failure.
int write_stdout(const char* str, size_t size)
{
    if (write(STDOUT_FILENO, str, size) < 0)
    {
        return errno;
    }

    if (fflush(stdout) != 0)
    {
        return ferror(stdout);
    }

    return 0;
}
