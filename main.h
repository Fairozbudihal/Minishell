#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>

#define BUILTIN 1
#define EXTERNAL 2

#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

extern pid_t shell_pgid;
extern int last_exit_status;

/* input */
void scan_input(char *prompt, char *input);

/* builtin */
int check_command_type(char *cmd);
void execute_internal_commands(char *input);

/* external */
void execute_external_commands(char *input);

/* jobs */
void list_jobs();
void bring_fg(int id);
void continue_bg(int id);

/* signals */
void sigchld_handler(int sig);

#endif
