#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define MAX_LINE 4096

typedef struct bgJob {
    pid_t pid;
    pthread_t thread;
    int id;
    char* command;
    struct bgJob* next;
} bgJob;

typedef struct {
    char **args;
    int background;
} thread_data;

void batchMode(char* filename);
void interactiveMode(void);
void cmdLine(char *command);
void executeSeq(char *args[], int background);
void *threadExecute(void *ptr);
void pipeControl(char *leftCmd, char *rightCmd);
void outputRedirection(char* filename, int append);
void inputRedirection(char* filename);
void setHistory(char *command);
char *getHistory(void);
void addBg(pid_t pid, char* command, pthread_t thread);
void setFg(int jobId);
void sigchldControl(int signo);

#endif
