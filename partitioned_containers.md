# Container Real-Time, Container Partizionati e RunPHI
## Una trattazione completa: dallo stack container tradizionale all'orchestrazione mixed-criticality

---

## Indice

1. [Introduzione e contesto](#1-introduzione-e-contesto)
2. [Il container "normale": anatomia dell'intero stack](#2-il-container-normale-anatomia-dellintero-stack)
3. [I container real-time: le famiglie di soluzioni](#3-i-container-real-time-le-famiglie-di-soluzioni)
4. [Il container partizionato](#4-il-container-partizionato)
5. [RunPHI: cos'è e dove si colloca nello stack](#5-runphi-cosè-e-dove-si-colloca-nello-stack)
6. [L'architettura software di runPHI: diagramma a blocchi commentato](#6-larchitettura-software-di-runphi-diagramma-a-blocchi-commentato)
7. [Mappatura runPHI ↔ runc: il ciclo di vita OCI a confronto](#7-mappatura-runphi--runc-il-ciclo-di-vita-oci-a-confronto)
8. [Conclusioni](#8-conclusioni)
9. [Riferimenti](#9-riferimenti)

---

## 1. Introduzione e contesto

Negli ultimi quindici anni i container si sono affermati come l'unità standard di impacchettamento, distribuzione e deployment del software nel cloud. Il loro successo deriva da una combinazione vincente di leggerezza (avvio in millisecondi, overhead minimo rispetto alle macchine virtuali), portabilità (l'immagine incapsula applicazione e dipendenze) e da un ecosistema di orchestrazione maturo (Docker, Kubernetes). Tuttavia, il modello di isolamento su cui i container si fondano — la condivisione del kernel dell'host tra tutti i container — è stato progettato per il mondo IT general-purpose, dove ciò che conta è il throughput medio e non il rispetto di scadenze temporali.

Il mondo industriale, automotive, ferroviario e in generale dei sistemi cyber-fisici ha esigenze radicalmente diverse: applicazioni di controllo con vincoli real-time hard, requisiti di certificazione di sicurezza funzionale (IEC 61508, ISO 26262, DO-178C), e soprattutto la coesistenza sullo stesso hardware di applicazioni a criticità diversa — il cosiddetto scenario *mixed-criticality*. La convergenza tra IT e OT (Operational Technology), spinta dai paradigmi di edge computing e cloud industriale (il "Real-Time Cloud"), pone quindi una domanda: è possibile portare la comodità operativa dei container — `docker run`, Kubernetes, rolling upgrade — nel dominio dei sistemi con vincoli temporali e di sicurezza stringenti?

Questa trattazione risponde alla domanda in quattro passi. Prima ricostruiamo con precisione come funziona un container tradizionale, dall'interfaccia utente di Docker fino al kernel, perché è su quello stack che tutto il resto si innesta. Poi esaminiamo le proposte esistenti per i container real-time e ne evidenziamo la frammentazione. Quindi approfondiamo il concetto di *container partizionato*, che sposta l'isolamento dal kernel a un hypervisor di partizionamento. Infine presentiamo RunPHI (runΦ), il runtime OCI sviluppato per mappare in modo trasparente i container su backend di isolamento eterogenei in funzione della loro criticità, analizzandone l'architettura software blocco per blocco e mappandola punto per punto sull'architettura nota di runc.

---

## 2. Il container "normale": anatomia dell'intero stack

### 2.1 Cos'è davvero un container

Contrariamente all'intuizione suggerita dalla metafora, un container **non è un oggetto** che esiste nel sistema: è un **processo Linux ordinario** (o un gruppo di processi) al quale il kernel presenta una vista ristretta e controllata delle risorse di sistema. Non esiste un "container" nel kernel Linux; esistono meccanismi di isolamento e di controllo delle risorse che, combinati, producono l'illusione di un sistema operativo dedicato. I tre pilastri sono i seguenti.

**I namespace** realizzano l'*isolamento della visibilità*: ogni namespace virtualizza una risorsa globale del kernel, facendo sì che i processi al suo interno vedano una copia privata di quella risorsa. Il namespace PID dà al container il proprio albero di processi (il suo init è il PID 1 locale); il namespace di rete gli fornisce stack TCP/IP, interfacce e tabelle di routing proprie; il namespace di mount gli dà un filesystem privato; e analogamente per UTS (hostname), IPC, user (mappatura degli UID) e cgroup.

**I control group (cgroups)** realizzano il *controllo delle risorse*: limitano e contabilizzano quanto un gruppo di processi può consumare in termini di CPU (quote e pesi sullo scheduler CFS, o banda garantita per le classi real-time tramite gli rt-cgroups), memoria, I/O a blocchi, numero di PID. Sono i cgroups a impedire che un container affamato di risorse strangoli i vicini — almeno *in media*, come vedremo.

**Il filesystem a layer (overlayfs)** realizza l'*impacchettamento*: un'immagine OCI è una pila di layer read-only più metadati; a runtime overlayfs li fonde in un'unica vista e aggiunge in cima un layer scrivibile ed effimero, privato del container. A questi tre pilastri si sommano i meccanismi di riduzione della superficie d'attacco: capabilities (frammentazione dei privilegi di root), seccomp (filtri sulle system call ammesse) e moduli LSM come AppArmor o SELinux.

Il punto essenziale da fissare, perché è la radice di tutti i problemi real-time discussi più avanti, è che **tutti i container di un host condividono lo stesso, identico kernel**. Ogni system call di ogni container entra nello stesso codice del kernel, contende gli stessi lock, gli stessi interrupt handler, le stesse cache hardware, la stessa banda di memoria.

### 2.2 Lo stack completo: da `docker run` al kernel

Quando un utente digita `docker run`, la richiesta attraversa una catena di componenti ben stratificata, che è utile visualizzare per intero perché **RunPHI si innesta esattamente in fondo a questa catena**:

```
┌─────────────────────────────────────────────────────────────────┐
│  UTENTE / ORCHESTRATORE                                         │
│  docker CLI          kubectl → kube-apiserver → kubelet         │
└───────────┬─────────────────────────────┬───────────────────────┘
            │ REST API                    │ CRI (gRPC)
┌───────────▼───────────┐     ┌───────────▼───────────────────────┐
│  dockerd              │     │  containerd            CRI-O      │
│  (Docker Engine)      ├────►│  - gestione immagini              │
│                       │gRPC │  - gestione snapshot (overlayfs)  │
└───────────────────────┘     │  - ciclo di vita dei container    │
                              └───────────┬───────────────────────┘
                                          │ exec (fork del processo)
                              ┌───────────▼───────────────────────┐
                              │  containerd-shim-runc-v2          │
                              │  (un processo per container:      │
                              │   tiene stdio, raccoglie exit     │
                              │   code, sopravvive a containerd)  │
                              └───────────┬───────────────────────┘
                                          │ exec: runc <comando OCI>
                              ┌───────────▼───────────────────────┐
                              │  runc  ◄── RUNTIME OCI (livello   │
                              │            in cui vive RunPHI)    │
                              │  legge il bundle OCI e crea:      │
                              │  namespaces + cgroups + mounts    │
                              └───────────┬───────────────────────┘
                                          │ syscall (clone, unshare,
                                          │ pivot_root, setns, ...)
                              ┌───────────▼───────────────────────┐
                              │  KERNEL LINUX (condiviso!)        │
                              │  namespaces │ cgroups │ overlayfs │
                              └───────────────────────────────────┘
```

Percorriamo la catena dall'alto verso il basso.

Il **client** (docker CLI oppure, nel mondo Kubernetes, il kubelet) esprime l'intento: "esegui questa immagine con questi parametri". Il **Docker Engine (dockerd)** espone l'API REST, gestisce build, volumi e networking di alto livello, ma da anni delega l'intero ciclo di vita dei container a containerd via gRPC. Il **containerd** (o l'alternativa CRI-O, nata per Kubernetes) è il *container manager*: scarica le immagini dal registry secondo la OCI *image-spec*, ne estrae i layer, prepara lo snapshot overlayfs e — momento cruciale — **prepara il bundle OCI**.

Il **bundle OCI** è il contratto tra il mondo di alto livello e il runtime: una directory contenente il `rootfs/` (il filesystem del container, già montato) e un file **`config.json`** che descrive, secondo la OCI *runtime-spec*, tutto ciò che il runtime deve fare: processo da eseguire, variabili d'ambiente, namespace da creare, limiti cgroup, mount, capabilities, hook, annotazioni. Si noti fin d'ora che questo `config.json` *del bundle* è cosa diversa dal `/boot/config.json` *dentro il rootfs* che incontreremo con runPHI.

Lo **shim** (containerd-shim-runc-v2) è un piccolo processo demone, uno per container, che fa da genitore adottivo del container: mantiene aperti i canali di I/O, raccoglie l'exit status, e disaccoppia la vita del container da quella di containerd (che può essere riavviato senza uccidere i container).

Infine **runc**, il runtime OCI di riferimento: un binario che implementa i comandi del ciclo di vita definito dalla runtime-spec — `create`, `start`, `kill`, `delete`, `state` — e che a `create` compie il lavoro sporco: invoca `clone()`/`unshare()` con i flag dei namespace richiesti, scrive nei filesystem dei cgroup, esegue `pivot_root` sul rootfs, applica seccomp e capabilities, e lascia il processo init del container congelato in attesa. A `start` sblocca l'init, che esegue la `execve` dell'entrypoint. Fatto questo, **runc termina**: è un processo effimero, non un demone. Lo stato del container vive su disco (tipicamente sotto `/run/containerd/` o `/run/runc/`) ed è il kernel a fare il resto.

### 2.3 Il punto di aggancio: la sostituibilità del runtime OCI

La genialità architetturale della standardizzazione OCI sta nel fatto che **qualunque binario che rispetti la runtime-spec può prendere il posto di runc** senza che nulla, al di sopra, se ne accorga. containerd non sa (e non vuole sapere) *come* il container venga isolato: sa solo che invocando `runtime create` con un bundle otterrà un container, e che `state` gli risponderà con un JSON di stato. Su questo punto di aggancio sono nati i runtime alternativi: **gVisor (runsc)**, che intercetta le syscall in un kernel user-space; **Kata Containers**, che avvolge il container in una micro-VM; **RunX**, che lo esegue come VM Xen; e, come vedremo, **RunPHI**, che lo trasforma in una partizione a interferenza zero. Tenete a mente questa proprietà: è l'intera tesi architetturale di runPHI.

### 2.4 Perché il container normale non basta per il real-time

Il modello descritto fallisce sui requisiti temporali per una ragione strutturale: namespaces e cgroups isolano la *visibilità* e limitano il consumo *medio* di risorse, ma **non danno alcuna garanzia sul comportamento temporale nel caso peggiore (WCET)**. Le fonti di interferenza restano molteplici. Il kernel condiviso è la prima: un container può provocare tempeste di interrupt, contendere lock del kernel, invocare percorsi non prelazionabili che ritardano i task degli altri container. Lo scheduler CFS ottimizza l'equità del throughput, non le scadenze. E anche isolando le CPU, l'interferenza scende ai livelli microarchitetturali: cache di ultimo livello condivisa, banda del controller di memoria, prefetcher, bus di I/O — tutti canali attraverso cui un container "best-effort" può degradare, in modo invisibile ai cgroups, il tempo di risposta di un container critico. Infine, un kernel Linux general-purpose da milioni di righe di codice è, per gli enti di certificazione di sicurezza funzionale, un oggetto di fatto non certificabile ai livelli di integrità più alti.

Da qui la fioritura di proposte per i "container real-time", che è il tema della prossima sezione.

---

## 3. I container real-time: le famiglie di soluzioni

Il materiale del corso sintetizza il panorama in quattro famiglie, ordinabili per crescente grado di isolamento (e, tendenzialmente, per crescente rinuncia alla comodità del modello Linux puro).

### 3.1 RT-Linux: container su kernel PREEMPT_RT

La prima strada è rendere real-time il kernel condiviso stesso. Si esegue lo stack container su un kernel Linux con patch **PREEMPT_RT** (che rende prelazionabile quasi tutto il kernel, trasforma gli interrupt handler in thread schedulabili e converte gli spinlock in mutex con priority inheritance) e si schedulano i container con la politica **SCHED_DEADLINE**, un vero scheduler EDF/CBS che assegna a ogni entità una terna (runtime, deadline, period). Il supporto al *group scheduling* tramite **rt-cgroups** permette di dare garanzie di banda CPU a interi container e non solo a singoli thread; a questo si sommano le tecniche classiche di partizionamento soft: **CPU affinity** per inchiodare i container a core specifici e **isolcpus** per sottrarre core allo scheduler generale e dedicarli ai carichi critici.

È la soluzione più trasparente (le immagini restano ordinarie, lo stack Docker/Kubernetes non cambia) e adatta al soft real-time, ma il kernel resta unico e condiviso: le latenze nel caso peggiore restano nell'ordine delle decine di microsecondi, l'interferenza microarchitetturale non è governata, e il problema della certificabilità rimane intatto.

### 3.2 Dual-kernel: container mappati su Xenomai o RTAI

La seconda famiglia affianca a Linux un **co-kernel real-time** (Xenomai, RTAI) che prende il controllo diretto degli interrupt: i task real-time vivono nel co-kernel, che schedula Linux stesso come task in background a priorità minima. Mappare un container su questo mondo significa fare in modo che i suoi task critici girino nel dominio del co-kernel, ottenendo latenze da hard real-time (pochi microsecondi). Il prezzo è un modello di programmazione dedicato (API specifiche o skin POSIX parziali), una manutenzione delle patch storicamente onerosa e una separazione dei domini che complica l'integrazione con lo stack container standard.

### 3.3 Sandbox: container come lightweight VM

La terza famiglia rinuncia al kernel condiviso: il container viene eseguito dentro una **macchina virtuale leggera**, con un kernel guest proprio. Vi appartengono **RunX** (il container diventa una VM Xen, con kernel minimale fornito dal runtime), **Firecracker** (microVM KVM con dispositivo model minimale, nato per il serverless), **gVisor** (che pur non essendo una VM interpone un kernel user-space tra container e host) e i **container partizionati**, introdotti dal gruppo di ricerca degli autori delle slide (Barletta et al., DSN 2023) e approfonditi nella sezione 4. Il salto qualitativo è che l'isolamento diventa mediato da un hypervisor — un componente ordini di grandezza più piccolo e analizzabile di un kernel Linux — e che le risorse possono essere assegnate in modo esclusivo.

### 3.4 Baremetal: container su acceleratori

L'ultima famiglia porta il carico critico *fuori* dai core applicativi: il "container" viene eseguito **bare-metal su un co-processore**, tipicamente una **RPU** (Real-time Processing Unit, ad esempio i core ARM Cortex-R degli MPSoC Xilinx/AMD Zynq UltraScale+), sotto forma di applicazione **Zephyr** o binario nudo. Qui l'isolamento è fisico: silicio dedicato, nessuna contesa di scheduler, latenze deterministiche da microcontrollore. In cambio si perde quasi tutto il modello container classico: niente kernel Linux nel guest, toolchain dedicata, risorse fisse.

### 3.5 La sfida: frammentazione e trasparenza

Il quadro che emerge è una scala di soluzioni con isolamento crescente — dal RT-Linux al bare-metal — ma **tra loro incompatibili**: ognuna richiede formati, toolchain, procedure di deployment e competenze diverse. La domanda posta dal materiale del corso è quindi la sfida fondante del progetto RunPHI:

> **È possibile mappare in modo *trasparente* un container su soluzioni di isolamento così diverse, in funzione dei suoi requisiti di criticità?**

In uno scenario mixed-criticality reale la risposta deve essere sì per tutti i livelli contemporaneamente: sullo stesso nodo devono convivere il container di data-analytics (criticità bassa → RT-Linux basta e avanza), il loop di controllo soft real-time (criticità media → sandbox partizionata) e il controllore di sicurezza (criticità alta → bare-metal su RPU), e l'operatore deve poterli gestire tutti con gli stessi strumenti: `docker run`, Kubernetes, le stesse immagini. Prima di vedere come RunPHI realizza questo, serve capire il mattone su cui poggia il livello di isolamento intermedio e alto: il container partizionato.

---

## 4. Il container partizionato

### 4.1 L'idea: sostituire il kernel condiviso con un hypervisor di partizionamento

Un **hypervisor di partizionamento** (o *static partitioning hypervisor*) è un hypervisor minimale che rinuncia deliberatamente alla flessibilità degli hypervisor cloud (overcommit, migrazione live, scheduling dei vCPU) in cambio del determinismo: la piattaforma hardware viene **divisa staticamente in partizioni (celle)**, ciascuna con CPU fisiche, regioni di memoria e dispositivi di I/O assegnati in modo **esclusivo**. L'esempio di riferimento è **Jailhouse**: si avvia dentro Linux, poi "solleva" se stesso sotto il sistema, confina Linux nella *root cell* e ritaglia *inmate cell* a cui cede pezzi di hardware. Non c'è scheduling tra celle: ogni pCPU appartiene a una sola cella, per sempre (mapping 1:1). Anche **Xen**, opportunamente configurato (vCPU pinnate, dom0 minimale, scheduler null), può operare in questa modalità. L'isolamento così ottenuto è **spaziale** (protezione della memoria via IOMMU/SMMU e second-stage translation) e **temporale** (nessuna contesa di CPU per costruzione; l'interferenza microarchitetturale residua si mitiga con tecniche come il *cache coloring*, che partiziona la cache di ultimo livello colorando le pagine di memoria assegnate a ciascuna cella).

Il **container partizionato** (Barletta et al., "Partitioned Containers: Towards Safe Clouds for Industrial Applications", DSN 2023) è la fusione dei due mondi: un carico di lavoro che viene **impacchettato, distribuito e gestito come un container OCI** (immagine su registry, `docker run`, orchestrazione), ma che a runtime viene **istanziato come una cella dell'hypervisor di partizionamento** invece che come un processo del kernel condiviso. L'evoluzione del concetto è lo **ZIC — Zero-Interference Container** (Ottaviano, Barletta, Boccola, "Zero-Interference Containers: A Framework to Orchestrate Mixed-Criticality Applications", DSN 2025): un container la cui esecuzione, per costruzione, non subisce e non genera interferenza verso le altre partizioni.

### 4.2 L'architettura di riferimento (il backend Jailhouse)

La slide sul backend Jailhouse di RunPHI mostra l'architettura completa di un nodo a container partizionati, che conviene descrivere strato per strato:

```
        descrizioni dei container partizionati (YAML dell'utente)
   ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
   │  PartitionA   │ │  PartitionB   │ │  PartitionC   │
   │ criticality = │ │ criticality = │ │ criticality = │
   │     LOW       │ │     MID       │ │     HIGH      │
   └───────┬───────┘ └───────┬───────┘ └───────┬───────┘
           └─────────────────┼─────────────────┘
 Piattaforme di              │
 orchestrazione ──► OCI runtime ──► RunΦ
                                     │
┌──────────────────┐  ┌───────────┐  ┌────────────┐  ┌────────────┐
│ Partizione       │  │Partizione │  │ Partizione │  │ Partizione │
│ privilegiata     │  │    A      │  │     B      │  │     C      │
│ (Linux + RunΦ)   │  │ Container │  │   RTOS /   │  │ Bare-metal │
│ partition ops:   │  │ ramdisk + │  │ unikernel  │  │    APP     │
│ create, destroy, │  │  kernel   │  │            │  │            │
│ load, shutdown,  │  │ 4 vCPU    │  │  1 vCPU    │  │            │
│ start, stats     │  └─────┬─────┘  └─────┬──────┘  └─────┬──────┘
└────────┬─────────┘        │              │               │
┌────────▼──────────────────▼──────────────▼───────────────▼──────┐
│                 HYPERVISOR DI PARTIZIONAMENTO                   │
└────────┬──────────────────┬──────────────┬───────────────┬──────┘
┌────────▼───────┐ ┌────────▼───────┐ ┌────▼─────────┐ ┌───▼──────────┐
│ I/O dedicati   │ │ I/O dedicati   │ │ Colored mem. │ │ Colored mem. │
│ Memoria        │ │ Memoria        │ │ pCPU general │ │ pCPU real-   │
│ pCPU general-  │ │ pCPU general-  │ │ purpose, GPU │ │ time (1:1),  │
│ purpose        │ │ purpose        │ │ (map 1:1)    │ │ FPGA         │
└────────────────┘ └────────────────┘ └──────────────┘ └──────────────┘
                              HARDWARE
```

In cima sta l'**utente**, che descrive i propri container in file di deployment (YAML) arricchiti di un attributo chiave: la **criticità** (LOW, MID, HIGH). Le **piattaforme di orchestrazione** (Kubernetes e simili) restano invariate e parlano con il nodo attraverso il consueto **OCI runtime** — che qui è **RunΦ**, ospitato in una **partizione privilegiata**: la cella Linux di gestione, l'unica autorizzata a invocare le *partition ops* dell'hypervisor (`create`, `destroy`, `load`, `start`, `shutdown`, `stats`). È l'analogo funzionale della root cell di Jailhouse o del dom0 di Xen.

Sotto, le partizioni applicative mostrano il ventaglio di ciò che un container partizionato può diventare, in funzione della criticità dichiarata. La **Partizione A** (criticità bassa) contiene un container "quasi normale": più applicazioni sopra un kernel Linux minimale avviato da ramdisk, con più vCPU — la comodità del modello container, ma dentro una cella con memoria e CPU dedicate. La **Partizione B** (criticità media) esegue lo stesso carico su un **RTOS o unikernel**, con footprint e latenza ridotti. La **Partizione C** (criticità alta) è un'**applicazione bare-metal** che possiede direttamente il suo hardware. In fondo, l'hardware evidenzia i meccanismi di isolamento fisico: pCPU **mappate 1:1** (mai condivise), **colored memory** per il partizionamento della cache, dispositivi di I/O, GPU e FPGA assegnati in esclusiva alle celle che ne hanno bisogno.

Due proprietà della slide meritano enfasi. Primo: la piattaforma **distingue i container per criticità** — lo stesso strumento (RunΦ) piazza carichi LOW, MID e HIGH su meccanismi diversi. Secondo: **lo stesso codice può girare partizionato su Jailhouse oppure migrare su un edge cloud come container standard** — con meno garanzie, ma senza modifiche. È la portabilità del formato OCI messa al servizio del continuum criticità/prestazioni.

### 4.3 Cosa si guadagna e cosa si paga

Il guadagno è il determinismo: isolamento temporale per costruzione, interferenza microarchitetturale mitigata, una TCB (Trusted Computing Base) minuscola e analizzabile ai fini della certificazione, e la possibilità di far convivere sullo stesso silicio domini certificati e domini best-effort. Il prezzo è la staticità: le risorse assegnate a una cella sono sottratte al resto del sistema anche quando inutilizzate (niente overcommit, densità inferiore), la creazione di una cella richiede una configurazione hardware dettagliata e specifica della piattaforma (regioni di memoria, interrupt, dispositivi), e il guest non dispone dei servizi di un Linux completo, a meno di non caricarne uno nella cella. È esattamente la complessità di quella configurazione — scrivere a mano i *cell file* di Jailhouse o i config di Xen — che RunPHI si propone di automatizzare e nascondere dietro l'interfaccia OCI.

---

## 5. RunPHI: cos'è e dove si colloca nello stack

### 5.1 Definizione e collocazione architetturale

**RunPHI (runΦ)** è un **runtime OCI alternativo, scritto in Rust**, che risponde alla sfida della sezione 3.5: prendere container OCI ordinari e mapparli, in modo trasparente per tutto ciò che sta sopra, su backend di isolamento eterogenei — dal RT-Linux al bare-metal su RPU — in funzione della criticità. La sua collocazione nello stack è la chiave di tutto e va detta senza ambiguità: **RunPHI vive esattamente al livello di runc**, cioè al gradino più basso dello stack container in user-space, subito sopra il confine con kernel/hypervisor. Riprendendo il diagramma della sezione 2.2, RunPHI non tocca nulla dei livelli superiori:

```
                    container (immagini OCI ordinarie)
      ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐
      └────┘ └────┘ └────┘ └────┘ └────┘ └────┘ └────┘ └────┘
┌────────────────────────────────────────────────────────────────┐
│           INTERFACCIA OCI  (containerd / CRI-O / docker)       │   ◄── invariata
└──────────────────────────────┬─────────────────────────────────┘
┌──────────────────────────────▼─────────────────────────────────┐
│                     RunΦ   (livello di runc)                   │
│              https://github.com/runphi                         │
└──┬───────────┬────────────┬───────────────┬───────────────┬────┘
┌──▼───────┐ ┌─▼─────────┐ ┌▼──────────┐ ┌──▼───────────┐ ┌─▼───────────┐
│ RT-Linux │ │ Dual-     │ │  RunX     │ │ Partitioned  │ │ Baremetal   │
│          │ │ kernel    │ │ (Xen VM)  │ │ container    │ │ su MPSoC    │
└──────────┘ └───────────┘ └───────────┘ └──────────────┘ └─────────────┘
   ─────────────── ISOLATION ASSURANCE crescente ──────────────►
```

Docker, containerd, CRI-O, kubelet continuano a invocare "runc" con gli stessi comandi e lo stesso bundle di sempre; è il binario sottostante a essere stato sostituito (lo script di installazione `switch_to_runphi.sh` rimpiazza il runc di sistema con runPHI, salvando l'originale come `runc_vanilla`). Sotto RunPHI, invece dei soli namespaces del kernel, si apre il ventaglio dei **backend**, ordinati lungo l'asse dell'*isolation assurance*: RT-Linux, dual-kernel, RunX, container partizionato, bare-metal su MPSoC.

### 5.2 I benefici del modello

Dalla posizione che occupa, RunPHI ricava una serie di proprietà notevoli, elencate nel materiale del corso e che vale la pena commentare una a una.

La **trasparenza** è la proprietà fondante: lo stesso `docker run` esegue lo stesso container RT-POSIX su Linux oppure su **Zephyr su un co-processore Cortex-R**, senza cambiare né il comando né l'immagine. La complessità della piattaforma (celle, cell file, toolchain dell'hypervisor) è interamente assorbita dal runtime.

Il supporto **mixed-criticality nativo** deriva dal fatto che il criterio di mappatura è la criticità dichiarata del container: la piattaforma alloca ogni carico al backend con il livello di garanzia appropriato, facendo convivere sullo stesso nodo best-effort e safety-critical.

La **ridondanza con diversità "gratis"** è una conseguenza elegante: poiché lo stesso container può essere istanziato su backend diversi (per esempio una replica su RT-Linux e una su cella Jailhouse), si ottengono repliche *diverse* per costruzione — esecuzione su stack software differenti — che non condividono i modi di guasto di modo comune. La diversità, che nei sistemi fault-tolerant tradizionali costa lo sviluppo di N versioni, qui emerge dal meccanismo di deployment.

La **migrazione seamless tra backend** e il **rolling upgrade diversificato** estendono al mondo mixed-criticality le pratiche operative del cloud: un container può essere spostato da un backend all'altro (ad esempio da cella partizionata sull'edge a container standard sul cloud, accettando garanzie inferiori), e gli aggiornamenti possono essere fatti avanzare per gradi attraverso backend diversi.

Infine, il perimetro concreto del supporto attuale: **container RT-Linux, container sandboxed su Jailhouse e Xen, container bare-metal su RPU**.

---

## 6. L'architettura software di runPHI: diagramma a blocchi commentato

Passiamo ora dentro la scatola, sulla base della documentazione del repository (`runphi/runphi_manager`). RunPHI è scritto in Rust e organizzato in *crate* (i moduli/pacchetti di Rust) con una separazione netta tra una **parte frontend indipendente dall'hypervisor** e una **parte backend dipendente dall'hypervisor**. Un principio va fissato subito: **un binario, un backend** — il backend (Jailhouse oppure Xen) viene scelto a *compile time* tramite una feature di Cargo (`jailhouse` è il default, `xen` l'alternativa), non esiste switch a runtime, e `runphi --version` dichiara quale backend è compilato dentro (es. `runphi 0.5.7 (backend: jailhouse)`).

### 6.1 Il diagramma a blocchi

```
┌───────────────────────────────────────────────────────────────────────┐
│                LIVELLO DI ORCHESTRAZIONE (invariato)                  │
│     kubelet ──► containerd ◄── dockerd          kubelet ──► CRI-O     │
└──────────────────────┬────────────────────────────────┬───────────────┘
                       │  exec "runc <cmd> <id>"        │
┌──────────────────────▼────────────────────────────────▼───────────────┐
│              BINARIO runphi (un backend compilato dentro)             │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │  ① crate runphi  (indipendente dall'hypervisor)                 │  │
│  │  ┌──────────────────────┐   ┌────────────────────────────────┐  │  │
│  │  │ main: dispatch OCI   │──►│ forwarding: decisione          │  │  │
│  │  │ (create/start/kill/  │   │ runPHI vs runc_vanilla         │  │  │
│  │  │  delete/state)       │   │ (annotazione / /boot/config.   │  │  │
│  │  └──┬────────┬──────┬───┘   │  json / /run/runPHI/<id>/)     │  │  │
│  │     │        │      │       └───────────────┬────────────────┘  │  │
│  └─────┼────────┼──────┼───────────────────────┼───────────────────┘  │
│        │        │      │                       │ container standard   │
│  ┌─────▼─────┐  │  ┌───▼──────────────┐        │ (pause, immagini     │
│  │② liboci_  │  │  │③ logging         │        │  docker qualsiasi)   │
│  │  cli      │  │  │ log di sistema + │        │                      │
│  │ parsing   │  │  │ timer TickSource │        │                      │
│  │ argomenti │  │  └───▲──────────────┘        │                      │
│  │ CLI OCI   │  │      │ install() all'avvio   │                      │
│  └───────────┘  │      │                       │                      │
│  ┌──────────────▼──────┴────────────────────┐  │                      │
│  │ ⑤ backend_jailhouse OPPURE backend_xen   │  │                      │
│  │   (selezione a compile time)             │  │                      │
│  │  ┌─────────────────────────────────────┐ │  │                      │
│  │  │ API: createguest / startguest /     │ │  │                      │
│  │  │ stopguest / destroyguest / cleanup /│ │  │                      │
│  │  │ storeinfo                           │ │  │                      │
│  │  └───────┬─────────────────────────────┘ │  │                      │
│  │  ┌───────▼─────────────────────────────┐ │  │                      │
│  │  │ configGenerator                     │ │  │                      │
│  │  │  ├ helper start / helper end        │ │  │                      │
│  │  │  ├ resource manager: CPU, memoria,  │ │  │                      │
│  │  │  │  RPU, device, comunicazione,     │ │  │                      │
│  │  │  │  rete, parametri di boot         │ │  │                      │
│  │  │  └ template manager (template della │ │  │                      │
│  │  │     piattaforma hardware)           │ │  │                      │
│  │  └─────────────────────────────────────┘ │  │                      │
│  │  TickSource: /dev/mem MMIO (Jailhouse)   │  │                      │
│  │              /dev/arm_timer (Xen)        │  │                      │
│  └───────┬──────────────────▲───────────────┘  │                      │
│          │                  │                  │                      │
│  ┌───────▼──────────────────┴───────────────┐  │                      │
│  │ ④ frontend_to_backend                    │  │                      │
│  │  FrontendConfig, ImageConfig: strutture  │  │                      │
│  │  dati condivise = API frontend/backend;  │  │                      │
│  │  parsing di <rootfs>/boot/config.json    │  │                      │
│  └──────────────────────────────────────────┘  │                      │
└──────────┬─────────────────────────────────────┼──────────────────────┘
           │ jailhouse cell create/load/start    │ exec
           │ xl create -p / unpause / destroy    ▼
┌──────────▼──────────────────┐   ┌──────────────────────────────┐
│ cella Jailhouse │ domU Xen  │   │ /usr/local/sbin/runc_vanilla │
│ (ZIC: la partizione guest)  │   │ (il runc originale salvato)  │
└─────────────────────────────┘   └──────────────────────────────┘
```

### 6.2 Commento blocco per blocco

**① Il crate `runphi` — il cuore hypervisor-independent.** Contiene il `main()` del binario: riceve l'invocazione da containerd (o CRI-O), fa il *dispatch* del comando OCI richiesto e coordina tutti gli altri blocchi. È qui che vive la logica più caratteristica di runPHI, la **decisione di forwarding**: prima di prendere in carico un container, runPHI decide se quel container è "suo" (uno ZIC da partizionare) oppure un container ordinario da inoltrare al runc originale (`runc_vanilla`). Le regole di decisione, in ordine di precedenza, sono tre. Primo, l'**annotazione OCI esplicita** `org.runphi.runtime`: il valore `"runphi"` forza la gestione locale, `"runc"` forza l'inoltro — è la manopola per chi vuole essere esplicito, per esempio tramite l'allowlist delle pod-annotation di containerd. Secondo, l'**auto-rilevamento al `create`**: il container è trattato come runPHI-managed se e solo se il suo rootfs contiene **`/boot/config.json`** (i parametri di boot della cella partizionata); tutto il resto — incluso il *pause container* di Kubernetes e le normali immagini docker — viene inoltrato a runc. Terzo, per i **comandi successivi** (start/kill/delete/state) fa fede l'esistenza della directory di stato **`/run/runPHI/<id>/`**, creata da runPHI al `create`: se esiste, il comando è gestito localmente, altrimenti inoltrato. Questo meccanismo è ciò che rende runPHI un *drop-in replacement* totale: un nodo può eseguire contemporaneamente container ordinari (che passano da runc come se nulla fosse) e ZIC partizionati, con lo stesso binario in `/usr/bin/runc`.

**② Il crate `liboci_cli` — il parsing della CLI OCI.** Traduce gli argomenti da riga di comando con cui containerd invoca il runtime (un sottoinsieme della OCI runtime-spec) in strutture dati Rust. È l'equivalente funzionale del layer CLI di runc; separarlo in un crate mantiene il `main` pulito e riusabile.

**③ Il crate `logging` — log e misurazione del tempo.** Usato da tutti gli altri crate per un logging sistematico (di default su `/usr/share/runPHI/log.txt`, con livello controllato dalla variabile d'ambiente `RUNPHI_DEBUG_LEVEL`: error/warn/info/debug/trace, che deve essere ereditata da containerd). Contiene inoltre un modulo `timer` basato sul trait **`TickSource`**: l'astrazione è indipendente dall'hypervisor (un solo metodo, `read_ticks() -> u64`), ma la *lettura fisica* dei tick è per forza platform-specific, quindi l'implementazione vive nel backend attivo — MMIO su `/dev/mem` sotto Jailhouse, il char device `/dev/arm_timer` sotto Xen — e viene installata una volta all'avvio dal `main`. È un esempio in miniatura del pattern architetturale dell'intero progetto: interfaccia comune nel frontend, implementazione nel backend.

**④ Il crate `frontend_to_backend` — il contratto tra i due mondi.** Definisce le strutture dati condivise (`FrontendConfig`, `ImageConfig`) che fungono da **API tra la parte hypervisor-independent e quella hypervisor-dependent**: frontend e backend concordano su questo formato e lo usano entrambi. È questo crate che effettua il parsing di `<rootfs>/boot/config.json` — il file, contenuto *nell'immagine del container*, che descrive come deve nascere la partizione (quale kernel o binario caricare, la variante di OS, i dispositivi, il layout di memoria). Vale la pena mostrare la forma di questo file, perché è il "config.json della cella" che affianca il config.json del bundle OCI:

```json
{ "os_var": "zephyr", "inmate": "/boot/hello.bin", "net": "no" }
```

per un inmate bare-metal (un binario Zephyr o qualunque binario singolo), oppure

```json
{ "os_var": "linux", "inmate": "/boot/Image",
  "ramdisk": "/boot/rootfs.cpio.gz", "net": "no" }
```

per un guest Linux completo (kernel + initramfs, con root in RAM). Per i guest Linux su Xen esistono anche strategie di root-disk alternative, selezionate dal campo `disk_type`: `"file"` (un'immagine ext4 grezza spedita dentro il container e usata sul posto, con le scritture che finiscono nel layer scrivibile del container) e `"lvm"` (un volume logico dell'host creato al `create` e popolato con un *clone del rootfs del container* — l'immagine docker diventa letteralmente il disco root della VM — e distrutto con `lvremove` alla delete).

**⑤ I crate `backend_jailhouse` / `backend_xen` — la parte hypervisor-dependent.** Ogni backend supportato è un crate `backend_<nome>`; i due esistenti convivono nello stesso albero dei sorgenti ma **esattamente uno** finisce nel binario, scelto dalla feature di Cargo. Il crate attivo è ri-esportato alla radice come `backend`, così il resto del codice chiama `backend::createguest(...)` senza sapere quale sia. Ogni backend espone la stessa superficie di **sette elementi**: le sei funzioni del ciclo di vita del guest — `createguest`, `startguest`, `stopguest`, `destroyguest`, `cleanup`, `storeinfo` — più il modulo **`configGenerator`**. Aggiungere il supporto a un nuovo hypervisor significa scrivere un nuovo crate `backend_<nome>` che produca la stessa superficie: l'estensibilità è progettata dentro l'architettura.

Il **configGenerator** merita un ingrandimento, perché è il pezzo che automatizza il lavoro che nella sezione 4.3 avevamo indicato come il costo dei container partizionati: la scrittura della configurazione della cella. Il generatore orchestra tre tipi di componenti. Gli **helper** (`config_generator_helper_start` e `_end`) aprono e chiudono la generazione. I **resource file** sono i gestori delle risorse della piattaforma hardware, invocati per comporre il file di configurazione: per Jailhouse ne esiste uno per ciascuna risorsa — **CPU, memoria, RPU, dispositivi, comunicazione, rete, altri parametri di boot** — e sono loro a decidere, sulla base delle risorse disponibili, cosa assegnare alla nuova cella. Il **template manager** carica il *template della piattaforma hardware*: la configurazione parte infatti da un modello fornito per la specifica board, che i resource manager specializzano. L'output è il file di configurazione hypervisor-specifico — il *cell file* `.c` di Jailhouse o il config del domU di Xen — salvato in `/run/runPHI/<id>/config<id>.conf`.

### 6.3 Lo stato su disco

RunPHI, come runc, è un processo effimero: lo stato persistente vive su filesystem. La mappa completa:

| Percorso | Creato da | Contenuto |
|---|---|---|
| `/usr/share/runPHI/log.txt` | `logging` | Log di runPHI (e marker di fase del timer) |
| `/run/runPHI/<id>/` | `main` al `create` | Directory di lavoro per container; la sua esistenza è la *source of truth* di "questo è un container runPHI" |
| `/run/runPHI/<id>/bundle` | `storeinfo` | Percorso assoluto del bundle OCI |
| `/run/runPHI/<id>/pidfile` | `storeinfo` | Percorso del pidfile del processo init |
| `/run/runPHI/<id>/rootfs` | `storeinfo` | Percorso assoluto del rootfs montato |
| `/run/runPHI/<id>/config<id>.conf` | `configGenerator` | Config hypervisor-specifica (cell file Jailhouse / config domU Xen) |
| `<rootfs>/boot/config.json` | l'immagine del container (input) | Parametri di boot della cella: kernel/inmate, os_var, device, memoria |
| `/usr/local/sbin/runc_vanilla` | `switch_to_runphi.sh` | Il runc originale della distribuzione, invocato nei forwarding |

Completa il quadro il componente **CNI** del repository: un plugin di Container Network Interface, ancora sperimentale, che integra il networking di Kubernetes con quello dell'hypervisor (assumendo due VM che comunicano su interfacce di rete) — il tassello necessario perché gli ZIC partecipino alle reti dei pod.

---

## 7. Mappatura runPHI ↔ runc: il ciclo di vita OCI a confronto

Questa sezione chiude il cerchio con il punto 1: mostriamo come ogni fase del ciclo di vita OCI, che nella sezione 2 abbiamo descritto per runc, viene reinterpretata da runPHI. La premessa è che, dal punto di vista di containerd, **non c'è alcuna differenza**: containerd continua a eseguire `/usr/bin/runc <comando>` per ogni evento del ciclo di vita; solo che quel binario, dopo `switch_to_runphi.sh`, è runPHI.

### 7.1 Il flusso per ciascun comando OCI

Il dispatch in `runphi/src/main.rs` realizza, comando per comando, questo schema:

```
create → legge il config.json del bundle → forwarding::decide_create
         ├─► (inoltro a runc_vanilla ed exit)            [container standard]
         └─► mkdir /run/runPHI/<id>/                     [ZIC]
             → frontend::commands::create
             → backend::createguest → backend::storeinfo

start  → forwarding::decide_existing (esiste /run/runPHI/<id>/ ?)
         ├─► inoltro   └─► frontend::commands::start → backend::startguest

kill   → stesso check → backend::stopguest + backend::destroyguest
delete → stesso check → stopguest + destroyguest + backend::cleanup
state  → stesso check → lettura di bundle/pidfile/rootfs da /run/runPHI/<id>/
```

`createguest` è il passo pesante. Su **Jailhouse** invoca `jailhouse cell create` passando il cell file prodotto dal configGenerator, poi `jailhouse cell load` per caricare l'inmate (il binario o kernel guest) nella cella. Su **Xen** costruisce la configurazione del domU e chiama `xl create -p`, cioè crea il dominio *in pausa*; sarà poi `startguest` a eseguire `xl unpause` (mentre su Jailhouse esegue il cell start). Questa scelta non è un dettaglio: riproduce fedelmente la **semantica a due fasi create/start** della runtime-spec OCI, in cui il container viene prima predisposto e congelato, e solo dopo — quando l'orchestratore lo decide — messo in esecuzione.

### 7.2 La tabella di corrispondenza

| Fase / concetto | runc (container classico) | runPHI (ZIC / container partizionato) |
|---|---|---|
| Chi lo invoca | containerd/CRI-O eseguono il binario per ogni evento | Identico: stesso exec, il binario è stato sostituito |
| Parsing CLI OCI | layer CLI di runc | crate `liboci_cli` |
| Input principale | `config.json` **del bundle** (namespaces, cgroups, mounts, processo) | `config.json` del bundle per l'interfaccia OCI **+ `/boot/config.json` nel rootfs** per i parametri della cella |
| Rilevamento del tipo | — (gestisce tutto) | forwarding: annotazione `org.runphi.runtime` → presenza di `/boot/config.json` → presenza di `/run/runPHI/<id>/` |
| `create` | `clone()/unshare()` dei namespace, setup cgroups, `pivot_root`, init congelato | configGenerator produce il cell file / config domU; `jailhouse cell create` + `cell load`, oppure `xl create -p` (dominio in pausa) |
| Meccanismo di isolamento | Namespaces + cgroups **sul kernel condiviso** | Cella / domU con pCPU, memoria (colorata) e device **dedicati, sotto hypervisor** |
| `start` | Sblocca l'init → `execve` dell'entrypoint | `jailhouse cell start` oppure `xl unpause` |
| `kill` | Invia il segnale al processo init | `stopguest` + `destroyguest` (arresto e distruzione della cella) |
| `delete` | Rimuove stato e cgroups | `stopguest` + `destroyguest` + `cleanup` (incluso `lvremove` per i root-disk LVM) |
| `state` | Legge lo stato da `/run/runc/` o `/run/containerd/` | Legge bundle/pidfile/rootfs da `/run/runPHI/<id>/` |
| Directory di stato | `/run/runc/<id>/`, `/run/containerd/...` | `/run/runPHI/<id>/` |
| Natura del processo runtime | Effimero: crea e termina | Identico: effimero, lo stato vive su disco |
| Fallback | — | I non-ZIC (pause container, immagini docker qualsiasi) sono inoltrati a `/usr/local/sbin/runc_vanilla` |
| Estensibilità | Monolitico | Nuovo crate `backend_<nome>` con la stessa superficie a 7 elementi |

### 7.3 Lettura della mappatura

Tre osservazioni riassumono il senso della tabella. La prima: **runPHI conserva integralmente il contratto OCI verso l'alto** — stessi comandi, stessa semantica a due fasi, stesso modello di stato su disco, stesso carattere effimero del processo runtime. Ciò che cambia è l'*implementazione* di ciascun verbo: dove runc parla al kernel con syscall (clone, cgroups, pivot_root), runPHI parla all'hypervisor con i suoi strumenti di gestione (jailhouse cell *, xl *). In termini architetturali, i verbi OCI vengono tradotti da operazioni sul kernel condiviso a *partition ops* della partizione privilegiata — esattamente le operazioni (create, load, start, shutdown, destroy, stats) che la slide del backend Jailhouse colloca nella partizione privilegiata.

La seconda: il **doppio config.json** è il punto in cui il formato immagine OCI viene esteso senza romperlo. Il `config.json` del bundle resta il contratto con containerd; il `/boot/config.json` dentro il rootfs è il contratto tra l'immagine e il backend, e viaggia dentro l'immagine stessa — il che significa che la "ricetta di partizionamento" è versionata, distribuita e cachata dal registry come qualunque altro contenuto del container. La sua semplice presenza, inoltre, fa da marcatore automatico di "questo è uno ZIC".

La terza: il **forwarding a runc_vanilla** è ciò che rende il tutto praticabile in un cluster reale. Kubernetes crea per ogni pod un pause container e dà per scontato che immagini arbitrarie girino; se runPHI pretendesse di partizionare tutto, nulla funzionerebbe. Invece il nodo resta un nodo Kubernetes perfettamente ordinario che, *in più*, sa istanziare celle a interferenza zero quando l'immagine lo richiede.

---

## 8. Conclusioni

Il percorso compiuto può essere riassunto in una frase: **RunPHI dimostra che il punto di sostituibilità creato dallo standard OCI — il livello di runc — è sufficiente a estendere il modello container dall'IT general-purpose fino ai sistemi mixed-criticality**, senza toccare una riga degli strati superiori dello stack.

Il container tradizionale (sezione 2) offre un modello operativo insuperabile ma un isolamento fondato sul kernel condiviso, strutturalmente inadatto alle garanzie temporali e alla certificazione. Le risposte del mondo real-time (sezione 3) — PREEMPT_RT con SCHED_DEADLINE, i co-kernel, le sandbox, il bare-metal — coprono l'intero spettro dell'isolamento ma sono tra loro frammentate e incompatibili. Il container partizionato (sezione 4) fornisce il mattone tecnico per i livelli di garanzia più alti: il carico OCI istanziato come cella di un hypervisor di partizionamento, con CPU mappate 1:1, memoria colorata e I/O dedicato — lo Zero-Interference Container. RunPHI (sezioni 5–7) fornisce il collante: un runtime OCI in Rust, con un frontend hypervisor-independent (dispatch, parsing CLI, strutture dati condivise, logging con TickSource) e backend intercambiabili a compile time (Jailhouse, Xen), che traduce i verbi OCI in operazioni di partizionamento, genera automaticamente le configurazioni delle celle tramite resource manager e template di piattaforma, e inoltra a runc tutto ciò che non è uno ZIC.

Il risultato, visto dall'operatore, è la promessa della slide sui benefici: lo stesso `docker run` che esegue lo stesso container su Linux o su Zephyr sopra un Cortex-R; mixed-criticality nativo; ridondanza con diversità gratuita; migrazione e rolling upgrade tra backend con livelli di garanzia diversi. Visto dall'architetto software, è una lezione di design: interfacce stabili (OCI in alto, la superficie a sette elementi dei backend in basso), strutture dati condivise come contratto (frontend_to_backend), e la complessità hypervisor-specific confinata dove non può contaminare il resto.

Restano, naturalmente, questioni aperte tipiche di un progetto di ricerca: la staticità dell'allocazione delle risorse (l'admission control e il packing ottimo dei container partizionati su risorse finite), il networking degli ZIC (il plugin CNI è dichiaratamente sperimentale), l'ampliamento dei backend, e l'integrazione piena con l'ecosistema di orchestrazione (scheduling Kubernetes consapevole della criticità). Ma la direzione è tracciata: il continuum cloud–edge–dispositivo gestito con un unico modello operativo, in cui la criticità di un carico è un attributo di deployment come oggi lo sono CPU e memoria.

---

## 9. Riferimenti

Materiale del corso: lezione "RT Cloud and real-time containers" (slide su proposte per container real-time, soluzione RunPhi, benefici di RunPhi, backend Jailhouse), Università degli Studi di Napoli Federico II.

M. Barletta, M. Cinque, R. Della Corte, G. Farina, L. De Simone, D. Ottaviano, "Partitioned Containers: Towards Safe Clouds for Industrial Applications", DSN 2023 — Disrupt Track.

D. Ottaviano, M. Barletta, F. Boccola, "Zero-Interference Containers: A Framework to Orchestrate Mixed-Criticality Applications", DSN 2025.

Repository del progetto: https://github.com/runphi (organizzazione) e https://github.com/runphi/runphi_manager (runtime; si vedano README.md e doc/README.md per architettura, ciclo di vita OCI e stato su disco).

Standard di riferimento: OCI Runtime Specification e OCI Image Specification (Open Container Initiative); documentazione di runc, containerd, Jailhouse e Xen.
