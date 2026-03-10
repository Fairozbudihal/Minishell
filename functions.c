#include "main.h"

/* ================= JOB STRUCT ================= */

typedef struct job
{
    int id;
    pid_t pgid;
    char cmd[256];
    int stopped;
    struct job *next;
} job;

job *head = NULL;
int job_id = 1;

/* ================= INPUT ================= */

void scan_input(char *prompt, char *input)
{
    printf(ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET, prompt);

    if (fgets(input, 1024, stdin) == NULL)
    {
        printf("\n");
        exit(0);
    }

    input[strcspn(input, "\n")] = '\0';
}

/* ================= BUILTIN CHECK ================= */

int check_command_type(char *cmd)
{
    if (!strcmp(cmd, "cd") ||
        !strcmp(cmd, "pwd") ||
        !strcmp(cmd, "echo") ||
        !strcmp(cmd, "exit") ||
        !strcmp(cmd, "jobs") ||
        !strcmp(cmd, "fg") ||
        !strcmp(cmd, "bg"))
        return BUILTIN;

    return EXTERNAL;
}

/* ================= ADD JOB ================= */

void add_job(pid_t pgid, char *cmd, int stopped)
{
    job *new = malloc(sizeof(job));
    new->id = job_id++;
    new->pgid = pgid;
    strcpy(new->cmd, cmd);
    new->stopped = stopped;
    new->next = head;
    head = new;
}

/* ================= REMOVE JOB ================= */

void remove_job(pid_t pgid)
{
    job *temp = head, *prev = NULL;

    while (temp)
    {
        if (temp->pgid == pgid)
        {
            if (prev)
                prev->next = temp->next;
            else
                head = temp->next;

            free(temp);
            return;
        }
        prev = temp;
        temp = temp->next;
    }
}

/* ================= LIST JOBS ================= */

void list_jobs()
{
    job *temp = head;

    if (!temp)
    {
        printf("No jobs\n");
        return;
    }

    while (temp)
    {
        printf("[%d] %s %s\n",
               temp->id,
               temp->stopped ? "Stopped" : "Running",
               temp->cmd);
        temp = temp->next;
    }
}

/* ================= FG ================= */

void bring_fg(int id)
{
    job *temp = head;

    if (id == 0)
        temp = head;
    else
        while (temp && temp->id != id)
            temp = temp->next;

    if (!temp)
    {
        printf("No such job\n");
        return;
    }

    tcsetpgrp(STDIN_FILENO, temp->pgid);
    kill(-temp->pgid, SIGCONT);

    int status;
    waitpid(-temp->pgid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (WIFSTOPPED(status))
        temp->stopped = 1;
    else
        remove_job(temp->pgid);
}

/* ================= BG ================= */

void continue_bg(int id)
{
    job *temp = head;

    if (id == 0)
        temp = head;
    else
        while (temp && temp->id != id)
            temp = temp->next;

    if (!temp)
    {
        printf("No such job\n");
        return;
    }

    kill(-temp->pgid, SIGCONT);
    temp->stopped = 0;
}

/* ================= SIGCHLD ================= */

void sigchld_handler(int sig)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        remove_job(pid);
    }
}

/* ================= BUILTINS ================= */

void execute_internal_commands(char *input)
{
    char temp[1024];
    strcpy(temp, input);

    char *cmd = strtok(temp, " ");

    if (!strcmp(cmd, "cd"))
    {
        char *dir = strtok(NULL, " ");

        if (!dir)
        {
            chdir(getenv("HOME"));
        }
        else
        {
            if (chdir(dir) != 0)
                perror("cd");
        }
    }
    else if (!strcmp(cmd, "pwd"))
    {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    }
    else if (!strcmp(cmd, "echo"))
    {
        char *arg = strtok(NULL, "");

        if (!arg)
            printf("\n");
        else if (!strcmp(arg, "$$"))
            printf("%d\n", getpid());
        else if (!strcmp(arg, "$?"))
            printf("%d\n", last_exit_status);
        else if (!strcmp(arg, "$SHELL"))
            printf("%s\n", getenv("SHELL"));
        else
            printf("%s\n", arg);
    }
    else if (!strcmp(cmd, "exit"))
        exit(0);
    else if (!strcmp(cmd, "jobs"))
        list_jobs();
    else if (!strcmp(cmd, "fg"))
    {
        char *id = strtok(NULL, " ");
        bring_fg(id ? atoi(id) : 0);
    }
    else if (!strcmp(cmd, "bg"))
    {
        char *id = strtok(NULL, " ");
        continue_bg(id ? atoi(id) : 0);
    }
}

/* ================= PIPE + EXEC ================= */

void execute_external_commands(char *input)
{
    char *commands[20];
    int count = 0;

    char temp[1024];
    strcpy(temp, input);

    char *token = strtok(temp, "|");
    while (token)
    {
        while (*token == ' ')
            token++;
        commands[count++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2];
    int in_fd = 0;
    pid_t pgid = 0;

    for (int i = 0; i < count; i++)
    {
        if (i < count - 1)
            pipe(pipefd);

        pid_t pid = fork();

        if (pid == 0)
        {
            if (pgid == 0)
                pgid = getpid();

            setpgid(0, pgid);

            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (in_fd != 0)
            {
                dup2(in_fd, 0);
                close(in_fd);
            }

            if (i < count - 1)
            {
                dup2(pipefd[1], 1);
                close(pipefd[0]);
                close(pipefd[1]);
            }

            execlp("sh", "sh", "-c", commands[i], NULL);
            exit(1);
        }
        else
        {
            if (pgid == 0)
                pgid = pid;

            setpgid(pid, pgid);

            if (in_fd != 0)
                close(in_fd);

            if (i < count - 1)
            {
                close(pipefd[1]);
                in_fd = pipefd[0];
            }
        }
    }

    tcsetpgrp(STDIN_FILENO, pgid);

    int status;
    waitpid(-pgid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, shell_pgid);

    if (WIFSTOPPED(status))
    {
        add_job(pgid, input, 1);
        printf("[%d] Stopped %s\n", job_id - 1, input);
    }
}
