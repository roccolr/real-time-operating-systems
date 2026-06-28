# RTPOSIX 
È il più popolare standard per quanto concerne i sistemi operativi real time. Lo standard specifica primitive per:
1. programmazione concorrente;
2. mutua esclusione con meccanismi di PI;
3. sincronizzazione con conditions variables;
4. code di messaggi _prioritizzate_ per la comunicazione inter-task.

Di seguito un richiamo (produttore-consumatore) di sincronizzazione con condition variables.

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 5

// Risorse condivise
int buffer[BUFFER_SIZE];
int count = 0;

// Primitive di sincronizzazione
pthread_mutex_t mutex;
pthread_cond_t cond_spazio_disponibile; // Il buffer ha slot liberi
pthread_cond_t cond_dati_disponibili;   // Il buffer ha dati da leggere

void* produttore(void* arg) {
    for (int i = 0; i < 10; ++i) {
        pthread_mutex_lock(&mutex);

        // Uso il 'while' e non 'if' per difendermi da risvegli spuri (spurious wakeups)
        while (count == BUFFER_SIZE) {
            printf("[Produttore] Buffer pieno. In attesa...\n");
            pthread_cond_wait(&cond_spazio_disponibile, &mutex);
        }

        // Sezione Critica: Inserimento dato
        buffer[count] = i;
        count++;
        printf("[Produttore] Inserito: %d (Elementi: %d)\n", i, count);

        // Segnala al consumatore che ora ci sono dati disponibili
        pthread_cond_signal(&cond_dati_disponibili);
        
        pthread_mutex_unlock(&mutex);

        sleep(1); // Simula il tempo di produzione
    }
    return NULL;
}

void* consumatore(void* arg) {
    for (int i = 0; i < 10; ++i) {
        pthread_mutex_lock(&mutex);

        // Attende finché il buffer è vuoto
        while (count == 0) {
            printf("[Consumatore] Buffer vuoto. In attesa...\n");
            pthread_cond_wait(&cond_dati_disponibili, &mutex);
        }

        // Sezione Critica: Estrazione dato (LIFO in questo esempio elementare)
        int item = buffer[count - 1];
        count--;
        printf("[Consumatore] Letto: %d (Elementi: %d)\n", item, count);

        // Segnala al produttore che ora si è liberato spazio
        pthread_cond_signal(&cond_spazio_disponibile);
        
        pthread_mutex_unlock(&mutex);

        sleep(2); // Simula il tempo di elaborazione (più lento)
    }
    return NULL;
}

int main() {
    pthread_t thread_prod, thread_cons;

    // Inizializzazione di mutex e condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_spazio_disponibile, NULL);
    pthread_cond_init(&cond_dati_disponibili, NULL);

    // Creazione dei thread
    pthread_create(&thread_prod, NULL, produttore, NULL);
    pthread_create(&thread_cons, NULL, consumatore, NULL);

    // Attesa terminazione
    pthread_join(thread_prod, NULL);
    pthread_join(thread_cons, NULL);

    // Pulizia delle risorse
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_spazio_disponibile);
    pthread_cond_destroy(&cond_dati_disponibili);

    return 0;
}
```

## Astrazioni temporali 
Un *clock* è un'astrazione che modella un'entità che fornisce il tempo corrente. Risponde alla domanda "che ora è?". 
Un *timer* è un'atrazione che modella un'entità che può generare eventi ad un determinato istante (interruzioni, segnali, etc.). 
I timer possono essere di tipo one-shot, ovver un timer che scade una volta solo e dopo viene disabilitato, oppure periodico, ovvero un timer che una volta scaduto viene ricaricato con lo stesso valore periodico e continua il conteggio.
Le API Unix tradizionali riservano ad ogni processo tre e solo tre tipi di timer:
1. Real time: tempo reale del sistema (wall clock) - ITIMER_REAL;
2. Virtual: tempo virtuale del processo, intero incrementato solo quando il processo sta eseguendo in modalità utente - ITIMER_VIRTUAL;
3. Profiling: tempo virtuale incrementato anche quando il kernel sta eseguendo per conto del processo - ITIMER_PROF. 

Osserviamo che esiste un solo real time timer per ogni processo, e che ognuno di questi timer è collegato a tre clock diversi. 

```c
#include <sys/time.h>

int settimer(int which, const struct itimerval *new_value, struct itimerval *old_value);

struct itimerval{
    struct timeval it_interval; // intervallo per timer periodico 
    struct timeval it_value; // tempo alla prossima scadenza
}
```

Osserviamo che le primitive POSIX compliant sono incluse con ```#include <time.h>```.

## Programmazione di un task periodico 
Pseudocodice per un task che inizia dopo 2 s, e cicla ogni 5 ms. 
```c
int main(){
    start_periodic_timer(2000000, 5000);
    while(1){
        wait_next_activation();
        job_body();
    }
    return 0;
}
```

Dobbiamo capire come implementare start_periodic_timer e wait_next_activation. 

### Soluzione 1
Usare la sleep:

```c
static long next_period;
static int period;

void start_periodic_timer(utin64_t offs, int t){
    struct timeval t1;
    gettimeofday(&t1, NULL);
    long now = t1.tv_sec*1000000 + t1.tv_usec;
    next_period = now+offs;
    period = t;
}

void wait_next_activation(void){
    struct timeval t1;
    gettimeofday(&t1, NULL);
    long now = t1.tv_sec*1000000 + t1.tv_usec;
    long delay = next_period - now;
    next_period += period;
    usleep(delay);
}
```

Questa rappresenta una soluzione pericolosa perchè può accadere una prelazione del task tra il retrieve del tempo e la sleep, causando un drift temporale permanente. 

### Soluzione 2
Usare i timers:

```c
#define wait_next_activation pause
static void sighand(int s){ }

void start_periodic_timer(utin64_t offs, int period){
    struct itimerval t;
    t.it_value.tv_sec = offs/1000000:
    t.it_value.tv_usec = offs%1000000;
    t.it_interval.tv_sec = period/1000000;
    t.it_interval.tv_usec = period%1000000; 

    signal(SIGALARM, sighand);
    setitimer(ITIMER_REAL, &t, NULL);
}


void wait_next_activation(void){
    struct timeval t1;
    gettimeofday(&t1, NULL);
    long now = t1.tv_sec*1000000 + t1.tv_usec;
    long delay = next_period - now;
    next_period += period;
    usleep(delay);
}
```

Questa rappresenta una soluzione pericolosa perchè può accadere una prelazione del task tra il retrieve del tempo e la sleep, causando un drift temporale permanente. 