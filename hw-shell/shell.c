#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {{cmd_help, "?", "show this help menu"},
                          {cmd_exit, "exit", "exit the command shell"},
                          {cmd_pwd, "pwd", "print the current working directory"},
                          {cmd_cd, "cd", "change the current working directory"}};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Print the current working directory*/
int cmd_pwd(unused struct tokens* tokens) {
  char buf[1024];
  if (getcwd(buf, 1024) == NULL) {
    perror("pwd error");
  } else
    printf("%s\n", buf);
  return 1;
}

int cmd_cd(struct tokens* tokens) {
  char* path = tokens_get_token(tokens, 1);
  if (chdir(path) != 0)
    perror("cd error");
  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

/*
Get the environment variable PATH, and split them into string array
*/
char** get_env_path(size_t* sz) {
  char** PATH = malloc(100 * sizeof(char*));
  *sz = 0;
  char* pathvar = getenv("PATH");
  char* saveptr;
  char* token;
  token = strtok_r(pathvar, ":", &saveptr);
  while (token) {
    PATH[*sz] = malloc(strlen(token) + 1);
    strcpy(PATH[*sz], token);
    (*sz)++;
    token = strtok_r(NULL, ":", &saveptr);
  }
  return PATH;
}

/*
Get program params
*/
char** get_params(struct tokens* tokens, size_t* n) {
  *n = tokens_get_length(tokens);
  char** params = malloc((*n + 1) * sizeof(char*));
  for (int i = 0; i < (*n); ++i) {
    char* arg = tokens_get_token(tokens, i);
    params[i] = malloc(strlen(arg) + 1);
    strcpy(params[i], arg);
  }
  params[*n] = NULL;
  return params;
}

/*
Check whether the path is the absolute path, the relative path or the filename in cwd, which is valid for execv()
*/
bool isvalid(const char* path) {
  if (path[0] == '/' || path[0] == '.')
    return true;
  char buf[1024];
  getcwd(buf, 1024);
  strcat(buf, "/");
  strcat(buf, path);
  if (access(buf, F_OK) == 0)
    return true;
  else
    return false;
}

/*
If the path is not absolute path, make it a absolute path
*/
char* make_abs_path(const char* path, char** PATH, const int sz) {
  for (int i = 0; i < sz; ++i) {
    char* dir = malloc(100);
    dir[0] = '\0';
    strcat(dir, PATH[i]);
    strcat(dir, "/");
    strcat(dir, path);
    if (access(dir, F_OK) == 0)
      return dir;
    else
      free(dir);
  }
  return NULL;
}

/*
Process the redirection
*/
void process_redirection(char** params, const size_t n) {
  int infd = 0;
  int outfd = 0;
  for (int i = 0; i < n; i++) {
    //Find the output redirection
    if (strcmp(params[i], ">") == 0) {
      params[i] = NULL;
      char* filename = malloc(strlen(params[i + 1]) + 1);
      strcpy(filename, params[i + 1]);
      outfd = creat(filename, 0644);
      if (outfd < 0) {
        perror("open output file fails");
        exit(0);
      }
      dup2(outfd, STDOUT_FILENO);
      close(outfd);
    }
    //Find the input redirection
    else if (strcmp(params[i], "<") == 0) {
      params[i] = NULL;
      char* filename = malloc(strlen(params[i + 1]) + 1);
      strcpy(filename, params[i + 1]);
      infd = open(filename, O_RDONLY);
      if (infd == -1) {
        perror("open input file fails");
        exit(0);
      }
      dup2(infd, STDIN_FILENO);
      close(infd);
    }
  }
}

/*Ignore all the signal*/
void ignore_signal() {
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
}

/*Open all the signal action*/
void open_signal() {
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGCONT, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
}

/* Execute the single program, support the redirection and path resolution*/
void exec_prog(char** params, const int n, char** PATH, const int sz) {
  char* filepath = isvalid(params[0]) ? params[0] : make_abs_path(params[0], PATH, sz);
  process_redirection(params, n);
  execv(filepath, params);
}

/* Spawn the child process*/
void spawn_proc(int in, int fd[2], char** argv, int argc, char** PATH, const size_t sz) {
  pid_t pid;
  int status;
  pid = fork();
  if (pid < 0) {
    perror("Fork fails");
    exit(0);
  } else if (pid == 0) {
    //There is no need for current program to read from the current pipe
    printf("SPAWN:pid: %d, SPAWN pgid: %d, terminal foreground pgid: %d\n", getpid(),
           getpgid(getpid()), tcgetpgrp(0));
    close(fd[0]);
    //Redirect the standard input to in, which is the pipe containing the result of last program
    if (in != STDIN_FILENO) {
      dup2(in, STDIN_FILENO);
      close(in);
    }

    //Redirect the standard output to out
    if (fd[1] != STDOUT_FILENO) {
      dup2(fd[1], STDOUT_FILENO);
      close(fd[1]);
    }
    exec_prog(argv, argc, PATH, sz);
  } else {
    //Control center should not have access to write fd of current pipe
    close(fd[1]);
    //The read fd of last pipe is no longer needed
    if (in != 0)
      close(in);
    //Control center only needs the read fd of current pipe and wait for the current program done.
    wait(&status);
  }
}

char** get_pipe_params(char** params, int start, int argc) {
  char** argv = malloc((argc + 1) * sizeof(char*));
  for (int j = 0; j < argc; ++j) {
    argv[j] = malloc(strlen(params[start + j]) + 1);
    strcpy(argv[j], params[start + j]);
  }
  argv[argc] = NULL;
  return argv;
}

/* Execute the program depending on whether that the command is pipeline or single program*/
void fork_pipes(char** params, const int n, char** PATH, const size_t sz) {
  //Split out to be a single process group
  setpgid(getpid(), getpid());
  //Make me the foreground process group of the terminal because terminal will only send signal to foreground process group
  tcsetpgrp(0, getpid());
  //Open the signal
  open_signal();
  printf("CENTER pid: %d, CENTER pgid: %d, terminal foreground pgid: %d\n", getpid(),
         getpgid(getpid()), tcgetpgrp(0));
  int start = 0;
  bool foundPipe = false;
  int in = 0, fd[2];
  for (int i = 0; i < n; ++i) {
    //Find a pipe command
    if (strcmp(params[i], "|") == 0) {
      foundPipe = true;
      int argc = i - start;
      char** argv = get_pipe_params(params, start, argc);
      //Generate the pipe for the pipe command
      if (pipe(fd) != 0) {
        perror("Pile fails");
        exit(0);
      }
      spawn_proc(in, fd, argv, argc, PATH, sz);
      //the next program will read from the read fd of current pipe
      in = fd[0];
      start = i + 1;
    }
  }
  if (foundPipe) {
    //Execute the final program and output should be sent to standard output
    printf("FINAL SPAWN pid: %d, FIANL SPAWN pgid: %d, terminal foreground pgid: %d\n", getpid(),
           getpgid(getpid()), tcgetpgrp(0));
    int argc = n - start;
    char** argv = get_pipe_params(params, start, argc);
    if (in != STDIN_FILENO)
      dup2(in, STDIN_FILENO);
    exec_prog(argv, argc, PATH, sz);
  }
  //No pipe command
  else {
    printf("PROC IN CENTER pid: %d, PROC IN CENTER pgid: %d, terminal foreground pgid: %d\n",
           getpid(), getpgid(getpid()), tcgetpgrp(0));
    exec_prog(params, n, PATH, sz);
  }
}

int main(unused int argc, unused char* argv[]) {
  init_shell();
  tcsetpgrp(0, getpid());
  ignore_signal();
  printf("SHELL pid: %d, SHELL pgid: %d terminal foreground pgid: %d\n", getpid(),
         getpgid(getpid()), tcgetpgrp(0));
  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      //fprintf(stdout, "This shell doesn't know how to run programs.\n");
      int status;
      pid_t pid;
      pid = fork();
      if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
      }
      //child process
      else if (pid == 0) {
        //Get the environment "PATH"
        //printf("Here is child\n");
        size_t sz = 0;
        char** PATH = get_env_path(&sz);
        size_t n = 0;
        char** params = get_params(tokens, &n);
        //Current process is a "control center", it forks the process to realize the series pipe command
        fork_pipes(params, n, PATH, sz);
      }
      //parent process
      else {
        //printf("Here is parent\n");
        wait(&status);
        //printf("Child returns and here is parent\n");
      }
    }
    //My foreground position was taken over by my child and now I take it back
    tcsetpgrp(0, getpid());
    printf("SHELL pid: %d, SHELL pgid: %d terminal foreground pgid: %d\n", getpid(),
           getpgid(getpid()), tcgetpgrp(0));
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }
  return 0;
}
