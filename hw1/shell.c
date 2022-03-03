#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_pwd(tok_t arg[]);

int cmd_cd(tok_t arg[]);

int cmd_wait(tok_t arg[]);


/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_pwd, "pwd", "print name of current/working directory"},
  {cmd_cd, "cd", "change directory"},
  {cmd_wait, "wait", "wait until all background jobs have terminated"},
};

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int cmd_pwd(tok_t arg[]) {
  char *cwd = malloc(INPUT_STRING_SIZE+1);
  size_t size = INPUT_STRING_SIZE+1;
  cwd = getcwd(cwd, size);
  printf("%s\n", cwd);
  free(cwd);
  return 1;
}

int cmd_cd(tok_t arg[]) {
  chdir(arg[0]);
  return 1;
}

int cmd_wait(tok_t arg[]) {
  while (waitpid(-1, NULL, 0) > 0) {
    ;
  }
  return 1;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void default_signals() {
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
}

void ignore_signals() {
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);

    ignore_signals();
  }
  /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  process* process = first_process;
  while (process->next) {
    process = process->next;
  }
  process->next = p;
  p->prev = process;
}

int execute(char* file_path, tok_t* vars, int argc) {
  if (execv(file_path, vars) > 0) {
    return 1;
  }
  // check PATH
  const char* PATH = getenv("PATH");
  if (PATH == NULL) {
    return -1;
  }
  for (char* c = strtok(PATH, ":"); c; c = strtok(NULL, ":")) {
    char *file_address = malloc(INPUT_STRING_SIZE+1);
    strcpy(file_address, c);
    strcat(file_address, "/");
    strcat(file_address, file_path);
    if (execv(file_address, vars) > 0) {
      return 1;
    }
    free(file_address);
  }
  return -1;
}

int redirect(int argc, tok_t* argv) {
  int out_index = isDirectTok(argv, ">");
  if (out_index != -1) {
    int redirect_file = open(argv[out_index + 1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(redirect_file, STDOUT_FILENO);
    int i;
    for (i = out_index; argv[i + 2]; i++) {
      argv[i] = argv[i + 2];
    }
    argv[i] = NULL;
    argc -= 2;
  }
  int in_index = isDirectTok(argv, "<");
  if (in_index != -1) {
    int redirect_file = open(argv[in_index + 1], O_RDONLY);
    dup2(redirect_file, STDIN_FILENO);
    int i;
    for (i = in_index; argv[i + 2]; i++) {
      argv[i] = argv[i + 2];
    }
    argv[i] = NULL;
    argc -= 2;
  }
  return argc;
}

int create_process(tok_t* argv) {
  if (argv == NULL || argv[0] == NULL) {
    return -1;
  }
  int argc = countToks(argv);
  int run_in_background = strcmp(argv[argc - 1], "&") == 0 ? TRUE : FALSE;
  if (run_in_background == TRUE) {
    argv[argc - 1] = NULL;
    argc--;
  }
  int cpid = fork();
  if (cpid > 0) {
    if (run_in_background == FALSE) {
      wait(0);
    }
    tcsetpgrp(shell_terminal, shell_pgid);
  } else if (cpid == 0) {
    setpgid(getppid(), getppid());
    if (run_in_background == FALSE && tcgetpgrp(shell_terminal) == shell_pgid) {
      tcsetpgrp(shell_terminal, getppid());
    }
    default_signals();
    argc = redirect(argc, argv);
    exit(execute(argv[0], argv, argc));
  }
  return 1;
}


int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();

  // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum=0;
  // fprintf(stdout, "%d: ", lineNum);
  while ((s = freadln(stdin))){
    t = getToks(s); /* break the line into tokens */
    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
      if (create_process(t) < 0) {
        fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
      }
    }
    // fprintf(stdout, "%d: ", lineNum);
  }
  return 0;
}