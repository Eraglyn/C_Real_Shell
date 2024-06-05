#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include "commands.h"
#include <linux/limits.h>

char *PATH;
char *LAST_PATH;
char *HOME;
int LAST_RETURN;
job *JOB_LIST[JOB_LIST_SIZE];
int NEXT_JOB_ID;
job *JOB_UPDATE_LIST[JOB_LIST_SIZE];
int NEXT_UPDATE_ID;

int pwd(int outputstd)
{
    int res = write(outputstd, PATH, strlen(PATH));
    write(outputstd, "\n", 1);

    if (res > 0)
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;
}

int cd(char *path)
{
    if (!strcmp(path, "-"))
    {
        char *tmp = malloc(PATH_MAX + 1);
        strcpy(tmp, PATH);
        strcpy(PATH, LAST_PATH);
        strcpy(LAST_PATH, tmp);
        chdir(PATH);
        free(tmp);
        return EXIT_SUCCESS;
    }
    errno = 0;
    int fd = 0;
    char *abs_path = malloc(PATH_MAX);
    char *a = realpath(path, abs_path);

    if (a == NULL)
    {
        fprintf(stderr, "bash: cd: NONEXISTENT: No such file or directory\n");
        // printf("errno = %d\n", errno);
        free(abs_path);
        return EXIT_FAILURE;
    }
    /* manually check if path points to a directory or a file */
    struct stat path_stat;
    stat(abs_path, &path_stat);
    if (S_ISREG(path_stat.st_mode))
    {
        free(abs_path);
        return 1;
    }
    strcpy(LAST_PATH, PATH);
    strcpy(PATH, abs_path);
    chdir(PATH);
    free(abs_path);
    return 0;
}

job *create_job(char *command, size_t id, pid_t pgid, job_state state, bool is_background)
{
    job *new = malloc(sizeof(job));
    new->id = id;
    new->pgid = pgid;
    new->calling_command = malloc(strlen(command) + 1);
    strcpy(new->calling_command, command);
    new->state = state;
    new->background = is_background;
    new->waiting_to_free = false;
    return new;
}

char *pp_job_state(enum job_state state)
{
    switch (state)
    {
    case Running:
        return "Running";
    case Stopped:
        return "Stopped";
    case Detached:
        return "Detached";
    case Killed:
        return "Killed";
    case Done:
        return "Done";
    default:
        return "error";
    }
    return "error";
}

char *pp_job(job *j)
{
    int buff_size = 1000;
    char *buffer = malloc(buff_size); // Assuming a reasonable buffer size, adjust as needed

    // Use snprintf to format the string into the buffer
    snprintf(buffer, buff_size, "[%zu] %d %s %s\n", j->id, j->pgid, pp_job_state(j->state), j->calling_command);

    // Use write to print the formatted string to stdout
    return buffer;
}

char *pp_job_list()
{
    char *buf = malloc(JOB_LIST_SIZE * 100 * sizeof(char));
    strcpy(buf, "");
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        if (JOB_LIST[i] != NULL)
        {
            if (JOB_LIST[i]->state == Running || JOB_LIST[i]->state == Stopped || JOB_LIST[i]->state == Detached)
            {
                char *tmp = pp_job(JOB_LIST[i]);
                buf = strcat(buf, tmp);
                free(tmp);
            }
        }
    }
    return buf;
}

// affiche les jobs dans la sortie erreur
void jobs()
{
    char *tmp = pp_job_list();
    write(STDERR_FILENO, tmp, strlen(tmp));
    free(tmp);
}

// renovie la taille de la liste des jobs
int get_job_list_size()
{
    int cpt = 0;
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        if (JOB_LIST[i] != NULL)
        {
            // pp_job(JOB_LIST[i]);
            cpt++;
        }
    }
    return cpt;
}

// renvoie le premier job ayant le pgid en argument
job *get_job_from_pgid(pid_t pgid)
{
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        if (JOB_LIST[i] != NULL)
        {
            if (JOB_LIST[i]->pgid == pgid)
            {
                return JOB_LIST[i];
            }
        }
    }
    return NULL;
}

int is_job_list_empty()
{

    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        if (JOB_LIST[i] != NULL)
        {
            return 0;
        }
    }
    return 1;
}

int add_job(job *job)
{
    JOB_LIST[NEXT_JOB_ID] = job;
    NEXT_JOB_ID++;
    return 0;
}

int remove_job(int id)
{
    job *job = JOB_LIST[id];
    free(job->calling_command);
    free(job);
    JOB_LIST[id] = NULL;
    return 0;
}

// ajoute le job passe en argument a la liste des job ayant un changement d'etat
int add_updated_job(job *job)
{
    if (job->background)
    {
        JOB_UPDATE_LIST[NEXT_UPDATE_ID] = job;
        NEXT_UPDATE_ID++;
        return 0;
    }
    return 0;
}

// retire le job passe en argument a la liste des job ayant un changement d'etat
int remove_updated_job(int id)
{
    JOB_UPDATE_LIST[id] = NULL;
    return 0;
}

void pp_updated_jobs()
{
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        job *j = JOB_UPDATE_LIST[i];
        if (j != NULL)
        {
            char *tmp = pp_job(j);
            fprintf(stderr, "%s", tmp);
            free(tmp);

            if (j->state == Done || j->state == Killed)
            {
                remove_job(j->id);
            }

            remove_updated_job(i);
        }
    }
}

// renvoie le file descriptor du fichier passe en argument
int openFdOutput(char **args, int argc)
{
    int i = indice_redirect(args, argc);
    if (i == -1 || !strcmp(args[i], "<"))
        return -1;
    int flags;
    if (!strcmp(args[i], "2>") || !strcmp(args[i], ">"))
        flags = O_WRONLY | O_CREAT | O_EXCL;

    else if (!strcmp(args[i], "2>>") || !strcmp(args[i], ">>"))
        flags = O_WRONLY | O_APPEND | O_CREAT;

    else if (!strcmp(args[i], "2>|") || !strcmp(args[i], ">|"))
        flags = O_WRONLY | O_CREAT | O_TRUNC;

    int fd = open(args[i + 1], flags, 0666);
    // printf("%d", fd);
    if (fd < 0)
    {
        perror("Erreur ouverture fd Sortie");
        exit(1);
    }
    return fd;
}

// renvoie le contenu du fichier passe en argument
char *readInput(char *name)
{

    int fd = open(name, O_RDONLY);
    char *fileContent = malloc(sizeof(char) * 1000000);
    int count = read(fd, fileContent, 100);
    int len = count;
    // printf("1st count%d, contenu %s", count, fileContent);
    while (count > 0)
    {
        count = read(fd, (fileContent + len), 100);
        len += count;
        if (count == -1)
        {
            free(fileContent);
            exit(1);
        }
    }
    close(fd);
    // if (*(fileContent + len - 1) == '\n')
    //     *(fileContent + len - 1) = '\0';
    *(fileContent + len) = '\0';
    // printf("%s\n", fileContent);
    return fileContent;
}

// set la valeur de LAST_RETURN a n
void set_last_return(int n)
{
    LAST_RETURN = n;
}

int last_return()
{
    return LAST_RETURN;
}

int verif_fic(const char *fic)
{
    struct stat file_stat;
    return !stat(fic, &file_stat) && S_ISREG(file_stat.st_mode);
}

// renvoie l'indice du premier charactere de redirection dans args
int indice_redirect(char **args, int argc)
{
    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(args[i], "<") || !strcmp(args[i], ">") || !strcmp(args[i], ">>") || !strcmp(args[i], "2>") || !strcmp(args[i], "2>>") || !strcmp(args[i], ">|") || !strcmp(args[i], "2>|"))
            return i;
    }
    return -1;
}

// renvoie l'indice du prenier & dans args
int indice_bg(char **args, int argc)
{
    for (int i = argc - 1; i >= 0; i--)
    {
        if (!strcmp(args[i], "&"))
            return i;
    }
    return -1;
}

// renvoie l'indice du premier pipe dans args
int indice_pipeline(char **args, int argc)
{
    for (int i = 0; i < argc - 1; i++)
    {
        if (!strcmp(args[i], "|"))
            return i;
    }
    return -1;
}

// renvoie le nb de pipe dans args
int nb_pipeline(char **args, int argc)
{
    int count = 0;
    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(args[i], "|"))
            count += 1;
    }
    return count;
}

// renvoie 1 si il y a un processus "Stopped", 0 sinon
int j_stopped()
{
    for (size_t i = 0; i < JOB_LIST_SIZE; i++)
    {
        if (JOB_LIST[i] != NULL)
        {
            if (JOB_LIST[i]->state == Stopped)
            {
                fprintf(stderr, "exit\nThere are stopped jobs.\n");
                return 1;
            }
        }
    }
    return 0;
}

// envoie le signal "sig" au job ayant le id "target"
int j_kill(int target, int sig)
{
    bool target_job = false;
    int pgid = 0;
    job *j;
    // printf("%i\n", target);
    if (target < 0)
    {
        target_job = true;
        if (JOB_LIST[abs(target)] != NULL)
        {
            j = JOB_LIST[abs(target)];
            pgid = j->pgid;
            // printf("pgid : %i\n", pgid);
        }
        else
        {
            return 1;
        }
    }
    // printf("signal de kill : %i\n", sig);
    if (target_job)
    {
        kill(-pgid, sig);
    }
    else
    {
        kill(target, sig);
    }
    return 0;
}

void si_handler_sighup(int signo, siginfo_t *si, void *data)
{
    job *j = get_job_from_pgid(si->si_pid);
    wait(NULL);
    // add_updated_job(j);
}

// Met a jour les jobs en fonction du signal recu
void si_handler_sigchild(int signo, siginfo_t *si, void *data)
{

    job *j = get_job_from_pgid(si->si_pid);
    int *status = 0;
    // printf("signo %i sigcode %i\n", si->si_signo, si->si_code);
    if (j->background == 1)
    {
        wait(NULL);
    }
    switch (si->si_code)
    {
    case 2:
        j->state = Killed;
        break;
    case 5:
        j->state = Stopped;
        break;
    default:
        j->state = Done;
        break;
    }

    add_updated_job(j);

    // char *line = pp_job(j);
    // printf("%s", line);
    // free(line);
    // printf("status : %i", si->si_status);
    if (j->background == 0)
    {
        remove_job(j->id);
    }
}

// fonction qui renvoie la liste des commandes de args
char ***separate_command_pipeline(char **args, int argc)
{
    int nbPipes = nb_pipeline(args, argc);

    // Allocate memory for the commands array
    char ***commands = malloc((nbPipes + 2) * sizeof(char **));

    int indiceArgs = 0;
    int indiceCommands = 0;

    for (size_t i = 0; indiceCommands < nbPipes + 2; i++)
    {
        if (indiceCommands == nbPipes + 1)
        {
            commands[indiceCommands] = NULL;
        }
        else
        {
            // printf("Indice Args : %i\n", indiceArgs);
            // printf("Indice argc : %i\n", argc);

            int indice = indiceArgs + i;
            int commandSize = indice_pipeline(args + indice, argc - indice);
            if (commandSize == -1)
                commandSize = argc - indice;
            // printf("CommandSize : %i\n", commandSize);
            commands[indiceCommands] = malloc((commandSize + 1) * sizeof(char *));

            for (size_t j = 0; j < commandSize; j++)
            {
                // printf("Ajout de %s dans la case %i d'indice %i\n", args[indice], indiceCommands, j);
                commands[indiceCommands][j] = args[indice];
                indiceArgs += 1;
                indice = indiceArgs + i;
            }

            commands[indiceCommands][commandSize] = NULL;
        }

        indiceCommands += 1;
    }

    return commands;
}

void print_char_array(char **arr)
{
    while (*arr != NULL)
    {
        printf("%s ", *arr);
        arr++;
    }
    printf("\n");
}

// execute la commande passe en argument avec des pipes
int execute_pipeline(char **args, int argc)
{
    int status;
    // int prev_read_pipe = -1;
    char *buf = malloc(sizeof(char) * 10000);
    char ***commands = separate_command_pipeline(args, argc);
    int nbPipes = nb_pipeline(args, argc);

    for (size_t i = 0; i < nbPipes + 1; i++)
    {
        // initialisation du pipe
        int fd[2];
        if (pipe(fd) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        // initialisation du fork
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        size_t commandIsize = 0;
        while (commands[i][commandIsize] != NULL)
        {
            commandIsize++;
        }

        // cas de base
        if (pid == 0 && i == 0)
        {
            dup2(fd[1], STDOUT_FILENO);

            close(fd[1]);
            close(fd[0]);
            status = execvp(commands[i][0], commands[i]);
        }

        // case recursif
        if (pid == 0 && i != 0)
        {
            // dup2(prev_read_pipe, STDIN_FILENO);
            write(STDIN_FILENO, &buf, 8);

            close(fd[0]);
            close(fd[1]);
            // close(prev_read_pipe);

            if (i != nbPipes)
            {
                dup2(fd[1], STDOUT_FILENO);
            }

            status = execvp(commands[i][0], commands[i]);
        }

        if (pid > 0)
        {
            waitpid(pid, &status, 0);
            // prev_read_pipe = fd[0];
            read(fd[0], &buf, 8);
            close(fd[0]);
            close(fd[1]);
        }
    }

    for (size_t i = 0; i < nbPipes + 2; i++)
    {
        free(commands[i]);
    }
    free(commands);

    return status;
}

// execute la commande passe en argument
int externes_command(char **args, int argc)
{
    int indiceRed = indice_redirect(args, argc);
    int indiceBg = indice_bg(args, argc);
    pid_t pid = fork();

    if (pid == -1)
    {
        set_last_return(-1);
    }
    else if (pid == 0)
    {
        // Code exécuté par le processus enfant
        struct sigaction sa = {0};
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = &si_handler_sighup;
        sigaction(SIGHUP, &sa, NULL);

        if (indiceRed > -1) // If a redirection is present, catch it and dup2 the appropriate fd
        {
            for (int i = indiceRed; i < argc - 1; i++)
            {
                if (!strcmp("<", args[i])) // Case STDIN Redirection
                {
                    if (verif_fic(args[i + 1]))
                    {
                        // printf("%s", args[i + 1]);
                        int fd = open(args[i + 1], O_RDONLY);
                        if (fd < 0)
                        {
                            perror("Erreur ouverture fd Entre");
                            exit(1);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                }
                else if (!strcmp(args[i], ">") || !strcmp(args[i], ">>") || !strcmp(args[i], "2>") || !strcmp(args[i], "2>>") || !strcmp(args[i], ">|") || !strcmp(args[i], "2>|")) // Case STDOUT / STDERR Redirection
                {
                    int flags; // Check the redirection to open with the appropriate flags
                    if (!strcmp(args[i], "2>") || !strcmp(args[i], ">"))
                        flags = O_WRONLY | O_CREAT | O_EXCL;

                    else if (!strcmp(args[i], "2>>") || !strcmp(args[i], ">>"))
                        flags = O_WRONLY | O_CREAT | O_APPEND;

                    else if (!strcmp(args[i], "2>|") || !strcmp(args[i], ">|"))
                        flags = O_WRONLY | O_CREAT | O_TRUNC;

                    int fd = open(args[i + 1], flags, 0666);
                    // printf("%d", fd);
                    if (fd < 0)
                    {
                        perror("Erreur ouverture fd Sortie");
                        exit(1);
                    }
                    // Duplicate the file to the correct output
                    if (!strcmp(args[i], ">>") || !strcmp(args[i], ">|") || !strcmp(args[i], ">"))
                        dup2(fd, STDOUT_FILENO);
                    else
                        dup2(fd, STDERR_FILENO);

                    close(fd);
                }
            }
            for (int i = indiceRed; i < argc; i++)
            {
                free(args[i]);
                args[i] = NULL;
                // printf("%s \n", args[i]);
            }
        }
        if (indiceBg > -1) // If command is in background, delete the & from the line to exec it
        {
            free(args[indiceBg]);
            args[indiceBg] = NULL;
            argc--;
        }
        int code = execvp(args[0], args);
        if (code == -1)
        {
            fprintf(stderr, "bash: %s: ", args[0]);
            perror("Erreur exec command extern");
            set_last_return(-code);
        }
        exit(EXIT_FAILURE);
    }
    else
    {
        // Code exécuté par le processus parent
        bool is_background = false;
        if (indiceBg > -1)
        {
            is_background = true;
            argc--;
        }

        char *line = malloc(9999);
        strcpy(line, "");
        for (int i = 0; i < argc; i++) // Recreate the line with a char* to save it in the job
        {

            strcat(line, args[i]);
            strcat(line, " ");
        }
        line[strlen(line) - 1] = '\0';
        job *j = create_job(line, NEXT_JOB_ID, pid, Running, is_background);
        free(line);
        add_job(j);

        struct sigaction sa = {0};
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sa.sa_sigaction = &si_handler_sigchild;
        sigaction(SIGCHLD, &sa, NULL);

        int status = 0;
        if (indiceBg == -1)
        {
            waitpid(pid, &status, 0);
            // printf("%i\n", last_return());
        }

        else
        {
            char *line = pp_job(j);
            fprintf(stderr, "%s", line);
            free(line);
        }
        setpgid(pid, pid);

        if (WIFEXITED(status))
        {
            // Le processus enfant s'est terminé normalement
            set_last_return(WEXITSTATUS(status));
        }
        else
        {
            // Le processus enfant s'est terminé de manière anormale
            set_last_return(1);
        }
    }
    return last_return();
}