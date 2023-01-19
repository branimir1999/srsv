#define _POSIX_C_SOURCE 200809L
#define BILLION 1000000000L

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


#define ENVVAR "SRSV_LAB4"
#define BUFFER 100
#define MAXMSG 25

struct task {
    int uid;
    int time;
    char *name;
};

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
struct task tasks[MAXMSG];
unsigned int queue_counter = 0, timed_out = 0, queue_index = 0, queue_size = 0;
void enqueue(struct task *task);
void dequeue(struct task *task);

void worker_thread(pthread_cond_t *wait_cond);
void get_data(char *shm_name, int size, int *data);
void get_env_variable(char *path);
void terminate();
struct task receive_message(char *path);
void print(const char *str, ...);

int main(int argc, char **argv) {
    if(argc < 3) {  // Treba unijeti argumente N (broj dretvi) i M (granica za zbrojeno trajanje svih trenutnih poslova u cekanju)
        printf("Not enough input variables\n");
        exit(1);
    }

    signal(SIGINT, end_simulation);
    signal(SIGTERM, end_simulation);
 
    int success, thread_count = atoi(argv[1]), time_threshold = atoi(argv[2]);
    pthread_t threads[thread_count];                     // Dretve
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex za buđenje radnih dretvi
    pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER; // Uvjet koji radne dretve čekaju za buđenje

    // Napravi radne dreve
    for(int i = 0; i < thread_count; i++) {
        success = pthread_create(&threads[i], NULL, (void *)worker_thread, (void*)&wait_cond);
        if (success != 0) {
            printf("Worker thread failed to create\n");
            exit(1);
        }
    }
    
    // Dretva zaprimanja posla je glavna dretva

    /*
    Dretva koja zaprima poslove radi to na sljedeći način:

    1. Ako ima poruka u redu poruka (poruke u red šalju generatori) onda ih dohvaća i 
       stavlja u lokalni spremnik kao "poslove", npr. u obliku FIFO liste.

    2. Ako u lokalnom spremniku ima barem N poslova koji ukupno trebaju minimalno M jedinica 
       vremena (sekundi) za obradu (M zadan kao parametar naredbene linije), onda dretva 
       signalizira svim radnim dretvama ("budi ih" nekim sinkronizacijskim mehanizmom, npr. monitorima).
    
    3. Ako unutar 30 sekundi u red ne dođe nova poruka, a u lokalnom spremniku ima barem jedan 
       prethodno dohvaćen posao, dretva budi radne dretve (da poslovi u redu previše ne čekaju).
    
    4. Po primitku signala SIGTERM dretva treba označiti kraj, signalizirati to radnim dretvama 
       i završiti s radom.
    */
    
    char path[BUFFER];  // Za environment varijablu
    struct task task;   // Za primanje taskova i kontrolu timedouta
    int time_sum = 0;   // Varijabla za mjerenje je li se M prekoračio

    get_env_variable(path); 
    while(1) {  // Beskonacna petlja
        while(queue_size < thread_count && time_sum < time_threshold) {
            task = receive_message(path);   // Dohvati posao (ili prazni posao)
            if(timed_out) { // Proslo je 30 sekundi; timed out
                print("P: pokrecem zaostale poslove (nakon isteka vise od 30 sekundi)\n");
                timed_out = 0;
                break;
            }
            
            print("P: zaprimio %d %d %s\n", task.uid, task.time, task.name);
            time_sum += task.time;  // Sumiraj ukupno trajanje posla
            enqueue(&task);         // Postavi posao u queue
        }

        // Probudi radne dretve
        pthread_mutex_lock(&mutex);
        pthread_cond_broadcast(&wait_cond);
        pthread_mutex_unlock(&mutex);
    }

    for(int i = 0; i < thread_count + 1; i++) {
        pthread_join(threads[i], NULL);
    }
    
    exit(0);
}

struct task receive_message(char  *path) {
    mqd_t message_queue;
    char msg[BUFFER];
    size_t size;
    unsigned priority;
    struct task task = {.uid = -1, .time = -1, .name = "\0"};
    struct timespec timeout;

    message_queue = mq_open(path, O_RDONLY);
    if (message_queue == (mqd_t) -1) {
        perror("potrosac:mq_open");
        return task;
    }

    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 30;  // Set for 20 seconds
    size = mq_timedreceive(message_queue, msg, BUFFER, &priority, &timeout);
    if (size < 0) {
        if (errno == ETIMEDOUT) {
            timed_out = 1;
            return task;
        } else {
            perror("mq_receive");
            return task;
        }
        
    }
    printf("Primljeno: %s [prio=%d]\n", msg, priority);

    

    sscanf(msg, "%d %d %s", &task.uid, &task.time, task.name);
    return task;
}

void worker_thread(pthread_cond_t *wait_cond) {
    /*
    Radna dretva obrađuje poslove na sljedeći način:

    1. Ako u lokalnom spremniku ima poslova, dretva uzima prvi i obrađuje ga.
    
    2. Ako u lokalnom spremniku nema poslova i nije označen kraj rada dretva čeka.
    
    3. Ako u lokalnom spremniku nema poslova i označen je kraj rada dretva završava s radom.
    
    4. Po završetku obrade ili po "buđenju", dretva opet kreće od točke 1.
    
    U simulaciji obrade koristiti odgodu za simulaciju trošenja vremena (a ne radno čekanje). 
    */
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct task active_task;
    int has_task = 0, size = 0;
    int *data;
    while(1) {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(wait_cond, &mutex);
        pthread_mutex_unlock(&mutex);

        if(queue_size > 0) {
            pthread_mutex_lock(&queue_mutex);
            if(queue_size > 0) {    // Dosao je na red; provjeri je li i dalje ima poslova
                dequeue(&active_task);
                has_task = 1;
            }
            pthread_mutex_unlock(&queue_mutex);
            if (has_task) {
                size = active_task.time * sizeof(int);
                get_data(active_task.name, size, data);
                for(int i = 0; i < active_task.time; i++) {
                    print("id:%d obrada podatka: %d (%d/%d)\n", active_task.uid, data[i], i, active_task.time);
                    sleep(1);
                }
                print("id:%d obrada gotova", active_task.uid);
                munmap(data, size);
                shm_unlink(active_task.name);
                has_task = 0;
            } else {
                print("nema poslova, spavam\n");
            }
        }
    }

    return;
}

void get_data(char *shm_name, int size, int *data){
    int shm_id, 
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
}

void enqueue(struct task *task) {
    if (queue_counter == MAXMSG) {
        perror("cannot enqueue when queue is full");
        exit(1);
    }
    tasks[queue_counter % MAXMSG] = *task;
    queue_counter++;
    queue_size++;
}

void dequeue(struct task *task) {
    if (queue_counter == 0) {
        perror("cannot dequeue when queue is empty");
        exit(1);
    }
    task = &tasks[queue_index % MAXMSG];
    queue_index++;
    queue_size--;
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

void print(const char *str, ...) {
    va_list args;
    va_start(args, str);
    vprintf(str, args);
    va_end(args);
    fflush(stdout);
}

void terminate() {

}