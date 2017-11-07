#include "comitoz.smallsh.h"
#include "comitoz.utils.h"

#include <stdlib.h>    // malloc, realloc, free
#include <stdio.h>     // getline
#include <string.h>    // strtok, strcmp, strcpy, strlen
#include <sys/types.h> // pid_t
#include <unistd.h>    // getpid


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
        printf("arg%d: %s\n", i, args[i]);
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
        write_stdout(": ", 2);
        
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
    return main_loop();
}
