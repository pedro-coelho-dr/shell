#include "shell.h"

int paralelo = 0;
bgJob* bgJobs = NULL;
int countJobID = 1;
char *history = NULL;
int original_stdin;
int original_stdout;


void batchMode(char* filename) {
    signal(SIGCHLD, sigchldControl);

    FILE *batchFile = fopen(filename, "r");
    if (!batchFile){
        perror("[ERROR] file not found");
        exit(1);
    }
    char line[MAX_LINE];
    int count_command = 0;
    while (fgets(line, sizeof(line), batchFile) != NULL){
        count_command++;
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line)==0 || line[0]=='\n') {
            continue;
        }
        printf("\n[%d] executing command: '%s'\n", count_command, line);
        cmdLine(line);
    }
    fclose(batchFile);
    exit(0);
}

void interactiveMode() {
    signal(SIGCHLD, sigchldControl);

    char input[MAX_LINE];
    while (1) {
        printf("\npcdr%s> ", paralelo ? " par" : " seq");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // ctrl+d
        }
        input[strcspn(input, "\n")] = '\0';
        if (strlen(input)==0 || input[0]=='\n') {
            continue;
        }
        cmdLine(input);

        usleep(100000);
    }
}

void cmdLine(char *command) {
    char *commands[MAX_LINE / 2 + 1];
    int command_count = 0;
    pthread_t threads[MAX_LINE / 2 + 1];
    int thread_index = 0;
    int bg_threads[MAX_LINE / 2 + 1] = {0};

    // multiple commands (;)
    char *token = strtok(command, ";");
    while (token != NULL) {
        commands[command_count] = token;
        token = strtok(NULL, ";");
        command_count++;
    }
    commands[command_count] = NULL;

    for (int i=0; i<command_count; i++) {
        char *args[MAX_LINE / 2 + 1];
        int token_count = 0;

        // pipe
        int pipe=0;
        for (char *ptr = commands[i]; *ptr; ptr++){
            if (*ptr == '|') {
                pipe = 1;
                break;
            }
        }
        if (pipe) {
            char *left_cmd = strtok(commands[i], "|");
            char *right_cmd = strtok(NULL, "|");
            pipeControl(left_cmd, right_cmd);
            continue;
        }

        // tokenize
        token = strtok(commands[i], " ");
        while (token!=NULL){
            args[token_count] = token;
            token = strtok(NULL, " ");
            token_count++;
        }
        args[token_count] = NULL;

        // commands
        if (token_count==2 && strcmp(args[0], "fg")==0){
            int jobId;
            if(sscanf(args[1], "%d", &jobId) == 1){
                setFg(jobId);
                continue;
            }else{
                printf("fg <id>\n");
                continue;
            }
        } else if(token_count==1 && strcmp(args[0], "exit") == 0){
            exit(0);
        } else if (token_count== 1 && strcmp(args[0], "!!") == 0){
            char *last_cmd = getHistory();
            if (*last_cmd != '\0') {
                strcpy(commands[i], last_cmd);
                i--;
                continue;
            } else {
                printf("No commands\n");
                continue;
            }
        } else if (token_count==2 && strcmp(args[0], "style") == 0){
            if (strcmp(args[1], "parallel") == 0) {
                paralelo = 1;
                continue;
            } else if (strcmp(args[1], "sequential") == 0) {
                paralelo = 0;
                continue;
            }
        }


        // background
        int background = 0;
        if (token_count>0 && strcmp(args[token_count-1], "&") == 0) {
            background = 1;
            args[token_count-1] = NULL;
        }

        setHistory(commands[i]);

        // redirection
        char* outputFile = NULL;
        char* inputFile = NULL;
        int append = 0;

        for (int j=0; j<token_count-1; j++) {
            if (strcmp(args[j], ">>")==0){
                outputFile = args[j + 1];
                append = 1;
                if(!paralelo)
                args[j] = NULL;                       
            }else if(strcmp(args[j], ">")==0){
                outputFile = args[j + 1];
                if(!paralelo)
                args[j] = NULL;  
            }else if(strcmp(args[j], "<")==0){
                inputFile = args[j + 1];
                if(!paralelo)
                args[j] = NULL;  
            }
        }

        // execute
        if (outputFile) {
            outputRedirection(outputFile, append);
        }
        if (inputFile) {
            inputRedirection(inputFile);
        }

         if (paralelo) {
            thread_data *data = malloc(sizeof(thread_data));
            data->args = args;
            data->background = background;
            if (pthread_create(&threads[thread_index], NULL, threadExecute, data) != 0) {
                perror("[ERROR] pthread_create");
                exit(1);
            }
            
            if (background) {
                bg_threads[thread_index] = 1;
                printf("[%d] %ld\n", countJobID, (long int) threads[thread_index]);
                addBg((pid_t) threads[thread_index], args[0], threads[thread_index]);
            }
            usleep(10000);
            thread_index++;
        } else {
            executeSeq(args, background);
        }

        if (outputFile) {
            dup2(original_stdout, STDOUT_FILENO);
            close(original_stdout);
        }
        if (inputFile) {
            dup2(original_stdin, STDIN_FILENO);
            close(original_stdin);
        }
    }
    if (paralelo) {
        for (int i = 0; i < thread_index; i++) {
            if (!bg_threads[i]) { 
                pthread_join(threads[i], NULL);
            }
        }
    }
}

void executeSeq(char *args[], int background) {
    pid_t pid = fork();
    if (pid == 0) {
        // child
        if (execvp(args[0], args) == -1) {
            perror("[ERROR] style sequential execvp");
            exit(1);
        }
        // fail
    } else if (pid < 0) {
        perror("[ERROR] style sequential fork");
        exit(1);
    } else {
        // parent
        if (background) {
            printf("[%d] %d\n", countJobID, pid);
            addBg(pid, args[0], 0);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

void *threadExecute(void *ptr) {
    thread_data *data = (thread_data *)ptr;
    char cmd[MAX_LINE] = {0};
    for (int i = 0; data->args[i] != NULL; i++) {
        strcat(cmd, data->args[i]);
        strcat(cmd, " ");
    }
    system(cmd);
    free(data);
    return NULL;
}

void pipeControl(char *left_cmd, char *right_cmd) {
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("[ERROR] pipe");
        exit(1);
    }
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stdin = dup(STDIN_FILENO);
    //left
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    cmdLine(left_cmd);
    dup2(saved_stdout, STDOUT_FILENO);

    //right
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    cmdLine(right_cmd);
    dup2(saved_stdin, STDIN_FILENO);

    close(saved_stdout);
    close(saved_stdin);
}

void outputRedirection(char* filename, int append) {
    FILE *file;
    if (append){
        file = fopen(filename, "a");
    }else{
        file = fopen(filename, "w");
    }
    if(!file){
        perror("[ERROR] fopen output redirection > >>");
        exit(1);
    }
    original_stdout = dup(STDOUT_FILENO);
    dup2(fileno(file), STDOUT_FILENO);
    fclose(file);
}

void inputRedirection(char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("[ERROR] fopen input redirection <");
        exit(1);
    }
    original_stdin = dup(STDIN_FILENO);
    dup2(fileno(file), STDIN_FILENO);
    fclose(file);
}

void setHistory(char *command){
    if (history){
        free(history);
    }
    history = strdup(command);
}

char *getHistory() {
    return history ? history : "";
}

void addBg(pid_t pid, char* command, pthread_t thread) {
    bgJob* job = (bgJob*)malloc(sizeof(bgJob));
    job->pid = pid;
    job->thread = thread;
    job->id = countJobID++;
    job->command = strdup(command);
    job->next = bgJobs;
    bgJobs = job;
}

void setFg(int jobId) {
    bgJob* aux = bgJobs;
    while (aux && aux->id != jobId) {
        aux = aux->next;
    }
    if (aux) {
        if (paralelo){
            pthread_join(aux->thread, NULL);
        }else{
            int status;
            waitpid(aux->pid, &status, 0);
        }
    } else {
        printf("no bg job");
    }
}

void sigchldControl(int signo) {
    (void) signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


int main(int argc, char *argv[]) {
    if (argc>2){
        fprintf(stderr, "[ERROR] incorrect number of arguments");
        exit(1);
    }
     
    if (argc==2){
        batchMode(argv[1]);
    }else{
        interactiveMode();
    }
    return 0;
}