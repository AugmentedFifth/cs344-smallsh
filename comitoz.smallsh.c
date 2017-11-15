#include "comitoz.smallsh.h"
#include "comitoz.utils.h"

#include <fcntl.h>     // open, close
#include <signal.h>    // sigaction, sigfillset, SIG_IGN, SIG_DFL
#include <stdlib.h>    // malloc, realloc, free, getenv
#include <stdio.h>     // getline, perror
#include <string.h>    // memmove, strtok, strcmp, strcpy, strlen
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid
#include <unistd.h>    // chdir, getcwd, getpid, fork, exec, dup2, etc.


// Globals
int status = 0;
bool status_is_term = false;

pid_t* children;
int child_count = 0;
int child_capacity;

bool allow_bg = true;


/*
// Signal handler for `SIGINT`s sent to the main shell.
void SIGINT_main(int signo)
{

}
*/

// Signal handler for `SIGTSTP`s sent to the main shell.
//
// Toggles the ability to background commands and spits out a message to the
// user in lieu of that.
void SIGTSTP_main(int signo)
{
    allow_bg = !allow_bg;

    if (allow_bg)
    {
        fwrite_stdout("Exiting foreground-only mode\n", 29);
    }
    else
    {
        fwrite_stdout("Entering foreground-only mode (& is now ignored)\n", 49);
    }
}

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
            return 1;
        }
        case 0:  // In the child process
        {
            // If this is a foreground process, we want it to accept `SIGINT`s
            // normally instead of ignoring them. If this is a background
            // process then we don't have to do anything since the `SIG_IGN`
            // will get inherited, which is the desired behavior
            if (!background)
            {
                struct sigaction SIGINT_default = {0};

                SIGINT_default.sa_handler = SIG_DFL;

                sigaction(SIGINT, &SIGINT_default, NULL);
            }

            int input_fd  = -1;
            int output_fd = -1;
            // Redirect inputs and outputs as necessary
            if (input_file != NULL || background)
            {
                input_fd = open(
                    input_file != NULL ? input_file : "/dev/null",
                    O_RDONLY
                );
                if (input_fd == -1)
                {
                    write_stderr("cannot open ", 12);
                    write_stderr(input_file, strlen(input_file));
                    fwrite_stderr(" for input\n", 11);
                    if (!background)
                    {
                        status = 1;
                        status_is_term = false;
                    }
                    exit(errno);
                    break;
                }

                if (dup2(input_fd, STDIN_FILENO) == -1)
                {
                    perror("dup2() failed!");
                    if (!background)
                    {
                        status = 1;
                        status_is_term = false;
                    }
                    exit(errno);
                    break;
                }
            }
            if (output_file != NULL || background)
            {
                output_fd = open(
                    output_file != NULL ? output_file : "/dev/null",
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0644
                );
                if (output_fd == -1)
                {
                    write_stderr("Could not open ", 15);
                    write_stderr(output_file, strlen(output_file));
                    fwrite_stderr(" for writing\n", 13);
                    if (!background)
                    {
                        status = 1;
                        status_is_term = false;
                    }
                    exit(errno);
                    break;
                }

                if (dup2(output_fd, STDOUT_FILENO) == -1)
                {
                    perror("dup2() failed!");
                    if (!background)
                    {
                        status = 1;
                        status_is_term = false;
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
                if (!background)
                {
                    status = 1;
                    status_is_term = false;
                }
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
                child_count++;
            }
            else
            {
                int wstatus;
                waitpid(spawned_pid, &wstatus, 0);

                if (WIFEXITED(wstatus)) // Child terminated normally
                {
                    status = WEXITSTATUS(wstatus);
                    status_is_term = false;
                }
                else                    // Child was killed by a signal
                {
                    status = WTERMSIG(wstatus);
                    status_is_term = true;

                    write_stdout("terminated by signal ", 21);
                    char num_str[12];
                    sprintf(num_str, "%d\n", status);
                    fwrite_stdout(num_str, strlen(num_str));
                }
            }

            break;
        }
    }

    return 0;
}

// Checks all currently stored background child processes to see if any of
// them have terminated since we last checked, waiting for them, deregistering
// them from the list of background children, and alerting the user.
int handle_bg_processes(void)
{
    int i;
    for (i = 0; i < child_count; ++i)
    {
        int wstatus;
        pid_t waited = waitpid(children[i], &wstatus, WNOHANG);
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
                // Report dead child process
                write_stdout("background pid ", 15);
                char num_str[64];
                if (WIFEXITED(wstatus)) // Bg process exited normally
                {
                    sprintf(
                        num_str,
                        "%d is done: exit value %d\n",
                        children[i],
                        WEXITSTATUS(wstatus)
                    );
                }
                else                    // Bg process was killed by a signal
                {
                    sprintf(
                        num_str,
                        "%d is done: terminated by signal %d\n",
                        children[i],
                        WTERMSIG(wstatus)
                    );
                }
                fwrite_stdout(num_str, strlen(num_str));

                // Remove dead child PID from `children` array (treating the
                // array like a `std::vector`, but less C++ and more C89)
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
    int ret = 0;

    char* token = strtok(line, " \n");
    // Handle blank lines and comments
    if (token == NULL || token[0] == '#')
    {
        return 0;
    }

    // State management for the parsing
    char* command = NULL;
    char* args[512 + 2];
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
            if (allow_bg)
            {
                background = true;
            }
        }
        else if (looking_for_input)
        {
            input_file = expand_pid(token);
            looking_for_input = false;
        }
        else if (looking_for_output)
        {
            output_file = expand_pid(token);
            looking_for_output = false;
        }
        else if (command == NULL)
        {
            command = expand_pid(token);//malloc((strlen(token) + 1) * sizeof(char));
            //strcpy(command, token);
        }
        else
        {
            args[argc] = expand_pid(token);
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

        ret = -1;
    }
    else if (strcmp(command, "cd") == 0)
    {
        ////////////
        char cwd_buf[1024];
        printf("%s\n", getcwd(cwd_buf, 1024 * sizeof(char)));
        ////////////

        if (argc < 2)
        {
            const char* target = getenv("HOME");
            if (chdir(target) == -1)
            {
                write_stderr("could not cd to ", 16);
                write_stderr(target, strlen(target));
                fwrite_stderr("\n", 1);
            }
        }
        else
        {
            const char* target = args[1];
            if (chdir(target) == -1)
            {
                write_stderr("could not cd to ", 16);
                write_stderr(target, strlen(target));
                fwrite_stderr("\n", 1);
            }
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
        ret = exec_command(
            command,
            args,
            input_file,
            output_file,
            background
        );
    }

    // Cleanup
    free(command);
    for (i = 1; i < argc; ++i)
    {
        free(args[i]);
    }
    if (input_file != NULL)
    {
        free(input_file);
    }
    if (output_file != NULL)
    {
        free(output_file);
    }

    return ret;
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
    // Establish general signal handling
    struct sigaction SIGINT_ignore  = {0};
    struct sigaction SIGTSTP_action = {0};

    SIGINT_ignore.sa_handler = SIG_IGN;
    //sigfillset(&SIGINT_ignore.sa_mask);

    SIGTSTP_action.sa_handler = SIGTSTP_main;
    sigfillset(&SIGTSTP_action.sa_mask);

    sigaction(SIGINT,  &SIGINT_ignore,  NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Start in `$HOME` directory
    chdir(getenv("HOME"));

    // Allocate space to store PIDs of backgrounded children
    child_capacity = 12;
    children = malloc(child_capacity * sizeof(pid_t));

    // Start up the shell
    int ret = main_loop();

    // Shell is closed, clean up
    free(children);

    return ret;
}
