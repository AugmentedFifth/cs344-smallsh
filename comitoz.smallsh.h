#pragma once

#include "comitoz.utils.h"


/*** Forward declarations ***/

// Signal handler for `SIGINT`s sent to the main shell. This is a noop
// function instead of `SIG_IGN` so that we know to re-prompt the user when
// they send a `SIGINT` to the shell (i.e. `getline()` is interrupted).
//
// ## Parameters:
// * `signo` - The signal number that triggered this function. Unused.
void SIGINT_main(int signo);

// Signal handler for `SIGTSTP`s sent to the main shell.
//
// Toggles the ability to background commands and spits out a message to the
// user in lieu of that.
//
// ## Parameters:
// * `signo` - The signal number that triggered this function. Unused.
void SIGTSTP_main(int signo);

// Fearsomely `SIGTERM`s all child processes and leaves them for dead. Any
// unruly children who do not properly respond to the `SIGTERM` are left in
// the cold, uncaring hands of the kernel.
void kill_children(void);

// Handles `fork()`ing and `exec()`ing commands.
//
// This function contains both the parent and child logic & behavior,
// including setting up input/output redirection, signal handling, and waiting
// for child processes.
//
// ## Parameters:
// * `command` - A string that is the user's typed-in command, after "$$"
//               expansion.
// * `args` - A `NULL`-terminated array of strings corresponding to the `argv`
//            for the command, including `args[0]` being the same string as
//            `command`.
// * `input_file` - A string representing a path to an input file. Can be
//                  `NULL`, in which case it is stdin for foreground processes
//                  and "/dev/null" for background processes.
// * `output_file` - A string representing a path to an output file. Can be
//                   `NULL`, in which case it is stdout for foreground
//                   processes and "/dev/null" for background processes.
// * `background` - Is this command to be run as a background process?
//
// **Returns** non-zero only on catastrophic failure.
int exec_command(const char*  command,
                 char* const* args,
                 const char*  input_file,
                 const char*  output_file,
                 bool         background);

// Checks all currently stored background child processes to see if any of
// them have terminated since we last checked, waiting for them, deregistering
// them from the list of background children, and alerting the user.
//
// **Returns** zero on success.
int handle_bg_processes(void);

// Parses a command and redirects its content to the corresponding
// behavior.
//
// This function does the "$$" expansion as well as all built-in commands.
//
// ## Parameters:
// * `line` - A string representing the literal line entered into the shell
//            by the user.
//
// **Returns** non-zero when the program should exit, and zero otherwise.
int process_command(char* line);

// Enters the main loop, spitting out a prompt and waiting for commands,
// forking and executing external commands via calling other functions.
//
// Handles re-prompting when `getline()` is interrupted (generally, by some
// signal handler).
//
// **Returns** zero on success.
int main_loop(void);
