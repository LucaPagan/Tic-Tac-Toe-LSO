# TRIS — Multiplayer Session Manager (Progetto LSO)

Questo progetto implementa il gioco del Tris (Tic-Tac-Toe) attraverso un'architettura **Client-Server** utilizzando **Socket TCP**. Sviluppato per l'esame di Laboratorio di Sistemi Operativi (LSO).

- **Server**: Implementato in C, gestisce il multithreading per permettere partite concorrenti tra vari giocatori, mantenendo uno stato sincronizzato.
- **Client**: Implementato in Java con JavaFX, con un'interfaccia grafica moderna (Dark Theme) che permette di creare nuove partite o unirsi a partite esistenti.

---

## 🚀 Esecuzione Rapida Autogestita (Raccomandata)

Il metodo più semplice e rapido per eseguire il progetto sia per te che per il docente è utilizzare interamente **Docker Compose**. Questa modalità avvierà sia il Server che il numero desiderato di Client in container isolati.

### Prerequisiti
- Docker e Docker Compose installati
- Server X11 avviato sul tuo sistema host (es. **XQuartz** su Mac o Xming/VcXsrv su Windows) e configurato per accettare connessioni dai container.

> [!IMPORTANT]
> **Configurazione DISPLAY (.env)**  
> Nel file `.env` è presente la configurazione per inoltrare la GUI dei client Docker al tuo host. Modifica la variabile se necessario. Su macOS solitamente `host.docker.internal:0` è sufficiente assicurandoti che XQuartz abbia la spunta su *'Allow connections from network clients'*.

### Come avviare una partita a 2 giocatori
Per avviare **1 Server** e **2 Client** simultaneamente, apri il terminale nella root del progetto e lancia:

```bash
docker-compose up --build --scale client=2
```
*(Per eseguire in background: aggiungere `-d` alla fine. Per chiudere: `docker-compose down`)*

### Vuoi più giocatori?
Ti basterà aumentare lo "scale" del client. Ad esempio, per 4 giocatori (2 partite separate in simultanea):
```bash
docker-compose up --build --scale client=4
```

---

## 🛠️ Esecuzione Ibrida (Server Docker + Client Locale)

Se preferisci far girare il Client Java nativamente sul tuo PC (ad esempio se hai problemi di rendering grafico con Docker) e solo il Server su Docker.

### 1. Avvio del Server
Nella root del progetto:
```bash
docker-compose up --build server -d
```
Il server sarà in ascolto sulla porta `5200`.

### 2. Compilazione e Avvio del/dei Client (nativamente)
Assicurati di avere `Maven` e almeno `Java 22` installati. Entra nella cartella `Client`:
```bash
cd Client
```

Dopodiché dovrai scaricare le librerie JavaFX e compilarle (esempio per Mac/Linux x64, adatta l'URL al tuo sistema operativo e architettura):

```bash
# Esempio per Linux/Mac x64:
curl -L -o javafx.zip https://download2.gluonhq.com/openjfx/24.0.2/openjfx-24.0.2_linux-x64_bin-sdk.zip
unzip javafx.zip
mv javafx-sdk-24.0.2/lib ./lib
rm -rf javafx.zip
```

**Compilazione del progetto:**
```bash
mvn clean package
```

**Esecuzione di un'istanza Client:**
```bash
java --module-path ./lib --add-modules javafx.controls,javafx.fxml -jar ./target/client-1.0-SNAPSHOT-shaded.jar
```

Per simulare più giocatori, ti basterà aprire **nuove finestre del terminale** e rieseguire il comando di avvio per generare un nuovo client.

---

## 🧩 Struttura Logica Soddisfatta

Il progetto aderisce integralmente alla traccia dell'esame:
- [x] R1: Server C, Client Java
- [x] R2-R3: Connessione Socket TCP e gestione Server con Thread Posix concorrenti
- [x] R4: Partite esclusive a 2 giocatori.
- [x] R5-R6: Un utente può instanziare stanze, altri possono unirsi ed il creatore accetta/rifiuta ("FORK NUOVO BLOCCO" / "AGGANCIO" / Popup di consenso).
- [x] R7: Nessun giocatore può far parte di più partite contemporaneamente.
- [x] R8-R9: Identificatore univoco di sessione, e corretta transizione di stato tra: Nuova (`FRESH_INST`), In attesa (`PENDING_PLYR`), In Corso (`IN_PROGRESS`), Terminata (`CONCLUDED`).
- [x] R10: Broadcast delle partite create e terminate a tutti i client a riposo nella lobby (Ledger aggiornato periodicamente tramite polling).
- [x] R11: Piena operatività della funzionalità "Rivincita" post-partita per vincite, sconfitte e pareggi.
- [x] R12: Orchestrazione su docker.