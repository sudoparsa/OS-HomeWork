#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

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

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
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
  }
  /** YOUR CODE HERE */
  // create first process
  first_process = (process *) malloc(sizeof(process));;
  first_process->pid = getpid();
  first_process->completed = 0;
  first_process->stopped = 0;
  first_process->background = 0;
  first_process->status = 0;
  first_process->next = NULL;
  first_process->prev = NULL;
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

int execute(char* file_path, tok_t* vars) {
  if (execv(file_path, vars) > 0) {
    return 1;
  }
  // check PATH
  const char* PATH = getenv("PATH");
  if (PATH == NULL) {
    printf("getenv returned NULL\n");
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

int create_process(tok_t* argv) {
  if (argv == NULL || argv[0] == NULL) {
    return -1;
  }
  int cpid = fork();
  if (cpid > 0) {
;
  } else if (cpid == 0) {
    return execute(argv[0], argv);
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
