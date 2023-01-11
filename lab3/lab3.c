#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <stdarg.h>

#define THREAD_COUNT 6      // Ukljucuje upravljac, stoga N=12
#define LAB3RT 0            // Korištenje RMPA/SCHED_FIFO rasporedivanja

// Pocetna konfiguracija dretve
struct input_data_structure {
    int id;             // Krece od 0
    int period;         // Sekunde
    int starting_delay; // Sekunde
    int priority;       // Prioritet; manji broj, veci prioritet
};

// Reprezentira razmjenu poruka između dretvi
struct message {
    int input_state;                        // Postavlja simulator
    int response;                           // Postavlja upravljac
    struct timeval response_timestamp;      // Postavlja upravljac
    int inputs_changed_count;               // Statistika; treba upravljac
    struct timeval time_sum;                // Statistika; treba upravljac
    struct timeval wait_time_sum;           // Statistika; treba upravljac
    struct timeval max_wait;                // Statistika; treba upravljac
    int overdue_response_count;             // Statistika; treba upravljac
    struct input_data_structure input_data; // Pocetni podaci za simulator
};

pthread_t threads[THREAD_COUNT];
pthread_t controller_threads[THREAD_COUNT];
struct message messages[THREAD_COUNT];
sig_atomic_t sigflag = 0; // Zastavica za prekid rada
unsigned long waitConstant = 1000000;

void *controller_thread();
void *input_simulator_thread(int * arg);
void processing(int period);
void msleep(long msec);
void wait10ms();
void wait_ms(long ms);
void setWaitConstant();
void init_thread_data();
long getTime(struct timeval time);
void print(const char *str, ...);
void end_simulation(int sig_num);

int main() {
    if(THREAD_COUNT < 2) {
        exit(0);
    }

    int success = 0;

    setWaitConstant();
    init_thread_data();
    signal(SIGINT, end_simulation);

    # if LAB3RT
	/* create threads */
    long min, max, policy = SCHED_FIFO;
    pthread_attr_t attr;
	struct sched_param prio;

    min = sched_get_priority_min(policy);
	max = sched_get_priority_max(policy);
    prio.sched_priority = min;

    pthread_attr_init (&attr);
	pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy (&attr, policy);
    if (pthread_setschedparam (pthread_self(), policy, &prio)) {
		perror ( "Error: pthread_setschedparam (root permission?)" );
		exit (1);
	}

	for (int i = 0; i < THREAD_COUNT; i++ ) {
		prio.sched_priority = max - messages[i].input_data.priority;
		pthread_attr_setschedparam(&attr, &prio);

        // Napravi upravljacku dretvu
        success = pthread_create(&threads[0], &attr, (void *)controller_thread, (void *)&(messages[i].input_data.id));
        if (success != 0) {
            printf("Controller thread failed to create\n");
            exit(1);
        }

        prio.sched_priority = max;
		pthread_attr_setschedparam(&attr, &prio);

        // Napravi dretvu simulatora ulaza
        success = pthread_create(&threads[i], &attr, (void *)input_simulator_thread, (void *)&(messages[i].input_data.id));
        if (success != 0) {
            printf("Input simulator thread failed to create\n");
            exit(1);
        }
	}
    # else    
    // Napravi istovremeno i upravljacke dretve i dretve simulatora ulaza
    for(int i = 0; i < THREAD_COUNT; i++) {
        // Napravi upravljacku dretvu
        success = pthread_create(&threads[0], NULL, (void *)controller_thread, (void *)&(messages[i].input_data.id));
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

    #endif

    // Cekaj da upravljacka petlja zavrsi
    pthread_join(threads[0], NULL);
    exit(0);
}

// Dretva upravljač
void * controller_thread(int * arg) {
    int id = *arg, prev_input_state = -1, current_state, to_wait;
    struct timeval timestamp_begin, timestamp_end, wait_time;

    srand(time(NULL) + id);
    
    wait_ms(messages[id].input_data.starting_delay * 1000); // pričekaj početak rada ulaza 'i' odnosno "prva pojava"

	while(sigflag == 0) {
		current_state = messages[id].input_state;        // Dohvati stanje ulaza    
        if(current_state != prev_input_state) {          // Ako se promijenilo stanje ulaza
            gettimeofday(&timestamp_begin, NULL); 
            print("Upr: ulaz %d: promjena (%d->%d), obradujem\n", id, prev_input_state, current_state);
            processing(messages[id].input_data.period);                         // Simuliraj trajanje obrade
            prev_input_state = current_state;
            print("Upr: ulaz %d: kraj obrade, postavljeno (%d)\n", id, current_state);
            gettimeofday(&timestamp_end, NULL);              
            messages[id].response = 1;                       // Postavi odgovor
            messages[id].response_timestamp = timestamp_end; // i trenutak zadnjeg odgovora
            timersub(&timestamp_end, &timestamp_begin, &wait_time);
            timeradd(&messages[id].wait_time_sum, &wait_time, &messages[id].wait_time_sum); // statistika
            to_wait = messages[id].input_data.period * 1000 - getTime(wait_time);
            msleep(to_wait);  // čekaj početak iduće periode; sleep jer nema obrade
        }
	}
    
    // Zavrsi dretvu cim zavrsis petlju
    exit(0);
}

// Dretva simulator ulaza "i"`
void * input_simulator_thread(int * arg) {
    int id = *arg;
    struct timeval state_changed_timestamp, response_time;
    response_time = (struct timeval){0};

    srand(time(NULL) + id);       

	msleep(messages[id].input_data.starting_delay * 1000); // pričekaj početak rada ulaza i "prva pojava"

	while(sigflag == 0) {
		messages[id].response = -1; // poništi prethodni odgovor
		messages[id].input_state = rand() % 900 + 100; // generiraj novo stanje (slučajni broj u rasponu 100-999)
		gettimeofday(&state_changed_timestamp, NULL); // ažuriraj trenutak promjene stanja
		messages[id].inputs_changed_count += 1; // povećaj broj promjena stanja

        print("Dretva %d: promjena %d\n", id, messages[id].input_state);

        // (a) odgovor treba doći najkasnije nakon jedne periode
		msleep(messages[id].input_data.period * 1000); // odgodi izvođenje za jednu periodu

        if (messages[id].response == -1) { // ako NIJE pristigao odgovor na novo stanje
			//problem: odgovor kasni
			print("Dretva %d: problem, nije odgovoreno; cekam odgovor\n", id);
            messages[id].overdue_response_count += 1; // povećaj brojač prekasno pristiglih odgovora
			//čekaj na odgovor
            while (messages[id].response == -1) { // dok NIJE pristigao odgovor na novo stanje
               msleep(1); // vrlo kratko odspavaj
            }
		}

		// (b) odgovor je stigao
        timersub(&messages[id].response_timestamp, &state_changed_timestamp, &response_time); // izračunaj trajanje od novog stanja do odgovora
        timeradd(&messages[id].time_sum, &response_time, &messages[id].time_sum); // dodaj to vrijeme u sumu (radi kasnijeg proračuna srednjeg vremena)
        if(getTime(messages[id].max_wait) < getTime(response_time)) { // ako je zadnje trajanje veće od najdužeg ažuriraj i to
            messages[id].max_wait = response_time;
        }

        print("Dretva %d: odgovoreno (%ld milisekundi od promjene ulaza); spavam %ld sekundi\n", id, getTime(response_time), messages[id].input_data.period);
    }

    // Zavrsi dretvu cim zavrsis petlju
    exit(0);
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

    int ms = (int)(percent * period * 1000); // Perioda je u sekundama pa mnozimo s 1000
    wait_ms(ms);
}

void msleep(long msec) {
    struct timespec ts;
    int success;

    if (msec < 0) {
        errno = EINVAL;
        return;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        success = nanosleep(&ts, &ts); // Cekaj odabrani broj milisekundi
    } while (success && errno == EINTR);
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
        if (getTime(diff) >= 10) {
            break;
        } else {
            waitConstant *= 10;
        }
	}
	waitConstant = waitConstant * 10 / getTime(diff);
}

// Postavlja podatke za komunikaciju izmedu simulatora ulaza i upravljaca
void init_thread_data() {
    struct input_data_structure inputs[THREAD_COUNT] = 
    { // Periode i pocetna odgoda obrade su u sekundama te priroitet prema duljini periode
        { .id = 0, .period = 1,  .starting_delay = 0, .priority = 1},
        { .id = 1, .period = 2,  .starting_delay = 1, .priority = 2},
        { .id = 2, .period = 2,  .starting_delay = 3, .priority = 2},
        { .id = 3, .period = 5,  .starting_delay = 5, .priority = 3},
        { .id = 4, .period = 10, .starting_delay = 4, .priority = 4},
        { .id = 5, .period = 20, .starting_delay = 2, .priority = 5}
    };

    for (int i = 0; i < THREAD_COUNT; i++) {
        messages[i] = (struct message) {
            .input_state = -1,
            .response = -1,
            .response_timestamp = (struct timeval){0},
            .inputs_changed_count = 0,
            .time_sum = (struct timeval){0},
            .wait_time_sum = (struct timeval){0},
            .max_wait = (struct timeval){0},
            .overdue_response_count = 0,
            .input_data = inputs[i]
        };
    }
}

// Vrijeme u milisekundama
long getTime(struct timeval time) {
    return time.tv_usec / 1000 + time.tv_sec * 1000;
}

void print(const char *str, ...) {
    va_list args;
    va_start(args, str);
    vprintf(str, args);
    va_end(args);
    fflush(stdout);
}

void end_simulation(int sig_num)
{
    sigflag = 1;

    printf("---------------  STATISTIKA ----------------");
    long global_inputs_changed_count = 0, global_time_sum = 0, global_wait_time_sum = 0, global_overdue_response_count = 0, global_max_wait = 0;
    int i;
    for(i = 0; i < THREAD_COUNT; i++) {
        float avg_response_time = (float)getTime(messages[i].time_sum) / messages[i].inputs_changed_count;
        float avg_wait_time = (float)getTime(messages[i].wait_time_sum) / messages[i].inputs_changed_count;
        printf("Dretva %d:\n", messages[i].input_data.id);
        printf("\tBroj promijenjenih ulaza: %d\n", messages[i].inputs_changed_count);
        printf("\tProsjecno vrijeme cekanja odgovora: %.3f ms\n", avg_response_time);
        printf("\tProsjecno vrijeme radnog cekanja: %.3f ms\n", avg_wait_time);
        printf("\tBroj zakasnjenih odgovora: %d\n", messages[i].overdue_response_count);
        printf("\tNajdulje cekanje: %ld ms\n", getTime(messages[i].max_wait));

        global_inputs_changed_count += messages[i].inputs_changed_count;
        global_time_sum += getTime(messages[i].time_sum);
        global_wait_time_sum += getTime(messages[i].wait_time_sum);
        global_overdue_response_count += messages[i].overdue_response_count;
        if (global_max_wait < getTime(messages[i].max_wait)) {
            global_max_wait = getTime(messages[i].max_wait);
        }
    }
    float global_avg_response_time = (float)global_time_sum / global_inputs_changed_count;    
    float global_avg_wait_time = (float)global_wait_time_sum / global_inputs_changed_count;    
    printf("Globalna statistika:\n");
    printf("\tBroj promijenjenih ulaza: %ld\n", global_inputs_changed_count);
    printf("\tProsjecno vrijeme cekanja odgovora: %.3f ms\n", global_avg_response_time);
    printf("\tProsjecno vrijeme radnog cekanja: %.3f ms\n", global_avg_wait_time);
    printf("\tBroj zakasnjenih odgovora: %ld\n", global_overdue_response_count);
    printf("\tNajdulje cekanje: %ld ms\n", global_max_wait);
    printf("-------------------------------\n");
    fflush(stdout);
}