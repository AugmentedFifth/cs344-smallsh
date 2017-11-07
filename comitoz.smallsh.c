#include "comitoz.smallsh.h"
#include "comitoz.utils.h"

#include <stdlib.h>    // malloc, realloc, free, getenv
#include <stdio.h>     // getline
#include <string.h>    // strtok, strcmp, strcpy, strlen
#include <sys/stat.h>  // stat
#include <sys/types.h> // pid_t
#include <unistd.h>    // getpid


// Globals
int status = 0;
bool status_is_term = false;
char* cwd;
bool free_cwd = false;


//
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
    char* args[512];
    int argc = 0;
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
        //
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
