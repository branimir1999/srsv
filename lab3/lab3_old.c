#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define THREAD_COUNT 6      // Ukljucuje upravljac, stoga N = 32 dretvi ulaza
#define K 1                 // Dodatna odgoda moze biti od nista do jednog cijele periode
#define MINISLEEP 1         // Kratko spavaj prije provjere upravljackog odgovora (1 MS)
#define LAB3RT 0            // Korištenje RMPA/SCHED_FIFO rasporedivanja

// Pocetna konfiguracija dretve
struct input_data_structure {
    int id;             // Krece od 0
    int period;         // Sekunde
    int starting_delay; // Sekunde
};

// Reprezentira razmjenu poruka između dretvi
struct message {
    int input_state;                        // Postavlja simulator
    int response;                           // Postavlja upravljac
    struct timeval response_timestamp;      // Postavlja upravljac
    int inputs_changed_count;               // Statistika; treba upravljac
    struct timeval time_sum;                // Statistika; treba upravljac
    struct timeval max_wait;                // Statistika; treba upravljac
    int overdue_response_count;             // Statistika; treba upravljac
    struct input_data_structure input_data; // Pocetni podaci za simulator
};

pthread_t threads[THREAD_COUNT];
pthread_t controller_threads[THREAD_COUNT];
struct message messages[THREAD_COUNT];
unsigned long waitConstant = 1000000;
sig_atomic_t sigflag = 0; // Zastavica za prekid rada

void end_simulation(int sig_num);
int msleep(int msec);
float randomFloat(int range);
void *controller_thread();
void *input_simulator_thread(int * arg);
void wait_ms(long ms);
void wait10ms();
void setWaitConstant();
struct input_data_structure *init_input_data_structure();
struct message *init_message_data_structure();
void init_thread_data();

int main() {
    if(THREAD_COUNT < 2) {
        exit(0);
    }

    setWaitConstant();
    init_thread_data();
    signal(SIGINT, end_simulation);

    int success = 0;
    
    // Napravi istovremeno i upravljacke dretve i dretve simulatora ulaza
    for(int i = 0; i < THREAD_COUNT; i++) {
        // Napravi upravljacku dretvu
        success = pthread_create(&threads[0], NULL, (void *)controller_thread, NULL);
        if (success != 0) {
            printf("Controller thread failed to create\n");
            exit(1);
        }

        // Napravi dretvu simulatora ulaza
        success = pthread_create(&threads[i], NULL, (void *)input_simulator_thread, (void *)&(messages[i].input_data.id));
        if (success != 0) {
            printf("Input simulator thread failed to create\n");
            exit(1);
        }
    }

    // Cekaj da upravljacka petlja zavrsi
    pthread_join(threads[0], NULL);
    exit(0);
}

// Dretva upravljač
void * controller_thread() {
    srand(time(NULL));
    
    int prev_input_state[THREAD_COUNT-1];   // Prethodna stanja
    int current_state, success, i;
    for(i = 0; i < THREAD_COUNT-1; i++) {   // Postavi sva prethodna stanja na -1 (defaultno pocetno stanje)
        prev_input_state[i] = -1;
    }

    struct timeval timestamp;
	while(sigflag == 0) {
		for(i = 0; i < THREAD_COUNT-1; i++) {
			current_state = messages[i].input_state;            // Dohvati stanje ulaza    
			if(current_state != prev_input_state[i]) {          // Ako se promijenilo stanje ulaza
                printf("Upr: ulaz %d: promjena (%d->%d), obradujem\n", messages[i].input_data.id, prev_input_state[i], messages[i].input_state);
                fflush(stdout);
                processing(messages[i].input_data.period);                         // Simuliraj trajanje obrade
                prev_input_state[i] = current_state;     
                printf("Upr: ulaz %d: kraj obrade, postavljeno (%d)\n", messages[i].input_data.id, messages[i].input_state);
                fflush(stdout);
                gettimeofday(&timestamp, NULL);              
                messages[i].response = 1;                       // Postavi odgovor
                messages[i].response_timestamp = timestamp;     // i trenutak zadnjeg odgovora
			} else {
                printf("Upr: ulaz %d: nema promjene (%d)\n", messages[i].input_data.id, prev_input_state[i]);
                fflush(stdout);
            }
		}
	}
    
    // Zavrsi dretvu cim zavrsis petlju
    exit(0);
}

// Dretva simulator ulaza "i"`
void * input_simulator_thread(int * arg) {
    int id = *arg;
    srand(time(NULL) + id);

    // Pomocne varijable
    struct timeval state_changed_timestamp, response_time;
    response_time = (struct timeval){0};
    float delay = 0;    

	sleep(messages[id].input_data.starting_delay); // pričekaj početak rada ulaza i "prva pojava"

	while(sigflag == 0) {
		messages[id].response = -1; // poništi prethodni odgovor
		messages[id].input_state = rand() % 900 + 100; // generiraj novo stanje (slučajni broj u rasponu 100-999)
		gettimeofday(&state_changed_timestamp, NULL); // ažuriraj trenutak promjene stanja
		messages[id].inputs_changed_count += 1; // povećaj broj promjena stanja

        printf("Dretva %d: promjena %d\n", id, messages[id].input_state);
        fflush(stdout);

        // (a) odgovor treba doći najkasnije nakon jedne periode
		sleep((float)messages[id].input_data.period + delay); // odgodi izvođenje za jednu periodu

        if (messages[id].response == -1) { // ako NIJE pristigao odgovor na novo stanje
			//problem: odgovor kasni
			printf("Dretva %d: problem, nije odgovoreno; cekam odgovor\n", id);
            fflush(stdout);
            messages[id].overdue_response_count += 1; // povećaj brojač prekasno pristiglih odgovora
			//čekaj na odgovor
            while (messages[id].response == -1) { // dok NIJE pristigao odgovor na novo stanje
               msleep(MINISLEEP); // vrlo kratko odspavaj
            }
		}

		// (b) odgovor je stigao
        timersub(&messages[id].response_timestamp, &state_changed_timestamp, &response_time); // izračunaj trajanje od novog stanja do odgovora
        timeradd(&messages[id].time_sum, &response_time, &messages[id].time_sum); // dodaj to vrijeme u sumu (radi kasnijeg proračuna srednjeg vremena)
        if(messages[id].max_wait.tv_usec < response_time.tv_usec) { // ako je zadnje trajanje veće od najdužeg ažuriraj i to
            messages[id].max_wait = response_time;
        }

		delay = randomFloat(K-1) * (float)messages[id].input_data.period; //odgodi do trenutak zadnje promjene (???) + period + dodatna odgoda

        printf("Dretva %d: odgovoreno (%ld - %ld = %ld milisekundi od promjene ulaza); spavam %.2f sekundi\n", id, messages[id].response_timestamp.tv_usec / 1000, state_changed_timestamp.tv_usec / 1000, response_time.tv_usec / 1000, (float)messages[id].input_data.period + delay);
        fflush(stdout);
    }

    // Zavrsi dretvu cim zavrsis petlju
    exit(0);
}

// Spavanje u sekundama
int sleep(float sec) {
    struct timespec ts;
    int success;

    if (sec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = sec;
    ts.tv_nsec = sec * 1000000;

    do {
        success = nanosleep(&ts, &ts); // Cekaj odabrani broj milisekundi
    } while (success && errno == EINTR);

    return success;
}

// Simulacija obrade
void processing(int period)
{
    int chance = rand() % 100;
    double percent;

    if (chance < 50) {        // 50%
        percent = 0.1;  
    } else if (chance < 80) { // 30%
        percent = 0.2;
    } else if (chance < 95) { // 15%
        percent = 0.4;
    } else {                  // 5%
        percent = 0.7; 
    }

    int ms = (int)(percent * period);
    wait_ms(ms);
}

// Vraca realni broj iz intervala [0, range]
float randomFloat(int range) {
    if(range != 0)
        return (float)rand() / (float)(RAND_MAX/range);
    else
        return 0;
}

void wait10ms() {
    for(int i = 0; i < waitConstant; i++) {
		asm volatile("" ::: "memory");
	}
}

void wait_ms(long ms) {
    for (int i = 0; i < ms/10; i++) {
        wait10ms();
    }
}

void setWaitConstant() {
    struct timeval t0, t1, diff;
	while (1) {
		gettimeofday(&t0, NULL); 
		wait10ms();
		gettimeofday(&t1, NULL); 
        timersub(&t1, &t0, &diff); // izračunaj trajanje od novog stanja do odgovora
        if (diff.tv_usec >= 10000) {
            break;
        } else {
            waitConstant *= 10;
        }
	}
	waitConstant = waitConstant * 10 / (diff.tv_sec * 1000 + diff.tv_usec / 1000);
}

struct input_data_structure * init_input_data_structure() {
    struct input_data_structure starting_inputs[] = 
    {
        { .id = 0, .period = 1,  .starting_delay = 0},
        { .id = 1, .period = 2,  .starting_delay = 1},
        { .id = 2, .period = 5,  .starting_delay = 18},
        { .id = 3, .period = 2,  .starting_delay = 0},
        { .id = 4, .period = 10, .starting_delay = 1},
        { .id = 5, .period = 1,  .starting_delay = 3}
    };

    return starting_inputs;
}

struct message *init_message_data_structure(struct input_data_structure *input_data) {
    struct message starting_msg = {
        .input_state = -1,
        .response = -1,
        .response_timestamp = (struct timeval){0},
        .inputs_changed_count = 0,
        .time_sum = (struct timeval){0},
        .max_wait = (struct timeval){0},
        .overdue_response_count = 0,
        .input_data = input_data
    };

    return &starting_msg;
}

// Postavlja podatke za komunikaciju izmedu simulatora ulaza i upravljaca
void init_thread_data() {
    struct input_data_structure starting_inputs[] = init_input_data_structure();

    for (int i = 0; i < THREAD_COUNT; i++) {
        messages[i] = *init_message_data_structure(&starting_inputs[i]);
    }
}

void end_simulation(int sig_num)
{
    sigflag = 1;

    printf("---------------  STATISTIKA ----------------");
    long global_inputs_changed_count = 0, global_time_sum = 0, global_overdue_response_count = 0, global_max_wait = 0;
    int i;
    for(i = 0; i < THREAD_COUNT - 1; i++) {
        float avg_response_time = (float)messages[i].time_sum.tv_usec / (float)messages[i].inputs_changed_count / 1000;
        printf("Dretva %d:\n", messages[i].input_data.id);
        printf("\tBroj promijenjenih ulaza: %d\n", messages[i].inputs_changed_count);
        printf("\tProsjecno vrijeme cekanja odgovora: %.3f milisekundi\n", avg_response_time);
        printf("\tBroj zakasnjenih odgovora: %d\n", messages[i].overdue_response_count);
        printf("\tNajdulje cekanje: %ld\n", messages[i].max_wait.tv_usec / 1000);

        global_inputs_changed_count += messages[i].inputs_changed_count;
        global_time_sum += messages[i].time_sum.tv_usec;
        global_overdue_response_count += messages[i].overdue_response_count;
        if (global_max_wait < messages[i].max_wait.tv_usec) {
            global_max_wait = messages[i].max_wait.tv_usec;
        }
    }
    float global_avg_response_time = (float)global_time_sum / (float)global_inputs_changed_count / 1000;    
    printf("Globalna statistika:\n");
    printf("\tBroj promijenjenih ulaza: %ld\n", global_inputs_changed_count);
    printf("\tProsjecno vrijeme cekanja odgovora: %.3f milisekundi\n", global_avg_response_time);
    printf("\tBroj zakasnjenih odgovora: %ld\n", global_overdue_response_count);
    printf("\tNajdulje cekanje: %ld\n", global_max_wait / 1000);
    printf("-------------------------------");
    fflush(stdout);
}