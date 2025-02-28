/* 
 * tsh - A tiny shell program with job control
 *  
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* Per-job data */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, FG, BG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

volatile sig_atomic_t ready; /* Is the newest child in its own process group? */

/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);
void sigusr1_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int freejid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(STDOUT_FILENO, STDERR_FILENO);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != -1) {
        switch (c) {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    Signal(SIGUSR1, sigusr1_handler); /* Child is ready */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) {
    // declare variables
    int fork_pid;
    char *argv[MAXARGS];
    int argc = parseline(cmdline, argv);
    int bg = 0;
    
    // set volatile variable `ready` to 0
    ready = 0;

    // check if cmdline is empty
    if (argv[0] == NULL)  // Without this, an empty cmdline input causes Segmentation Fault.
        return;
    if (!strcmp(argv[argc - 1], "&")) {
        bg = 1;
        argv[argc - 1] = NULL;
    }
    // check if it's a built-in command
    if (!builtin_cmd(argv)) {  // if not, execute code below
        // block SIGINT, SIGTSTP
        sigset_t blockmask, oldmask;
        sigemptyset(&blockmask);
        sigaddset(&blockmask, SIGINT);
        sigaddset(&blockmask, SIGTSTP);
        sigprocmask(SIG_BLOCK, &blockmask, &oldmask);

        // fork
        if ((fork_pid = fork()) < 0) {
            perror("fork");
            exit(1);
        }
        // CHILD PROCESS
        // -------------
        else if (fork_pid == 0) {
            // set process group id for main child process
            setpgid(0, 0);

            // let parent know that process group id has been set
            kill(getppid(), SIGUSR1);

            // uninstall the SIGCHLD handler for child process (so that it can wait for its own children)
            struct sigaction childact;
            childact.sa_handler = SIG_DFL;
            childact.sa_flags = 0;
            sigemptyset(&childact.sa_mask);
            sigaction(SIGCHLD, &childact, NULL);

            // ---------------------------------------------------------------------------------------

            int i;
            int pipe_encountered = 0;  // if ("|" encountered) any following "<" renders the cmdline INVALID.
            int out_redir_encountered = 0;  // if (">" encountered) any following "|" renders the cmdline INVALID.
            // ERROR CHECKS
            for (i = 0; i < argc && argv[i] != NULL; i++) {
                // update flags
                if (!strcmp(argv[i], "|"))
                    pipe_encountered = 1;
                if (!strcmp(argv[i], ">"))
                    out_redir_encountered = 1;
                // ERROR CHECK: argv[0] is obviously not a command
                if (!strcmp(argv[0], "<") || !strcmp(argv[0], ">") || !strcmp(argv[0], "|")) {
                    printf("Invalid commandline\n");
                    exit(1);
                }
                // ERROR CHECK: invalid argument to i/o redirectors or pipe operator
                if (!strcmp(argv[i], "<") || !strcmp(argv[i], ">") || !strcmp(argv[i], "|")) {
                    if (argv[i + 1] == NULL || !strcmp(argv[i + 1], "<")
                          || !strcmp(argv[i + 1], ">") || !strcmp(argv[i + 1], "|")) {
                        printf("Invalid commandline\n");
                        exit(1);
                    }
                }
                // ERROR CHECK: "<" appears after encountering "|"
                if (pipe_encountered && !strcmp(argv[i], "<")) {
                    printf("Invalid commandline: an input redirector \"<\" cannot appear after a pipe \"|\"\n");
                    exit(1);
                }
                // ERROR CHECK: "|" appears after encountering ">"
                if (out_redir_encountered && !strcmp(argv[i], "|")) {
                    printf("Invalid commandline: a pipe operator \"|\" cannot appear after an output redirector \">\"\n");
                    exit(1);
                }
            }
            // 1. Detect pointers in argv that point to "|", "<", or ">".
            // 2. Replace such pointers with a NULL pointer.
            // 3. Remember the index position.
            int pipe_count = 0;
            int is_pipe[MAXARGS] = {0};
            int is_inredir[MAXARGS] = {0};
            int is_outredir[MAXARGS] = {0};
            for (i = 0; i < argc && argv[i] != NULL; i++) {
                if (!strcmp(argv[i], "|")) {
                    is_pipe[i] = 1;
                    argv[i] = NULL;
                    pipe_count++;
                } else if (!strcmp(argv[i], "<")) {
                    is_inredir[i] = 1;
                    argv[i] = NULL;
                } else if (!strcmp(argv[i], ">")) {
                    is_outredir[i] = 1;
                    argv[i] = NULL;
                }
            }
            //int fd_stdin_backup = dup(fileno(stdin));
            //int fd_stdout_backup = dup(fileno(stdout));
            int fds[pipe_count][2];
            int i_fd = -1;
            int skip_this_iteration = 0;
            char **argv_ptr = argv;
            for (i = 0; i < argc; i++) {
                printf("ITERATION (%d):\n", i);
                printf("argv[%d] = %s\n", i, argv[i]);
                if (skip_this_iteration) {  // due to argv[i - 1] having been "<", ">", or "|"
                    if (is_pipe[i - 1]) {
                        *argv_ptr = argv[i];
                        printf("\n--- after `*argv_ptr = argv[i]`...");
                        printf("  argv_ptr[0] = %s \n\n", argv_ptr[0]);
                    }
                    skip_this_iteration = 0;
                    continue;
                }
                // USAGE: /bin/ls | /bin/grep e | /bin/grep drive

                if (is_inredir[i]) {
                    skip_this_iteration = 1;
                    int fdin = open(argv[i + 1], O_RDONLY);
                    dup2(fdin, fileno(stdin));
                }

                else if (is_outredir[i]) {
                    skip_this_iteration = 1;
                    int fdout = open(argv[i + 1], O_WRONLY | O_CREAT, 0644);
                    dup2(fdout, fileno(stdout));
                }

                else if (is_pipe[i]) {
                    skip_this_iteration = 1;
                    i_fd++;
                    if (pipe(fds[i_fd]) == -1) {
                        perror("pipe");
                        exit(1);
                    }
                    int r = fork();
                    if (r < 0) {
                        perror("fork");
                        exit(1);
                    } else if (r == 0) {  // SUB-CHILD
                        close(fds[i_fd][0]);
                        dup2(fds[i_fd][1], fileno(stdout));
                        close(fds[i_fd][1]);

                        sigprocmask(SIG_SETMASK, &oldmask, NULL);

                        // execute command
                        execve(argv_ptr[0], argv_ptr, environ);  //TODO
                        printf("%s: Command not found\n", argv_ptr[0]);
                        exit(1);
                    } else {              // MAIN-CHILD
                        close(fds[i_fd][1]);
                        dup2(fds[i_fd][0], fileno(stdin));
                        close(fds[i_fd][0]);
                    }
                }
                printf("END OF ITERATION (%d)\n\n", i);
            }
            // unblock SIGINT, SIGTSTP
            sigprocmask(SIG_SETMASK, &oldmask, NULL);

            // execute command
            execve(argv_ptr[0], argv_ptr, environ);
            printf("%s: Command not found\n", argv_ptr[0]);
            exit(1);
        }
        // PARENT PROCESS
        // --------------
        while (!ready);  // block until the child has set its own process group id

        // BACK-GROUND
        if (bg) {
            addjob(jobs, fork_pid, BG, cmdline);
            // unblock SIGINT, SIGTSTP
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            // BG process update
            printf("[%d] (%d) %s", pid2jid(fork_pid), fork_pid, cmdline);
        }
        // FORE-GROUND
        else {
            addjob(jobs, fork_pid, FG, cmdline);
            // unblock SIGINT, SIGTSTP
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            waitfg(fork_pid);
        }
    }
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return number of arguments parsed.
 */
int parseline(const char *cmdline, char **argv) {
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to space or quote delimiters */
    int argc;                   /* number of args */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
        delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    
    return argc;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) {

    if(strcmp(argv[0], "quit") == 0) {
        exit(1);
    }
    if(strcmp(argv[0], "jobs") == 0) {
        listjobs(jobs);
        return 1;
    }

    int res_bg = strcmp(argv[0], "bg");
    int res_fg = strcmp(argv[0], "fg");

    if(res_fg == 0 || res_bg == 0) {
        do_bgfg(argv);
        return 1;
    }

    return 0;     /* not a builtin command */
    
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
    pid_t pid;
    int jid;
    int argc = 0;
    char bufarg[MAXARGS];
    struct job_t *job;

    while (argv[argc] != NULL) {
        argc++;
    }
    if (!strcmp(argv[0], "bg")) {
        // error checks
        if (argc != 2) {
            printf("bg command requires PID or %%jid argument\n");
            return;
        }
        // case_1: second arg = %jid
        if (argv[1][0] == '%') {
            if (argv[1][1] == '\0' || !isdigit(argv[1][1])) {
                printf("%s: No such job\n", argv[1]);
                return;
            }
            strncpy(bufarg, &argv[1][1], sizeof(bufarg));
            bufarg[MAXARGS - 1] = '\0';
            jid = atol(bufarg);
            if ((job = getjobjid(jobs, jid)) == NULL) {
                printf("%s: No such job\n", argv[1]);
                return;
            }
            kill(-(job->pid), SIGCONT);
            job->state = BG;
            printf("[%d] (%d) %s", jid, job->pid, job->cmdline);
            return;
        }
        // case_2: second arg = PID
        if (!isdigit(argv[1][0])) {
            printf("bg: argument must be a PID or %%jid\n");
            return;
        }
        pid = atol(argv[1]);
        if ((job = getjobpid(jobs, pid)) == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
        kill(-(job->pid), SIGCONT);
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, pid, job->cmdline);
        return;


    } else if (!strcmp(argv[0], "fg")) {
        // error checks
        if (argc != 2) {
            printf("fg command requires PID or %%jid argument\n");
            return;
        }
        // case_1: second arg = %jid
        if (argv[1][0] == '%') {
            if (argv[1][1] == '\0' || !isdigit(argv[1][1])) {
                printf("%s: No such job\n", argv[1]);
                return;
            }
            strncpy(bufarg, &argv[1][1], sizeof(bufarg));
            bufarg[MAXARGS - 1] = '\0';
            jid = atol(bufarg);
            if ((job = getjobjid(jobs, jid)) == NULL) {
                printf("%s: No such job\n", argv[1]);
                return;
            }
            kill(-(job->pid), SIGCONT);
            job->state = FG;
            waitfg(job->pid);
            return;
        }
        // case_2: second arg = PID
        if (!isdigit(argv[1][0])) {
            printf("fg: argument must be a PID or %%jid\n");
            return;
        }
        pid = atol(argv[1]);
        if ((job = getjobpid(jobs, pid)) == NULL) {
            printf("(%d): No such process\n", pid);
            return;
        }
        kill(-(job->pid), SIGCONT);
        job->state = FG;
        waitfg(pid);
        return;
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
    sigset_t mask;

    // Mask is now an empty set, this is because we do not want sigsuspend
    // to block any signal.

    sigemptyset(&mask);

    while(pid == fgpid(jobs)){

        //sigsuspend, suspends the process here until any singal is recieved
        //by handle, it then checks if process pid is still the foreground
        //process, if it is not, then the loop breaks.

        sigsuspend(&mask);

    }
    if (verbose) printf("waitfg: Process (%d) no longer the fg process\n", pid);
    return;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) {
    if (verbose) printf("sigchld_handler: entering\n");

    pid_t job_pid;
    int status;


    while((job_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        struct job_t *job = getjobpid(jobs, job_pid);
        if(WIFEXITED(status)){  // If SIGCHLD was received because the child terminated natrually
            if (verbose) printf("sigchld_handler: Job [%d] (%d) deleted\n", pid2jid(job_pid), job_pid);
            if (verbose) printf("sigchld_handler: Job [%d] (%d) terminates OK (status %d)\n", pid2jid(job_pid), job_pid, WEXITSTATUS(status));
            deletejob(jobs, job_pid);
        }
        else if(WIFSIGNALED(status)){
            if (verbose) printf("sigchld_handler: Job [%d] (%d) deleted\n", pid2jid(job_pid), job_pid);
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(job_pid), job_pid, WTERMSIG(status));
            deletejob(jobs, job_pid);
        }
        else if(WIFSTOPPED(status)){
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(job_pid), job_pid, WSTOPSIG(status));
            job->state = ST;
        }
    }

    if(job_pid == -1 && errno != ECHILD){
        perror("waitpid");
    }

    if (verbose) printf("sigchld_handler: exiting\n");
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {
    if (verbose) printf("sigint_handler: entering\n");
    //Get job pid of the foreground job from Jobs
    pid_t job_pid = fgpid(jobs);
    //If their is a job in the foreground then we kill the job
    if(job_pid != 0){
        if (verbose) printf("sigint_handler: Job [%d] (%d) killed\n", pid2jid(job_pid), job_pid);
        int return_val = kill(-job_pid, SIGINT);
        //error check in case process could not be killed
        if(return_val == -1){
            fprintf(stderr, "SIGINT Error: Job Could not be killed");
        }
    }
    if (verbose) printf("sigint_handler: exiting\n");
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) {
    if (verbose) printf("sigtstp_handler: entering\n");

    //Get job pid of the foreground jov
    pid_t job_pid = fgpid(jobs);

    //If their is a job in the foreground then we stop the job
    if(job_pid != 0){
        if (verbose) printf("sigtstp_handler: Job [%d] (%d) stopped\n", pid2jid(job_pid), job_pid);

        int return_val = kill(-job_pid, SIGTSTP);

        if(return_val == -1){
            fprintf(stderr, "SIGTSTOP Error: Job Could not be killed.");
        }
    }
    if (verbose) printf("sigtstp_handler: exiting\n");
    return;
}

/*
 * sigusr1_handler - child is ready
 */
void sigusr1_handler(int sig) {
    if (verbose) printf("sigusr1_handler: entering\n");
    ready = 1;
    if (verbose) printf("sigusr1_handler: exiting\n");
}


/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* freejid - Returns smallest free job ID */
int freejid(struct job_t *jobs) {
    int i;
    int taken[MAXJOBS + 1] = {0};
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid != 0) 
        taken[jobs[i].jid] = 1;
    for (i = 1; i <= MAXJOBS; i++)
        if (!taken[i])
            return i;
    return 0;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
    int i;
    
    if (pid < 1)
        return 0;
    int free = freejid(jobs);
    if (!free) {
        printf("Tried to create too many jobs\n");
        return 0;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = free;
            strcpy(jobs[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    return 0; /*suppress compiler warning*/
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
    }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG: 
                    printf("Running ");
                    break;
                case FG: 
                    printf("Foreground ");
                    break;
                case ST: 
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ", 
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message and terminate
 */
void usage(void) {
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
