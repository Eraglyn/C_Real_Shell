#ifndef COMMANDS_H
#define COMMANDS_H
#define JOB_LIST_SIZE 1000

typedef enum job_state
{
    Running,
    Stopped,
    Detached,
    Killed,
    Done
} job_state;

typedef struct job
{
    size_t id;
    pid_t pgid;
    enum job_state state;
    char *calling_command;
    bool background;
    bool waiting_to_free;
} job;

extern char *PATH;
extern char *LAST_PATH;
extern char *HOME;
extern int LAST_RETURN;
extern job *JOB_LIST[JOB_LIST_SIZE];
extern int NEXT_JOB_ID;
extern job *JOB_UPDATE_LIST[JOB_LIST_SIZE];
extern int NEXT_UPDATE_ID;

extern char *job_done_buffer;

job *create_job(char *command, size_t id, pid_t pgid, job_state state, bool is_forground);
char *pp_job_state(enum job_state state);
char *pp_job(job *j);
void jobs();
int get_job_list_size();
job *get_job_from_pgid(pid_t pgid);
int add_job(job *job);
int remove_job(int id);
void pp_updated_jobs();
int add_updated_job(job *job);
int remove_updated_job(int id);
int j_stopped();
int j_kill(int sig, int target);
void si_handler(int signo, siginfo_t *si, void *data);

int openFdOutput(char **args, int argc);
char *readInput(char *name);
int pwd(int outputstd);
int cd(char *path);
int last_return();
void set_last_return(int n);
int indice_redirect(char **args, int argc);
int indice_bg(char **args, int argc);
int indice_pipeline(char **args, int argc);
int externes_command(char *args[], int argc);
int execute_pipeline(char **args, int argc);
#endif