#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <pwd.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <linux/wait.h>

#include "commands.h"

#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_BLUE "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#define ANSI_COLOR_CYAN "\033[36m"
#define ANSI_COLOR_RESET "\033[0m"

#define PWD 1
#define CD 2
#define LAST_RETURN 3
#define EXIT 4
#define KILL 5
#define JOBS 6

bool need_to_exit = false;
bool need_to_change_cwd = false;
char *cmds[6] = {"pwd", "cd", "?", "exit", "kill", "jobs"};
size_t size_of_cmds = sizeof(cmds) / sizeof(cmds[0]);

int int_of_string(char *n)
{
    int len = strlen(n);
    int res = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (!isdigit(n[i]))
        {
            return -1;
        }
        res += pow((double)10, (double)(len - 1 - i)) * ((int)n[i] - '0');
    }
    return res;
}

char *string_of_int(int n)
{
    int cpt = 1;
    int tmp = n;
    while (tmp / 10 != 0)
    {
        cpt++;
        tmp /= 10;
    }
    char *res = malloc(cpt * sizeof(char) + 1);
    sprintf(res, "%d", n);

    return res;
}

char *format_path()
{
    char *cp_path = malloc(PATH_MAX);
    strcpy(cp_path, PATH);
    if (strlen(cp_path) <= 25)
    {
        return cp_path;
    }
    char *formatted_path = malloc(26);
    strcpy(formatted_path, "...");
    strncat(formatted_path, (cp_path + (strlen(cp_path) - 22)), 22);
    free(cp_path);

    return formatted_path;
}

char *format_prompt()
{
    char *nb_jobs = malloc(100 * sizeof(char));
    strcpy(nb_jobs, "");
    int n_jobs = get_job_list_size();
    char *n_jobs_string = string_of_int(n_jobs);

    strcat(nb_jobs, ANSI_COLOR_CYAN "[" ANSI_COLOR_YELLOW);
    strcat(nb_jobs, n_jobs_string);
    strcat(nb_jobs, ANSI_COLOR_CYAN "]" ANSI_COLOR_RESET);
    // printf("int n_jobs : %s FIN\n", nb_jobs);
    char *formatted_path = format_path();
    char *prompt = malloc(100);
    strcpy(prompt, (strcat(strcat(nb_jobs, formatted_path), "$ ")));
    free(n_jobs_string);
    free(nb_jobs);
    free(formatted_path);
    return prompt;
}

// renvoie l'entier correspondant a "cmd", renvoie 0 sinon
int cmd_to_int(char *cmd)
{
    for (size_t i = 0; i < size_of_cmds; i++)
    {
        if (!strcmp(cmd, cmds[i]))
        {
            return i + 1;
        }
    }
    return 0;
}

int int_of_id(char *id)
{
    // printf("id: %c\n", id[0]);
    if (id[0] == '%')
    {
        return -int_of_string((id + 1));
    }
    job *j = JOB_LIST[int_of_string(id + 1)];
    if (j == NULL)
    {
        return -1;
    }
    return j->pgid;
}

int sig_of_string(char *sig)
{
    if (sig[0] != '-')
    {
        return -1;
    }
    else
    {
        return int_of_string((sig + 1));
    }
}

// lance la focntion correspond a la ligne de command "line"
int parse_line(char *line)
{
    int exit_val = 0;
    char *tmp = malloc(strlen(line) + 1);
    strcpy(tmp, line);
    char **args = malloc(sizeof(char *) * strlen(line) + 10);
    int argc = 0;
    char *tok = strtok(tmp, " ");
    // separate the line in a char**
    while (tok != NULL)
    {
        *(args + argc) = malloc(strlen(tok) + 1);
        strcpy(*(args + argc), tok);
        argc++;
        tok = strtok(NULL, " ");
    }
    args[argc] = NULL;

    int r = indice_redirect(args, argc);

    if (argc >= 0)
    {
        int cmd = cmd_to_int(args[0]);

        // Dans chaque compare la valeur de r pour les redirections
        switch (cmd)
        {
        case PWD:
            if (argc == 1)
            {
                set_last_return(pwd(STDOUT_FILENO));
            }

            else if (argc == 3 && r > -1)
            {
                int fd = openFdOutput(args, argc);
                set_last_return(pwd(fd));
                close(fd);
            }
            else
            {
                set_last_return(1);
                exit_val = 1;
            }

            break;
        case CD:
            if (argc == 1)
            {
                set_last_return(cd(HOME));
            }
            else if (argc == 2)
            {
                set_last_return(cd(args[1]));
            }
            else if (r > -1 && argc == 3)
            {
                char *name = readInput(args[r + 1]);
                set_last_return(cd(name));
            }
            else
            {
                set_last_return(1);
                exit_val = 1;
            }

            break;
        case LAST_RETURN:

            if (argc == 2 || argc > 3)
            {
                set_last_return(1);
            }
            else
            {
                char *buffer = string_of_int(last_return());
                if (r == -1)
                {

                    write(STDOUT_FILENO, buffer, strlen(buffer));
                    write(STDOUT_FILENO, "\n", 1);
                    fflush(stdout);
                }
                else
                {
                    int fd = openFdOutput(args, argc);
                    write(fd, buffer, strlen(buffer));
                    close(fd);
                }
                free(buffer);
                set_last_return(0);
            }

            break;
        case EXIT:
            if (j_stopped() > 0)
            {
                set_last_return(1);
                break;
            }

            if (argc == 1)
            {
                exit_val = 1;
            }
            else if (argc == 2)
            {
                int code = (int_of_string(args[1]));
                if (code == -1)
                {
                    code = 1;
                }
                set_last_return(code);
                exit_val = 1;
                // printf("code exit : %i\n", last_return());
                // exit(last_return());
            }
            else if (argc == 3 && r > -1)
            {
                char *fileContent = readInput(args[2]);
                if (fileContent[strlen(fileContent) - 1] == '\n')
                    fileContent[strlen(fileContent) - 1] = '\0';
                int code = (int_of_string(fileContent));
                // printf("%s into %d\n", fileContent, code);
                free(fileContent);
                if (code > -1)
                {
                    code = 0;
                }
                exit_val = 1;
                // printf("%i", -code);
                set_last_return(-code);
            }

            break;
        case KILL:
            if (argc != 2 && argc != 3)
            {
                set_last_return(1);
            }
            else
            {
                int kill_sig = SIGTERM;
                int target = 0;
                if (argc == 2)
                {
                    target = int_of_id(args[1]);
                    // printf("Cas 2 args : target : %i, signal : %i\n", target, kill_sig);
                    set_last_return(j_kill(target, kill_sig));
                }
                else
                {
                    kill_sig = sig_of_string(args[1]);
                    target = int_of_id(args[2]);
                    // printf("Cas 3 args : target : %i, signal : %i\n", target, kill_sig);
                    set_last_return(j_kill(target, kill_sig));
                }
            }
            break;
        case JOBS:
            if (argc != 1)
            {
                set_last_return(1);
            }
            else
            {
                jobs();
            }
            break;
        default:
            if (indice_pipeline(args, argc) != -1)
                set_last_return(execute_pipeline(args, argc));
            else
                set_last_return(externes_command(args, argc));
            break;
        }
    }
    else
    {
        // printf("argc inf 0 %i", argc);
        exit_val = 1;
    }

    free(line);
    free(tmp);
    for (size_t i = 0; i < argc; i++)
    {
        free(*(args + i));
    }

    free(args);
    return exit_val;
}

int parse_input()
{
    // get formatted prompt and wait line from user
    char *prompt = format_prompt();
    char *line = readline(prompt);
    if (line == NULL)
    {
        // really bad if happens
        free(prompt);
        free(line);
        exit(last_return());
    }
    if (strlen(line) == 0)
    {
        // empty line, just reprompt
        free(prompt);
        free(line);
        return 0;
    }
    add_history(line);
    free(prompt);
    // now handle the line
    return parse_line(line);
}

void init_job_list()
{
    NEXT_JOB_ID = 1;
    NEXT_UPDATE_ID = 1;
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        JOB_LIST[i] = NULL;
        JOB_UPDATE_LIST[i] = NULL;
    }
}

void init_path()
{
    PATH = getcwd(PATH, PATH_MAX);
    if (PATH == NULL)
    {
        perror("Error, PATH couldn't be initialized.");
    }
    LAST_PATH = malloc(PATH_MAX);
    strcpy(LAST_PATH, PATH);
}

void init_home()
{
    HOME = getenv("HOME");
    if (HOME == NULL)
    {
        perror("Error, HOME couldn't be initialized.");
    }
}

int main()
{
    // init for global variables
    rl_outstream = stderr;
    init_path();
    init_home();
    init_job_list();
    printf(ANSI_COLOR_CYAN "jsh has started.\n" ANSI_COLOR_RESET);
    fflush(stdout);

    // not working
    struct sigaction sa;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT | SIGTERM | SIGTTIN | SIGQUIT | SIGTTOU | SIGTSTP);
    sigprocmask(SIG_BLOCK, &set, NULL);
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sa.sa_mask = set;

    // jsh loop
    while (1)
    {

        // check if exit was called with the return value of parse_input
        int exit_val = parse_input();
        // after handling the line of user, show updated jobs
        pp_updated_jobs();
        if (exit_val > 0)
        {
            // exit procedure
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            exit(last_return());
        }
    }
    return EXIT_SUCCESS;
}