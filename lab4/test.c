#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <mqueue.h>

struct task {
    int uid;
    int time;
    char *name;
};

struct uid_generator {
    pthread_mutex_t mutex;
    int last_uid;
};

#define UID_GENERATOR_SIZE sizeof(struct uid_generator)
#define ENVVAR "SRSV_LAB4"
#define BUFFER 100
#define MAXMSG 25


int generate_uids(char *path, int task_count);
void get_env_variable(char *path);
void generate_task_name(char *shm_name, char *path, int uid);
void generate_shared_memory(int uid, int time, char *shm_name);
void send_message(struct task task, char * path);
void terminate();
char * to_str(char *str, int num);

int main(int argc, char **argv) {
    if(argc < 3) {
        printf("Not enough input variables\n");
        exit(1);
    }

    signal(SIGINT, terminate);
    //signal(SIGTERM, terminate);

    int success, offset, task_count = atoi(argv[1]), max_time =  atoi(argv[2]);
    char path[BUFFER];
    int uids[task_count];

    srand(time(NULL));
    get_env_variable(path);
    offset = generate_uids(path, task_count);
    
    // Generate tasks
    for (int i = 0; i < task_count; i++) {
        int uid = offset + i;
        int time = rand() % (max_time - 1) + 1;
        char shm_name[BUFFER];
        generate_task_name(shm_name, path, uid);
        generate_shared_memory(uid, time, shm_name);

        struct task task = {.uid = uid, .time = time, .name = shm_name};
        send_message(task, path);

        sleep(1);
    }
    
    exit(0);
}

void get_env_variable(char *path) {
    char data[BUFFER];

    if(!getenv(ENVVAR)){
        fprintf(stderr, "The environment variable %s was not found.\n", ENVVAR);
        exit(1);
    }

    // Make sure the buffer is large enough to hold the environment variable
    // value. 
    if(snprintf(data, BUFFER, "%s", getenv(ENVVAR)) >= BUFFER){
        fprintf(stderr, "BUFSIZE of %d was too small. Aborting\n", BUFFER);
        exit(1);
    }    

    path[0] = '/';
    path[1] = '\0';
    strcat(path, data);
}

int generate_uids(char *path, int task_count) {
    char data[BUFFER];
    int shm_id, offset, exists = 0;
    struct uid_generator *generator; // TODO: inicijalizirati ako ne postoji

    strcpy(data, path); // Trenutno: /lab4
    strcat(data, "-g"); // Trenutno: /lab4-g

    shm_id = shm_open(data, O_CREAT | O_RDWR | O_EXCL, 00600);
    if (shm_id == -1 && errno == EEXIST) {
        shm_id = shm_open(path, O_RDWR, 00600);
        exists = 1;
    }
    if (ftruncate(shm_id, UID_GENERATOR_SIZE) == -1) {
        perror("shm_open/ftruncate");
        exit(1);
    }

    generator = mmap(NULL, UID_GENERATOR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0);
    if (generator == (void *) -1) {
        perror("mmap");
        exit(1);
    }

    if(!exists) {
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        generator->mutex = mutex;
        generator->last_uid = 0;
    }

    close(shm_id);
    pthread_mutex_lock(&(generator->mutex));
    offset = generator->last_uid;      // Dohvati pomak
    generator->last_uid += task_count; // Postavi novi vrijednost
    pthread_mutex_unlock(&(generator->mutex));
    munmap(generator, UID_GENERATOR_SIZE);
    
    return offset;
}

void generate_task_name(char *shm_name, char *path, int uid) {
    strcpy(shm_name, path); // Trenutno: /lab4
    strcat(shm_name, "-");  // Trenutno: /lab4-

    // Pretvori int u string
    char str[BUFFER];
    strcat(shm_name, to_str(str, uid));  //Trenutno: /lab4-5
}

void generate_shared_memory(int uid, int time, char *shm_name) {
    int shm_id, size = time * sizeof(int);
    int* data;
    int tasks[time];

    shm_id = shm_open(shm_name, O_CREAT | O_RDWR, 00600);
    if (shm_id == -1 || ftruncate(shm_id, size) == -1) {
        perror("shm_open/ftruncate");
        exit(1);
    }
    
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0);
    if (data == (void *) -1) {
        perror("mmap");
        exit(1);
    }

    close(shm_id);
    for(int i = 0; i < time; i++) {
        int num = rand() % 1000;
        data[i] = num;
        tasks[i] = num;
    }
    munmap(data, size);

    char tasks_str[BUFFER];
    tasks_str[0] = '\0';
    for(int i = 0; i < time; i++) {
        char tmp[BUFFER];
        strcat(tasks_str, to_str(tmp, tasks[i]));
        strcat(tasks_str, " ");
    }

    print("G: posao %d %d %s [%s]\n", uid, time, shm_name, tasks_str);
}

void send_message(struct task task, char * path) {
    char msg[BUFFER];
    struct mq_attr attr = {.mq_flags = 0, .mq_maxmsg = MAXMSG, .mq_msgsize = BUFFER};
    unsigned priority = 10;
    mqd_t message_queue;
   
    sprintf(msg, "%d %d %s", task.uid, task.time, task.name);
    size_t size = strlen(msg) + 1;
    message_queue = mq_open(path, O_WRONLY | O_CREAT, 00600, &attr);
    if (message_queue == (mqd_t) -1) {
        perror("proizvodjac:mq_open");
        return -1;
    }
    if (mq_send(message_queue, msg, size, priority)) {
        perror("mq_send");
        return -1;
    }
    printf("Poslano: %s [prio=%d]\n", msg, priority);
}

char * to_str(char *str, int num) {
    int length = snprintf(NULL, 0, "%d", num);
    snprintf(str, length + 1, "%d", num);
    return str;
}

void terminate() {
    shm_unlink("/lab4-g");
    mq_unlink("/lab4"); // TODO
    // TODO: obrisat sve shm_unlink za taskove /lab4-id
}