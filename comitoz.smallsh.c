#include "comitoz.smallsh.h"
#include "comitoz.utils.h"

#include <fcntl.h>     // open, close
#include <stdlib.h>    // malloc, realloc, free, getenv
#include <stdio.h>     // getline, perror
#include <string.h>    // strtok, strcmp, strcpy, strlen
#include <sys/stat.h>  // stat
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid
#include <unistd.h>    // getpid, fork, exec, dup2, etc.


// Globals
int status = 0;
bool status_is_term = false;
char* cwd;
bool free_cwd = false;


// Handles `fork()`ing and `exec()`ing commands.
int exec_command(const char*  command,
                 const char** args,
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
            // TODO: Cleanup here too.
            exit(1);
            break;
        }
        case 0:  // In the child process
        {
            int input_fd  = -1;
            int output_fd = -1;
            // Redirect inputs and outputs as necessary
            if (input_file != NULL)
            {
                input_fd = open(input_file, O_RDONLY);
                if (input_fd == -1)
                {
                    write_stderr("Could not open ", 15);
                    write_stderr(input_file, strlen(input_file));
                    fwrite_stderr(" for reading\n", 13);
                    exit(errno);
                    break;
                }

                if (dup2(input_fd, STDIN_FILENO) == -1)
                {
                    perror("dup2() failed!");
                    close(input_fd);
                    exit(errno);
                    break;
                }
            }
            if (output_file != NULL)
            {
                output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC);
                if (output_fd == -1)
                {
                    write_stderr("Could not open ", 15);
                    write_stderr(output_file, strlen(output_file));
                    fwrite_stderr(" for writing\n", 13);
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
                exit(errno);
            }

            break;
        }
        default: // In the parent process
        {
            if (background)
            {
                
            }
            else
            {
                int spawned_exit_status;
                waitpid(spawned_pid, &spawned_exit_status, 0);
            }
                
            break;
        }
    }
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
    if (input_file != NULL)
    {
        printf("input file: %s\n", input_file);
    }
    if (output_file != NULL)
    {
        printf("output file: %s\n", output_file);
    }
    printf("background: %s\n", background ? "true" : "false");

    // Start doing stuff based on the parsed command, builtins first.
    if (strcmp(command, "exit") == 0)
    {
        // TODO: Kill all processes and jobs here

        return -1;
    }
    else if (strcmp(command, "cd") == 0)
    {
        printf("%s\n", cwd);
        if (argc == 0)
        {
            if (free_cwd)
            {
                free(cwd);
                free_cwd = false;
            }
            cwd = getenv("HOME");
        }
        else if (args[0][0] == '/')
        {
            struct stat s;
            if (stat(args[0], &s) == -1 || !S_ISDIR(s.st_mode))
            {
                write_stdout("could not cd to ", 16);
                write_stdout(args[0], strlen(args[0]));
                fwrite_stdout("\n", 1);

                return 0;
            }

            if (free_cwd)
            {
                free(cwd);
            }
            cwd = malloc((strlen(args[0]) + 1) * sizeof(char));
            free_cwd = true;
            strcpy(cwd, args[0]);
        }
        else
        {
            char* new_cwd = path_cat(cwd, args[0]);

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
        exec_command(command, args, input_file, output_file, background);
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
        fwrite_stdout(": ", 2);

        while ((chars_read = getline(&line, &getline_buffer_size, stdin)) == -1)
        {
            clearerr(stdin);
        }

        if ((command_result = process_command(line)) != 0)
        {
            return command_result;
        }
    } while (1);

    free(line);

    return 0;
}

int main(void)
{
    cwd = getenv("HOME");

    int ret = main_loop();

    // Cleanup
    if (free_cwd)
    {
        free(cwd);
    }

    return ret;
}

