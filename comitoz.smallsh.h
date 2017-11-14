#pragma once

#include "comitoz.utils.h"


// Forward declarations
int exec_command(const char*  command,
                 char* const* args,
                 const char*  input_file,
                 const char*  output_file,
                 bool         background);

int handle_bg_processes(void);

int process_command(char* line);

int main_loop(void);
