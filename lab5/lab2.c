#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>

// Broj dretvi simulatora ulaza
#define THREAD_COUNT 5

// Struktura za ispis statistike na kraju programa
struct logger {
	int zadAOverflow;
	int zadBOverflow;
	int zadCOverflow;
	int zadDOverflow;
	int zadAInterrupted;
	int zadBInterrupted;
	int zadCInterrupted;
	int zadDInterrupted;
	int zadADone;
	int zadBDone;
	int zadCDone;
	int zadDDone;
	int zadATime;
	int zadBTime;
	int zadCTime;
	int zadDTime;
} logs;

int sigflag = 0; 	   // Globalna varijabla za kraj programa
int queue_counter = 0; // Globalna varijabla za praćenje slijeda zadataka

void end_simulation() {
	sigflag = 1;

	printf("---------------  STATISTIKA ----------------\n");
    printf("Grupa A:\n");
    printf("\tBroj obavljenih poslova: %d\n", logs.zadADone);
    printf("\tBroj produljenih zadataka: %d (%.3f%%)\n", logs.zadAOverflow, (float)logs.zadAOverflow/logs.zadADone * 100);
    printf("\tBroj prekinutih zadataka: %d (%.3f%%)\n", logs.zadAInterrupted, (float)logs.zadAInterrupted/logs.zadADone * 100);
    printf("\tProsjecno vrijeme obrade po zadatku: %.3f milisekundi\n", (float)logs.zadATime/logs.zadADone);
	
	printf("Grupa B:\n");
    printf("\tBroj obavljenih poslova: %d\n", logs.zadBDone);
    printf("\tBroj produljenih zadataka: %d (%.3f%%)\n", logs.zadBOverflow, (float)logs.zadBOverflow/logs.zadBDone * 100);
    printf("\tBroj prekinutih zadataka: %d (%.3f%%)\n", logs.zadBInterrupted, (float)logs.zadBInterrupted/logs.zadBDone * 100);
    printf("\tProsjecno vrijeme obrade po zadatku: %.3f milisekundi\n", (float)logs.zadBTime/logs.zadBDone);
	
	printf("Grupa C:\n");
    printf("\tBroj obavljenih poslova: %d\n", logs.zadCDone);
    printf("\tBroj produljenih zadataka: %d (%.3f%%)\n", logs.zadCOverflow, (float)logs.zadCOverflow/logs.zadCDone * 100);
    printf("\tBroj prekinutih zadataka: %d (%.3f%%)\n", logs.zadCInterrupted, (float)logs.zadCInterrupted/logs.zadCDone * 100);
    printf("\tProsjecno vrijeme obrade po zadatku: %.3f milisekundi\n", (float)logs.zadCTime/logs.zadCDone);
	
	printf("Grupa D:\n");
    printf("\tBroj obavljenih poslova: %d\n", logs.zadDDone);
    printf("\tBroj produljenih zadataka: %d (%.3f%%)\n", logs.zadDOverflow, (float)logs.zadDOverflow/logs.zadDDone * 100);
    printf("\tBroj prekinutih zadataka: %d (%.3f%%)\n", logs.zadDInterrupted, (float)logs.zadDInterrupted/logs.zadDDone * 100);
    printf("\tProsjecno vrijeme obrade po zadatku: %.3f milisekundi\n", (float)logs.zadDTime/logs.zadDDone);
	

	int zadGlobalDone = logs.zadADone + logs.zadBDone + logs.zadCDone + logs.zadDDone;
	int zadGlobalInterrupted = logs.zadAInterrupted + logs.zadBInterrupted + logs.zadCInterrupted + logs.zadDInterrupted;
	int zadGlobalOverflow = logs.zadAOverflow + logs.zadBOverflow + logs.zadCOverflow + logs.zadDOverflow;
	int zadGlobalTime = logs.zadATime + logs.zadBTime + logs.zadCTime + logs.zadDTime;
	printf("Globalna Statistika:\n");
    printf("\tBroj obavljenih poslova: %d\n", zadGlobalDone);
    printf("\tBroj produljenih zadataka: %d (%.3f%%)\n", zadGlobalOverflow, (float)zadGlobalOverflow/zadGlobalDone * 100);
    printf("\tBroj prekinutih zadataka: %d (%.3f%%)\n", zadGlobalInterrupted, (float)zadGlobalInterrupted/zadGlobalDone * 100);
    printf("\tProsjecno vrijeme obrade po zadatku: %.3f milisekundi\n", (float)zadGlobalTime/zadGlobalDone);
    fflush(stdout);
}

// Spavanje u milisekundama
int msleep(int msec) {
    struct timespec ts;
    int success;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        success = nanosleep(&ts, &ts); // Cekaj odabrani broj milisekundi
    } while (success && errno == EINTR);

    return success;
}

// Nasumično odaberi zadatak (statički)
int chooseTask() {
	// Postoje kategorije zadataka (A,B,C,D)
	// Jedna kategorija ima neki broj zadataka sa istom periodom
	// Svi zadaci iz svih kategorija se moraju obaviti jednom svake njihove periode
	// Perioda prekida je 100 ms; 1 prekid -> 1 zadatak se obradi
	
	// Perioda zadataka A je 1 s
	int ZadA[5] = {0, 1, 2, 3, 4};
	// Perioda zadataka B je 2 s
	int ZadB[6] = {10, 11, 12, 13, 14, 15};
	// Perioda zadataka C je 4 s
	int ZadC[4] = {100, 101, 102, 103};
	// Perioda zadataka C je 8 s
	int ZadD[3] = {1000, 1001, 1002};

	// U jednoj sekundi moraju se obaviti: 
	// - svi zadaci iz A (500 ms)
	// - pola iz B (300 ms), 
	// - jedan iz C (100 ms)
	// - 0.25 zadatka iz D (100 ms); dodajemo 6 praznih zadataka (svi su oznaceni kao 1002)
	// Redoslijed je proizvoljan i nema ponavljanja zadataka:
	int queue_small[10] = {0, 1, 2, 3, 4, 10, 11, 12, 100, 1000};

	// Puni proizvoljan redoslijed za 8 sekundi:
	int queue[80] = {
		0, 1, 2, 3, 4, 10, 11, 12, 100, 1000,
		0, 1, 2, 3, 4, 13, 14, 15, 101, 1001,
		0, 1, 2, 3, 4, 10, 11, 12, 102, 1002,
		0, 1, 2, 3, 4, 13, 14, 15, 103, 1002,
		0, 1, 2, 3, 4, 10, 11, 12, 100, 1002,
		0, 1, 2, 3, 4, 13, 14, 15, 101, 1002,
		0, 1, 2, 3, 4, 10, 11, 12, 102, 1002,
		0, 1, 2, 3, 4, 13, 14, 15, 103, 1002
	};
	// Ovaj redoslijed se smije permutirati unutar svakog retka te između
	// redaka koji zajedno sadrže cijeli komplet zadataka jedne kategorije
	// npr. 1. i 2. red za zadatke B te 1.,2.,3. i 4. red za zadatke C

	return queue[queue_counter];
}

// Dodavanje podataka za statistiku
int updateLogger(int task, int type, int time) {
	if(task / 1000 == 1) {
		if (type == 0) {
			logs.zadDOverflow += 1;
		} else if (type == 2) {
			logs.zadDDone += 1;
		} else if (type == 3) {
			logs.zadDTime += time;
		} else {
			logs.zadDInterrupted += 1;
		}
	} else if(task / 100 == 1) {
		if (type == 0) {
			logs.zadCOverflow += 1;
		} else if (type == 2) {
			logs.zadCDone += 1;
		} else if (type == 3) {
			logs.zadCTime += time;
		} else {
			logs.zadCInterrupted += 1;
		}
	} else if(task / 10 == 1) {
		if (type == 0) {
			logs.zadBOverflow += 1;
		} else if (type == 2) {
			logs.zadBDone += 1;
		} else if (type == 3) {
			logs.zadBTime += time;
		} else {
			logs.zadBInterrupted += 1;
		}
	} else {
		if (type == 0) {
			logs.zadAOverflow += 1;
		} else if (type == 2) {
			logs.zadADone += 1;
		} else if (type == 3) {
			logs.zadATime += time;
		} else {
			logs.zadAInterrupted += 1;
		}
	}
}

// Nasumično odabire vrijeme trajanja obrade zadatka
int taskProcessTime() {
	int chance = rand() % 100;

    if (chance < 20) {        // 20%
        return 30;  
    } else if (chance < 70) { // 50%
        return 50;
    } else if (chance < 95) { // 25%
        return 80;
    } else {                  // 5%
        return 400; 
    }
}

// Vrati neku jedinstvenu vrijednost, npr. trenutno vrijeme u ms
int uniqueValue() {
	struct timeval  tv;
	gettimeofday(&tv, NULL);

	return (int)((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000);

    //struct timespec spec;
    //clock_gettime(CLOCK_REALTIME, &spec);
	//return round(spec.tv_nsec / 1.0e6);
} 

//globalne varijable za glavnu petlju
int task_id = 0;    		// zadatak_u_obradi = 0 - koji je zadatak trenutno u obradi
int processing_task_id = 0; // posao_u_obradi = 0 - neki broj za razlikovanje, isti zadatak se može ponovno javiti dok prethodni nije gotov
int no_overflow_streak = 0; // bez_prekoračenja = 0 - broj uzastopnih perioda bez prekoračenja
int period_state = 0; 		// perioda = 0 - perioda izvođenja zadatka u obradi
/*
0 - nema obrade
1 - u obradi, regularno u svojoj prvoj periodi
2 - u obradi u drugoj uzastopnoj periodi
*/

// Dretva obrade
int process() {
	int continuation = 0;

	if (task_id != 0 && sigflag == 0) { // Ako već ima neki zadatak
		if (period_state == 1 && no_overflow_streak >= 10) { // Prekidamo ovaj zadatak ili dopuštamo produljenje?
			printf("Dopušteno produljenje obrade zadatka: %d\n", task_id); // zabilježi da je prekinuta obrada zadatka
            fflush(stdout);
			period_state = 2; 		// Namjesti stanje da smo u produljenju
			// no_overflow_streak = 0; // Resetiraj streak bez prekoračenja
			continuation = 1;
			updateLogger(task_id, 0, 0);
		} else if (period_state == 2) {
			period_state = 3;
			no_overflow_streak = 0; // Resetiraj streak bez prekoračenja
			continuation = 1;
			printf("Dopušteno duplo produljenje obrade zadatka: %d\n", task_id); // zabilježi da je prekinuta obrada zadatka
			fflush(stdout);
		} else { // U suprotnom prekini trenutni zadatak i izaberi novi preko petlje ispod
            printf("Prekinuta obrada zadatka zbog prekoračenja vremena: %d\n", task_id); // zabilježi da je prekinuta obrada zadatka
            fflush(stdout);
			updateLogger(task_id, 1, 0);
		}
	}

	// Lokalne varijable za dretvu obrade
	int thread_task = 0; // moj_zadatak = 0
	int thread_work = 0; // moj_posao = 0
	int to_process = 0;  // za_odraditi = 0
	int take_next = 1;   // uzmi_idući = 1

	while (take_next == 1 && sigflag == 0 && continuation == 0) { // Dok ima zastavicu za uzimanje sljedeceg zadatka
		take_next = 0; 									  // Postavi zastavicu na 0 što onemogućuje uzimanje novih zadataka
		task_id = chooseTask(); 						  // Odaberi sljedeći zadatak u slijedu za obradu
		queue_counter = (queue_counter + 1) % 80;
		processing_task_id = uniqueValue(); 			  // Daj jedinstven broj tom zadatku
		thread_task = task_id; 							  // Sinkroniziraj lokalnu i globalnu vrijednost tipa zadatka
		thread_work = processing_task_id; 				  // Sinkroniziraj lokalnu i globalnu vrijednost id zadatka
		period_state = 1; 								  // Postavi stanje da se zadatak u svojoj prvoj periodi obrađuje
		printf("Poč. obrade zadatka: %d\n", thread_task); // zabilježi početak obrade
        fflush(stdout);

		to_process = taskProcessTime(); // Izračunaj trajanje obrade
		updateLogger(task_id, 3, to_process);

		// Prekidanje omogućeno preko processing_task_id (posao_u_obradi) varijable

		// Dokle god su lokalna i globalna vrijednost sinkronizirane 
		// na id-u zadatka (dretva obavlja zadatak koji je zadan) te
		// ima preostalog posla za obraditi, nastavi obrađivati
		while (thread_work == processing_task_id && to_process > 0 && sigflag == 0) {
			msleep(5); // Odradi 5 milisekundi posla
			to_process = to_process - 5; // Oduzmi 5 milisekundi posla od preostalog posla
		}

		// U ovom trenutku, obrada je ili završena ili je prekinuta
		if (thread_work == processing_task_id && to_process <= 0 && sigflag == 0) { // Obrada je završena
			printf("Kraj obrade zadatka: %d\n", thread_task); // zabilježi kraj obrade
        	fflush(stdout);
			updateLogger(task_id, 2, 0);
			task_id = 0; 			  // Resetiraj globalnu varijablu na 0 (nema aktivnog zadatka)
			processing_task_id = 0;   // Resetiraj globalnu varijablu na 0 (isto)
			if (period_state == 1) {  // Ako nije bilo prekoracenja tokom ovog zadatka
				no_overflow_streak++; // Povećaj streak bez prekoračenja
			} else { // Zadatak je prekoračio u drugu periodu 
				take_next = 1; 		  // Odmah mora obraditi zaostatke pa se petlja nastavlja
			}
		} else { // Zadatak/obrada je prekinuta
			return 0;  // izađi iz ove funkcije
		}
	}
}

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
struct message messages[THREAD_COUNT];

// Nasumicno odabere duljinu periode
int random_period() {
    int chance = rand() % 100;
    int msec;

    if (chance < 35) {        // 35%
        msec = 1;  
    } else if (chance < 65) { // 30%
        msec = 2;
    } else if (chance < 85) { // 20%
        msec = 5;
    } else if (chance < 95) { // 10%
        msec = 10;
    } else {                  // 5%
        msec = 20; 
    }
    return msec;
}

// Dretva simulator ulaza "i"`
void * input_simulator_thread(int * arg) {
    int id = *arg;
    srand(time(NULL) + id);

	msleep(messages[id].input_data.starting_delay); // pričekaj početak rada ulaza i "prva pojava"

	while(sigflag == 0) {
		messages[id].response = -1; // poništi prethodni odgovor
		messages[id].input_state = rand() % 900 + 100; // generiraj novo stanje (slučajni broj u rasponu 100-999)
		messages[id].inputs_changed_count += 1; // povećaj broj promjena stanja

        // (a) odgovor treba doći najkasnije nakon jedne periode
		msleep(messages[id].input_data.period); // odgodi izvođenje za jednu periodu
    }

    // Zavrsi dretvu cim zavrsis petlju
    exit(0);
}

// Postavlja podatke za komunikaciju izmedu simulatora ulaza i upravljaca
struct message prepare_thread_data(int id) {
    struct message var;
    var.input_state = -1;
    var.response = -1;
    var.response_timestamp = (struct timeval){0};
    var.inputs_changed_count = 0;
    var.time_sum = (struct timeval){0};
    var.max_wait = (struct timeval){0};
    var.overdue_response_count = 0;

    struct input_data_structure input_data;
    input_data.id = id;
    input_data.period = random_period();
    input_data.starting_delay = random_period();
    var.input_data = input_data;

    return var;    
}



int main() {
	logs.zadAOverflow = 0;
	logs.zadBOverflow = 0;
	logs.zadCOverflow = 0;
	logs.zadDOverflow = 0;
	logs.zadAInterrupted = 0;
	logs.zadBInterrupted = 0;
	logs.zadCInterrupted = 0;
	logs.zadDInterrupted = 0;
	logs.zadADone = 0;
	logs.zadBDone = 0;
	logs.zadCDone = 0;
	logs.zadDDone = 0;
	logs.zadATime = 0;
	logs.zadBTime = 0;
	logs.zadCTime = 0;
	logs.zadDTime = 0;
    signal(SIGINT, end_simulation);

	int i, success;
    for(i = 0; i < THREAD_COUNT; i++) {
        messages[i] = prepare_thread_data(i);
        success = pthread_create(&threads[i], NULL, (void *)input_simulator_thread, (void *)&(messages[i].input_data.id));
        if (success != 0) {
            printf("Pogreška pri stvaranju dretve\n");
            exit(1);
        }
    }

	pthread_t worker_thread;

	while(sigflag == 0) { // Ili dok se ne obrade svi zadaci???
		msleep(100);
		success = pthread_create(&worker_thread, NULL, (void *)process, NULL);
		if (success != 0) {
			printf("Pogreška pri stvaranju dretve\n");
			exit(1);
    	}
	}

    // Cekaj da upravljacka petlja zavrsi
    exit(0);
}