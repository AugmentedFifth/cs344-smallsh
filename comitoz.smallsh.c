#include "comitoz.smallsh.h"
#include "comitoz.utils.h"

#include <fcntl.h>     // open, close
#include <stdlib.h>    // malloc, realloc, free, getenv
#include <stdio.h>     // getline, perror
#include <string.h>    // memmove, strtok, strcmp, strcpy, strlen
#include <sys/stat.h>  // stat
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid
#include <unistd.h>    // chdir, getpid, fork, exec, dup2, etc.


// Globals
int status = 0;
bool status_is_term = false;

char* cwd;
bool free_cwd = false;

pid_t* children;
int child_count = 0;
int child_capacity;


// Handles `fork()`ing and `exec()`ing commands.
int exec_command(const char*  command,
                 char* const* args,
                 const char*  input_file,
                 const char*  output_file,
                 bool         background)
{
    pid_t spawned_pid = fork();
    switch (spawned_pid)
    {
        case -1:
        {
            perror("fork() failed!");
            status = 1;
            status_is_term = false;
            return 1;
        }
        case 0:  // In the child process
        {
            int input_fd  = -1;
            int output_fd = -1;
            // Redirect inputs and outputs as necessary
            if (input_file != NULL || background)
            {
                input_fd = open(
                    input_file != NULL ? input_file : "/dev/null",
                    O_RDONLY | O_CLOEXEC
                );
                if (input_fd == -1)
                {
                    write_stderr("Could not open ", 15);
                    write_stderr(input_file, strlen(input_file));
                    fwrite_stderr(" for reading\n", 13);
                    status = 1;
                    status_is_term = false;
                    exit(errno);
                    break;
                }

                if (dup2(input_fd, STDIN_FILENO) == -1)
                {
                    perror("dup2() failed!");
                    close(input_fd);
                    status = 1;
                    status_is_term = false;
                    exit(errno);
                    break;
                }
            }
            if (output_file != NULL || background)
            {
                output_fd = open(
                    output_file != NULL ? output_file : "/dev/null",
                    O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC
                );
                if (output_fd == -1)
                {
                    write_stderr("Could not open ", 15);
                    write_stderr(output_file, strlen(output_file));
                    fwrite_stderr(" for writing\n", 13);
                    status = 1;
                    status_is_term = false;
                    exit(errno);
                    break;
                }

                if (dup2(output_fd, STDOUT_FILENO) == -1)
                {
                    perror("dup2() failed!");
                    close(output_fd);
                    if (input_fd != -1)
                    {
                        close(input_fd);
                    }
                    status = 1;
                    status_is_term = false;
                    exit(errno);
                    break;
                }
            }

            // `exec()` away
            if (execvp(command, args) == -1)
            {
                // TODO: iunno
                write_stderr("Could not exec ", 15);
                write_stderr(command, strlen(command));
                fwrite_stderr("\n", 1);
                // Close files so we don't accidentally create random
                // empty sticky-bit files, which no one wants
                if (input_fd != -1)
                {
                    close(input_fd);
                }
                if (output_fd != -1)
                {
                    close(output_fd);
                }
                status = 1;
                status_is_term = false;
                exit(errno);
                break;
            }

            break;
        }
        default: // In the parent process
        {
            if (background)
            {
                write_stdout("Started background process ", 27);
                char num_str[12];
                sprintf(num_str, "%d\n", spawned_pid);
                fwrite_stdout(num_str, strlen(num_str));

                if (child_count >= child_capacity)
                {
                    child_capacity *= 2;
                    children = realloc(
                        children,
                        child_capacity * sizeof(pid_t)
                    );
                }
                children[child_count] = spawned_pid;
            }
            else
            {
                waitpid(spawned_pid, &status, 0);
            }

            break;
        }
    }

    return 0;
}

int handle_bg_processes(void)
{
    int i;
    for (i = 0; i < child_count; ++i)
    {
        int exit_status;
        pid_t waited = waitpid(children[i], &exit_status, WNOHANG);
        switch (waited)
        {
            case -1:
            {
                perror("waitpid() failed!");
                return 1;
            }
            case 0:
            {
                break;
            }
            default:
            {
                write_stdout("Background process ", 19);
                char num_str[64];
                sprintf(
                    num_str,
                    "%d terminated with exit code %d\n",
                    children[1],
                    exit_status
                );
                fwrite_stdout(num_str, strlen(num_str));

                int trailing_entries = child_count - i - 1;
                if (trailing_entries > 0)
                {
                    memmove(
                        &children[i],
                        &children[i + 1],
                        trailing_entries * sizeof(pid_t)
                    );
                }

                child_count--;
                i--;

                break;
            }
        }
    }

    return 0;
}

// Parses a command and redirects its content to the corresponding
// behavior.
int process_command(char* line)
{
    char* token = strtok(line, " \n");
    // Handle blank lines and comments
    if (token == NULL || token[0] == '#')
    {
        return 0;
    }

    // State management for the parsing
    char* command = NULL;
    char* args[514];
    int argc = 1;
    char* input_file = NULL;
    bool looking_for_input = false;
    char* output_file = NULL;
    bool looking_for_output = false;
    bool background = false;

    // Parse command, one token at a time
    while (token != NULL)
    {
        background = false;

        if (strcmp(token, "<") == 0)
        {
            looking_for_input = true;
        }
        else if (strcmp(token, ">") == 0)
        {
            looking_for_output = true;
        }
        else if (strcmp(token, "&") == 0)
        {
            background = true;
        }
        else if (looking_for_input)
        {
            input_file = token;
            looking_for_input = false;
        }
        else if (looking_for_output)
        {
            output_file = token;
            looking_for_output = false;
        }
        else if (command == NULL)
        {
            command = token;//malloc((strlen(token) + 1) * sizeof(char));
            //strcpy(command, token);
        }
        else
        {
            args[argc] = token;
            argc++;
        }

        token = strtok(NULL, " \n");
    }

    args[0] = command;
    args[argc] = NULL;

    if (command != NULL)
    {
        printf("command: %s\n", command);
    }
    int i;
    for (i = 0; i < argc; ++i)
    {
        printf("  arg%d: %s\n", i, args[i]);
    }
    printf("  last arg is NULL: %s\n", args[i] == NULL ? "true" : "false");
    if (input_file != NULL)
    {
        printf("input file: %s\n", input_file);
    }
    if (output_file != NULL)
    {
        printf("output file: %s\n", output_file);
    }
    printf("background: %s\n", background ? "true" : "false");
    fflush(stdout);

    // Start doing stuff based on the parsed command, builtins first.
    if (strcmp(command, "exit") == 0)
    {
        // TODO: Kill all processes and jobs here

        return -1;
    }
    else if (strcmp(command, "cd") == 0)
    {
        printf("%s\n", cwd);
        if (argc < 2)
        {
            if (free_cwd)
            {
                free(cwd);
                free_cwd = false;
            }
            cwd = getenv("HOME");

            chdir(cwd);
        }
        else if (args[1][0] == '/')
        {
            struct stat s;
            if (stat(args[1], &s) == -1 || !S_ISDIR(s.st_mode))
            {
                write_stdout("could not cd to ", 16);
                write_stdout(args[1], strlen(args[1]));
                fwrite_stdout("\n", 1);

                return 0;
            }

            if (free_cwd)
            {
                free(cwd);
            }
            cwd = malloc((strlen(args[1]) + 1) * sizeof(char));
            free_cwd = true;
            strcpy(cwd, args[1]);

            chdir(cwd);
        }
        else
        {
            char* new_cwd = path_cat(cwd, args[1]);

            struct stat s;
            if (stat(new_cwd, &s) == -1 || !S_ISDIR(s.st_mode))
            {
                write_stdout("could not cd to ", 16);
                write_stdout(new_cwd, strlen(new_cwd));
                fwrite_stdout("\n", 1);

                free(new_cwd);

                return 0;
            }

            if (free_cwd)
            {
                free(cwd);
            }
            free_cwd = true;
            cwd = new_cwd;

            chdir(cwd);
        }
    }
    else if (strcmp(command, "status") == 0)
    {
        if (status_is_term)
        {
            write_stdout("terminated by signal ", 21);
        }
        else
        {
            write_stdout("exit status ", 12);
        }

        char num_str[12];
        sprintf(num_str, "%d\n", status);

        fwrite_stdout(num_str, strlen(num_str));
    }
    else
    {
        return exec_command(
            command,
            args,
            input_file,
            output_file,
            background
        );
    }

    return 0;
}

// Enters the main loop, spitting out a prompt and waiting for commands,
// forking and executing external commands by calls to other functions.
int main_loop(void)
{
    // `getline` stuff
    char* line = NULL;
    size_t getline_buffer_size = 0;
    ssize_t chars_read;

    int command_result;

    do
    {
        int bg_res = handle_bg_processes();
        if (bg_res != 0)
        {
            if (line != NULL)
            {
                free(line);
            }

            return bg_res;
        }

        fwrite_stdout(": ", 2);

        while ((chars_read = getline(&line, &getline_buffer_size, stdin)) == -1)
        {
            clearerr(stdin);
        }

        if ((command_result = process_command(line)) != 0)
        {
            if (line != NULL)
            {
                free(line);
            }

            return command_result;
        }
    } while (1);

    if (line != NULL)
    {
        free(line);
    }

    return 0;
}

int main(void)
{
    cwd = getenv("HOME");
    chdir(cwd);

    child_capacity = 12;
    children = malloc(child_capacity * sizeof(pid_t));

    int ret = main_loop();

    // Cleanup
    free(children);
    if (free_cwd)
    {
        free(cwd);
    }

    return ret;
}
