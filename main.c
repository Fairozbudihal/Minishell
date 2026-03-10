#include "main.h"

char input[1024];
char prompt[100] = "minishell$ ";

pid_t shell_pgid;
int last_exit_status = 0;

int main()
{
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);

    while (1)
    {
        scan_input(prompt, input);

        if (strlen(input) == 0)
            continue;

        char temp[1024];
        strcpy(temp, input);

        char *cmd = strtok(temp, " ");
        if (!cmd)
            continue;

        int type = check_command_type(cmd);

        if (type == BUILTIN)
            execute_internal_commands(input);
        else
            execute_external_commands(input);
    }

    return 0;
}
