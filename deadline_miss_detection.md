![alt text](/pics/deadline_miss_detection.png "deadline_miss_detection")

# Pattern Architetturale: Deadline Miss Detection (RT-POSIX)

Questo codice in C implementa un pattern architetturale standard per i sistemi Real-Time (basato su RT-POSIX). Il suo obiettivo è rilevare se un thread supera il tempo massimo consentito per completare il suo lavoro (Deadline Miss) e attivare una procedura di emergenza.

Il sistema si basa su tre entità logiche cooperanti: **Monitor**, **Server** e **Watchdog**, che comunicano tramite timer hardware e segnali POSIX asincroni.

---

## 1. Il Monitor (Setup e Inizializzazione)

La funzione `monitor()` funge da orchestratore. Non esegue il lavoro pratico, ma prepara l'infrastruttura di sicurezza e avvia il thread lavoratore.

### Fasi operative:

1. **Configurazione del Segnale (Watchdog):**
   Utilizza `SIGACTION` per istruire il kernel: *"Se ricevi un segnale `SIGALRM`, interrompi l'esecuzione corrente e salta alla funzione `watchdog_handler`"*.
2. **Creazione e Armamento del Timer:**
   Configura un timer hardware di tipo *one-shot* (singolo scatto). 
   Viene impostata la variabile `alarm_time.it_value = deadline;`. Tramite `TIMER_SETTIME`, il timer hardware viene armato. Da questo istante, il tempo inizia inesorabilmente a scorrere all'indietro.
3. **Avvio del Lavoratore (Server):**
   Utilizza `PTHREAD_CREATE` per generare un nuovo thread esecutivo, associandolo alla funzione `server`. Passa al thread il puntatore al timer appena creato.

---

## 2. Il Server (Il Task Lavoratore)

La funzione `server()` rappresenta l'attività applicativa vera e propria (es. elaborazione di un dato sensoriale, calcolo di una traiettoria).

```c
void server(timer_t *watchdog) {
    /* perform required service */
    TIMER_DELETE(*watchdog);
}
```

Mentre il blocco /* perform required service */ è in esecuzione, il timer hardware precedentemente armato continua il suo conto alla rovescia. Da qui si aprono due scenari deterministici.

1. Successo (Deadline Rispettata), Il timer viene distrutto (l'allarme è disinnescato). Il sistema prosegue il suo normale flusso di esecuzione.
2. Fallimento (Deadline Miss), L'hardware genera un interrupt. Il kernel invia immediatamente il segnale SIGALRM al processo.

## 3. Il Watchdog (Gestione dell'Errore)
```c
void watchdog_handler(int signum, siginfo_t *data, void *extra) {
    /* SIGALRM handler */
    
    /* server is late: undertake recovery */
}
```

Questo gestore interrompe asincronamente l'esecuzione. Nello spazio /* undertake recovery */, il programmatore inserisce le routine di sicurezza, come:

1. Terminazione forzata (kill) del thread server rimasto bloccato.

2. Attivazione di un task di graceful degradation (es. modalità provvisoria o safe-state).

3. Registrazione dell'evento critico nei log di diagnostica.

Grazie a questo pattern, i vincoli temporali sono garantiti dall'hardware e dal sistema operativo, rendendo il sistema resiliente anche in caso di crash totale del codice applicativo.
