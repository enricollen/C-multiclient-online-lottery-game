#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

//VAR GLOBALI
uint16_t lmsg; //per ricevere la lunghezza del buffer
int connesso; //variabile passata come condizione al while del figlio 
pid_t pid; //processo figlio
int ret, socket_server, nuovo_socket, len;
int porta;
int periodo = 5*60;
int tentativiLogin = 3; //variabile conteggio per i tentativi login falliti
char buffer[4096]; //buffer per scambio dati send/rcv con client
char username_utente_loggato[1024]; //variabile in cui viene salvato l'username dell'utente attualmente loggato
// Strutture dati per indirizzi dei Socket server e socket client
struct sockaddr_in server_addr, client_addr;
char sessionID[11]="0x00000000";
//var ausiliarie per il comando !invia_giocata (utente deve specificare tutti e 3 i separatori -r -n -i)
int r,n,i=0;

struct schedina{
        char utente[1024];
        int ruote[11]; //10 più "tutte"
        int numeri[10];
        float importi[5];
};

//-------------------------------------------------------------------------------------------------------

void inviaMessaggio(char* buffer){ //funzione di invio al client del messaggio contenuto in buffer
        int ret;
		int len = strlen(buffer) + 1; // Voglio inviare anche il carattere di fine stringa
        lmsg = htons(len); //endianess
		ret = send(nuovo_socket, (void*) &lmsg, sizeof(uint16_t), 0);
        ret = send(nuovo_socket, (void*) buffer, len, 0);
        if(ret < 0){ //gestione errori
                perror("SERVER: Errore in fase di invio messaggio!\n");
        	exit(-1);
    	}
}

void riceviMessaggio(char* buffer){ //funzione di ricezione dal client di un messaggio che verrà salvato in buffer
	int len;
	int ret;
	ret = recv(nuovo_socket, (void*)&lmsg, sizeof(uint16_t), 0); //ricevo prima la lunghezza del messaggio
 	len = ntohs(lmsg); // Rinconverto in formato host
	ret = recv(nuovo_socket, (void*)buffer, len, 0); //poi ricevo il messaggio vero e proprio
	if(ret < 0){ //gestione errori
            perror("SERVER: Errore in fase di ricezione messaggio!\n");
    	exit(-1);
    }
    if(ret == 0){
	        printf("SERVER: Client disconnesso, chiusura socket.\n");
	        exit(-1);
    }
}


//funzione che legge dalla lista degli utenti registrati e controlla se l'username inserito
//nel comando !signup è già stato utilizzato o meno
int controllo_validita_username(char* username){
        FILE* fh;
		fh=fopen("utenti_registrati.txt","r");

        //controllo se il file esiste
        if (fh == NULL){
            printf("ERRORE: non è stato possibile aprire il file! \n");
            return 0;
        }

        //leggo riga per riga il file utenti_registrati.txt e controllo se esiste gia un username uguale
        const size_t line_size = 1024;
        char* line = malloc(line_size);
        while (fgets(line, line_size, fh) != NULL)  {
                char* tmp;
                tmp = strtok(line, "/");
                if(strcmp(tmp, username)==0)
                        return 0;
        }
        
        if(line)
                free(line);
        fclose(fh);
        
        return 1;
}

//funzione utilizzata dalla comando_signup per creare i file relativi alle giocate 
//effettuate dagli utenti, presenti nel path Schedine Utenti/
int creazione_scheda_utente(char* username){
        
        char* path = "./Schedine Utenti/";
        int lunghezza_path_cartella=strlen(path);
        int lunghezza_username=strlen(username);
        char file_path[lunghezza_path_cartella + lunghezza_username + 5]; // "./Schedine Utenti/" + "user" + ".txt" + "/0"
        sprintf(file_path, "%s%s.txt", path, username);
        
        FILE* f;
        if ((f=fopen(file_path, "w"))==NULL)
            	printf("ERRORE: non è stato possibile aprire il file! \n");

	fclose(f);
}

//funzione che si occupa di effettuare la registrazione di un nuovo utente.
//fa uso delle funzioni ausiliare controllo_vaidita_username per verificare l unicita
// e creazione_scheda_utente se il controllo viene superato
int comando_signup(char* username, char* password){
        
        int ret=1;
        
        FILE* f;
        //se non esiste lo crea, in modo che la controllo_validita_utente non fallisca nell'aprire il file
	f=fopen("utenti_registrati.txt","a"); 
        
        //controllo utente con stesso username
        if(!controllo_validita_username(username)){
            	printf("SERVER: Username già presente, notifico il client! \n");
                inviaMessaggio("ERRORE: username scelto già in uso, riprovare.\n");
                return -1;
        }
        
        //se arrivo qui ho passato il test di univocità
        inviaMessaggio("SERVER: Registrazione avvenuta con successo!\n");

	if (!f) { 
            	printf("ERRORE: non è stato possibile aprire il file utenti_registrati.txt! \n");
            	return -1;
  	}
        
        //scrivo in utenti_registrati.txt le credenziali del nuovo utente
        fprintf(f, "%s/%s", username, password);
        
        //e creo il file "nomeutente.txt" dentro la cartella Schedine Utenti
        //destinato a contenere le giocate e le vincite realizzate dall'utente
        creazione_scheda_utente(username);
        
	fclose(f);
	return ret; //ret==1 registrazione andata a buon fine
}

//funzione utilizzata nella comando_login, va a generare un sessionID come sequenza
//di caratteri alfanumerici. Modifica direttamente la variabile globale sessionID
void genera_sessionID(const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    srand(time(NULL));
    int i = 0;
    for (; i < len; ++i) {
        sessionID[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    sessionID[len] = 0;
}


//funzione che, dati un username e password, controlla dal file utenti_registrati.txt
//che esista una corrispondenza. Ritorna 0 se il controllo va a buon fine, 1 altrimenti
int check_credenziali_login(char* username, char* password){
        
    if(!password) 
            return 1;
                
    //Variabili ausiliarie per lettura singole righe da file
	char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;
  	
  	//Metto insieme username e password in un unica stringa
  	//nello stesso formato di come le leggerò da utenti_registrati.txt (<username>/<password>)
	int usrlen = strlen(username);
	int pswlen = strlen(password); 
	char credenziali[usrlen + pswlen + 3];
	sprintf(credenziali, "%s/%s", username, password); //formato username/password
  	
    FILE* f;
	f=fopen("utenti_registrati.txt","r");

        //controllo se il file esiste
        if (f == NULL){
            printf("ERRORE: non è stato possibile aprire il file utenti_registrati.txt!\n");
            return 1;
        }
        
        //leggo riga per riga il file utenti_registrati.txt e controllo prima se esiste un username uguale a quello inserito
        line_size = getline(&line_buf, &line_buf_size, f);

  	while (line_size >= 0) { // scorro tutte le linee del file utenti_registrati.txt
            	if(strcmp(line_buf,credenziali) == 0) { //se la riga letta dal file è uguale alla stringa <user>/<password>
            		printf("SERVER: Username e password inseriti sono corretti \n");
            		return 0; //credenziali ok
            	}
    	        line_size = getline(&line_buf, &line_buf_size, f); //avanzo alla prossima riga interna al file
  	}
  	
  	return 1; //match non avvenuto, utente/password errati
}


//funzione invocata per gestire il comando !login <username> <password>
//richiama la funzione di utilità check_credenziali_login, e se il controllo
//va a buon fine, altrimenti gestisco il numero di tentativi errati e se questi
//risultano essere 3 inserisco l'IP nel file black_list.txt per bloccare la connessione
int comando_login(char* username, char* password){

        int autenticazione = check_credenziali_login(username, password);
        
        //se l'autenticazione va a buon fine
        if(autenticazione==0){
                tentativiLogin = 3; //resetto nr tentativi
                genera_sessionID(10);
                
                //inserisco l'username dell'utente appena loggato nella var globale username_utente_loggato
                strcpy(username_utente_loggato,username); 
                
                //mando al client il messaggio di successo e il sessionID generato
                inviaMessaggio("SERVER: Login effettuato correttamente\n");
                inviaMessaggio(sessionID);
                printf("SERVER: session ID assegnato '%s' \n", sessionID);
    	        printf("SERVER: SessionID inviato al client con successo \n");
        }
        
        //se l'autenticazione fallisce
        else{
                
                if(tentativiLogin > 1){
                        char msg[100];
                        sprintf(msg, "SERVER: Username e/o password errati.\nTentativi rimanenti: %i\n", tentativiLogin-1);
                        inviaMessaggio(msg); //invio un messaggio di errore al client
                }
    		else {
    			inviaMessaggio("SERVER: Superato limite di tentativi login falliti, disconnessione...\n");
    			printf("SERVER: Limite tentativi login superato, disconnetto client. \n");
    		}
            tentativiLogin = tentativiLogin - 1; //decremento i tentativi
            	
            	if(tentativiLogin == 0){
            	        //stringa per la stampa su file
                        char stringaFinale[100]; 

				    		//variabili per il timestamp
				  		char timestamp[50]; //stringa dove verrà salvato il timestamp nel formato "dd-mm-yyyy hh-mm-ss"
				    		time_t t = time(NULL);
				  		struct tm ts = *localtime(&t); //struct per il timestamp

						sprintf(timestamp,"%02d-%02d-%d %02d:%02d:%02d",ts.tm_mday, ts.tm_mon + 1,ts.tm_year + 1900, ts.tm_hour, ts.tm_min, ts.tm_sec);

						//inet_ntoa restituisce l'indirizzo ip come stringa da client_addr
						//formato timestamp sec/IPClient/date time
						sprintf(stringaFinale, "%d/%s/%s", (int)time(NULL), inet_ntoa(client_addr.sin_addr), timestamp); 
						
						//time() restituisce il timestamp in secondi dal 1 gennaio 1970, servirà per i confronti tra timestamp
						FILE *fd = fopen("black_list.txt", "a");
						if (fd == NULL){
				                printf("ERRORE: non è stato possibile aprire il file!\n");
				                return -1;
				        }
						fprintf(fd, "%s\n", stringaFinale); //inserisco l'ip del client e il timestamp nel file black_list.txt
						fclose(fd);
						connesso = 0;
            	}
            	
                return -1;
        }
}

//funzione che gestisce il comando !esci, controlla se il sessionID ricevuto
//corrisponde a quello salvato e ritorna un messaggio di avvenuta disconnessione
void comando_esci(char* sessionIDricevuto){
			if(strcmp(sessionID, sessionIDricevuto)==0){
                //disconnetto il client
                //e resetto il sessionID al valore di default
                strcpy(sessionID,"0x00000000"); 
                printf("SERVER: disconnessione client %s... \n", sessionIDricevuto);
                inviaMessaggio("SERVER: disconnessione avvenuta con successo.\n");
            }
}

//funzione che controlla se le ruote specificate nella giocata rientrino tra quelle disponibili
int check_correttezza_ruote(char ruote[10][1024], int quante_ruote_specificate){

        int ruota_non_riconosciuta=0;
        int j=0;
        for(; j < quante_ruote_specificate; j++) {
	        if(strcmp(ruote[j], "bari")==0 ||
	           strcmp(ruote[j], "cagliari")==0 ||
	           strcmp(ruote[j], "firenze")==0 ||
	           strcmp(ruote[j], "genova")==0 ||
	           strcmp(ruote[j], "milano")==0 ||
	           strcmp(ruote[j], "napoli")==0 ||
	           strcmp(ruote[j], "palermo")==0 ||
	           strcmp(ruote[j], "roma")==0 ||
	           strcmp(ruote[j], "torino")==0 ||
	           strcmp(ruote[j], "venezia")==0 ||
	           strcmp(ruote[j], "nazionale")==0 ||
	           strcmp(ruote[j], "tutte")==0
	        )
	                continue;
	        else{
	                ruota_non_riconosciuta=1;        
	                break;
	        }
        }
        
        return ruota_non_riconosciuta;
}

//BubbleSort, la utilizzo per ordinare l'array dei numeri giocati, per controllare piu
//facilmente la presenza di eventuali numeri duplicati
void sort(int numeri[10]){
        int n = 10; int i=0; int j=0;
        for (i = 0; i < n; i++){
                for (j = 0; j < n; j++){
			if (numeri[j] > numeri[i]){
				int tmp = numeri[i];       
				numeri[i] = numeri[j];            
				numeri[j] = tmp;            
			}  
		}
	}
}

//funzione che controlla se i numeri specificati nella giocata sono compresi tra [1,90] e se non ci sono doppioni tra loro
//utilizzata nella funzione comando_invia_giocata
int check_correttezza_numeri(char numeri[10][1024], int quanti_numeri_specificati){

        int numero_fuori_range=0;
        int numero_duplicato=0;
        int numero;
        int i=0;
        
        int numeri_giocati[10];
        
        for(i=0; i < 10; i++) {
                if(strcmp(numeri[i], "")!=0){
                        numero=atoi(numeri[i]);
                        if(numero==0){ // se l'utente inserisce 0
                                numero_fuori_range=1;
                                break; 
                        }
                }
                else
                        numero=0;
                        
                if(numero >= 0 && numero <= 90)
                        numeri_giocati[i] = numero;
                else{
                        numero_fuori_range=1;
                        break;  
                }                    
        }
        
        //faccio sort su array dei numeri giocati                
        sort(numeri_giocati);
        
        //e confronto ogni numero col successivo per vedere se sono presenti duplicati                        
        for(i=0; i < 10; i++)
                if(numeri_giocati[i] == numeri_giocati[i+1] && numeri_giocati[i]!=0){
                        numero_duplicato=1;
                        break;
                }
                
        return numero_fuori_range || numero_duplicato;
}

//funzione che controlla se gli importi specificati nella giocata sono positivi
//utilizzata nella funzione comando_invia_giocata
int check_correttezza_importi(char importi[5][1024], int quanti_importi_specificati){
        int importo_negativo;
        int i=0;
        for(; i < quanti_importi_specificati; i++) {
                float importo=0.0;
                importo = atof(importi[i]);
	        if(importo >= 0)
	                continue;
	        else{
	                importo_negativo=1;        
	                break;
	        }
        }
        
        return importo_negativo;
}

//Funzione che si occupa di creare una schedina s a partire da 3 array contenenti ruote numeri e importi rispettivamente
//viene invocata alla fine della funzione comando_invia_giocata, dopo aver passato tutti e 4 i controlli sulla sintassi.
//Ritorna la schedina s che verrà poi passata alla funzione registra_giocata che si occupa di salvare la schedina sul file
//schedine_in_attesa_di_estrazione.txt 
struct schedina crea_schedina(char ruote[11][1024], char numeri[10][1024], char importi[5][1024]){
        
        struct schedina schedina;
                
        int i=0;
        
        for(i=0; i < 11; i++){
                schedina.ruote[i]=0;
                if(i<10){
                        schedina.numeri[i]=0;
                }
                if(i<5){
                        schedina.importi[i]=0;
                }
        }
        
        int indice_ruota=0;
        for(i=0;i < 11; i++){
            if(strcmp(ruote[i], "bari")==0) indice_ruota=0;
	        else if(strcmp(ruote[i], "cagliari")==0) indice_ruota=1;
	        else if(strcmp(ruote[i], "firenze")==0) indice_ruota=2;
	        else if(strcmp(ruote[i], "genova")==0) indice_ruota=3;
            else if(strcmp(ruote[i], "milano")==0) indice_ruota=4;
            else if(strcmp(ruote[i], "napoli")==0) indice_ruota=5;
	        else if(strcmp(ruote[i], "palermo")==0) indice_ruota=6;
	        else if(strcmp(ruote[i], "roma")==0) indice_ruota=7;
            else if(strcmp(ruote[i], "torino")==0) indice_ruota=8;
	        else if(strcmp(ruote[i], "venezia")==0) indice_ruota=9;
	        else if(strcmp(ruote[i], "nazionale")==0) indice_ruota=10;
	        else if(strcmp(ruote[i], "tutte")==0) indice_ruota=11;
	        
	        if(indice_ruota<11)
	                schedina.ruote[indice_ruota]=1;
	        
	        if(i<10)
                        schedina.numeri[i] = atoi(numeri[i]);
                if(i<5)
                        schedina.importi[i] = atof(importi[i]);
        }
        
        //se l'utente specifica "tutte", setto tutte le ruote = 1
        if(indice_ruota==11)
        for(i=0;i < 11; i++)
                schedina.ruote[i]=1;
        
        return schedina;
}


//Funzione invocata dalla comando_invia_giocata, dopo aver passato tutti i controlli di sintassi e aver creato la schedina s relativa
//alla giocata. Si occupa di registrare la schedina s sui file giocate_in_attesa_di_estrazione.txt e Schedine Utenti/nomeutente.txt
int registra_giocata(struct schedina *schedina){
        
        char path[40];
        strcpy(path, "./Schedine Utenti/");
        strcat(path, username_utente_loggato);
        strcat(path, ".txt");
        
        FILE *fd = fopen(path, "a");
        if (fd == NULL){
                printf("ERRORE: non è stato possibile aprire il file!\n");
                return -1;
        }
        
        //inserisco la giocata anche nel file che mantiene tutte le giocate che sono ancora "attive"
        //quel file sarà ogni volta ripulito dopo che avviene un estrazione (funzione estrazione) che svuota il contenuto
        FILE *fd2 = fopen("giocate_in_attesa_di_estrazione.txt", "a");
        if (fd2 == NULL){
                printf("ERRORE: non è stato possibile aprire il file giocate_in_attesa_di_estrazione.txt!\n");
                return -1;
        }
        
        char timestamp[1024];
        sprintf(timestamp,"%d",(int)time(NULL));
        int timestampAttuale = atoi(timestamp);
        
        //stampo su file dell'utente il timestamp di creazione e la schedina 
        //in particolare il timestamp mi servirà nella funzione !vedi_giocate 0/1 per confrontarlo
        //con il timestamp dell'ultima estrazione e quindi decidere se stampare a video la giocata perchè ancora valida
        //(vedi_giocate 1) oppure se scaduta (vedi_giocate 0)
        fprintf(fd, "%i ",timestampAttuale);
        //nel file giocate_in_attesa_di_estrazione non serve il timestamp (la funzione estrazione chiama la controlla_vincite 
        //che le scorre tutte), piuttosto mi servirà il nome utente di chi ha effettuato la giocata!
        fprintf(fd2, "%s ",username_utente_loggato);
               
        int j=0;
        for(;j < 11; j++){
                fprintf(fd, "%i ", schedina->ruote[j]);
                fprintf(fd2, "%i ", schedina->ruote[j]);
        }
                
        int h=0;
        for(;h < 10; h++){
                fprintf(fd, "%i ", schedina->numeri[h]);
                fprintf(fd2, "%i ", schedina->numeri[h]);
        }
        
        int k=0;
        for(;k < 5; k++){
                fprintf(fd, "%.2f ", schedina->importi[k]);
                fprintf(fd2, "%.2f ", schedina->importi[k]);
        }
        
        fprintf(fd, "\n");
        fprintf(fd2, "\n");
        
        fclose(fd);
        fclose(fd2);
        
        return 0;
}

//comando che si occupa di inviare una giocata (schedina) effettuata da un utente loggato; effettuo 5 controlli di sintassi
//e poi chiamo le fx crea_schedina e registra_giocata per creare la schedina e salvarla su file rispettivamente
int comando_invia_giocata(char parole[32][1024]){
        int controlli_passati=0;
	/*
	  stima del max num di parole che possono comporre il comando piu lungo (invia_giocata)
	  parole[0] = !comando
	  parole[1] = -r
	  parole[2]->parole[13] = num ruote max
	  parole[14] = -n
	  parole[15]->parole[25] = scelto il max di numeri
	  parole[26] = -i
	  parole[27]->parole[32] = importi max su estratto, ambo, terno, quaterna, cinquina
	*/
	
		//1) controllo se nel comando sono presenti tutti e tre i delimitatori -r -n -i
        int j=0;
        for(; j < 32; j++) {
	        if(strcmp(parole[j], "-r")==0)    r=1;
	        if(strcmp(parole[j], "-n")==0)    n=1;
	        if(strcmp(parole[j], "-i")==0)    i=1;
	        if(r&&n&&i) break;
        }
                
        if(r==1 && n==1 && i==1)
                controlli_passati++;
        else{
                inviaMessaggio("SERVER: Errore, è necessario specificare tutti i separatori -r -n -i nel comando !invia_giocata. \n");
                return -1;
        }
                
        r=0; n=0; i=0;       
                
        //2) controllo se tra -r e -n le ruote specificate sono valide
        char ruote[11][1024];
        
        int i=2;
        int q=0;
        int ret;
        int max_ruote_specificate=11;
        
        //pulisco ruote
        for(q=0; q < 11; q++)
                strcpy(ruote[q], "");
        q=0;        
        while(strcmp(parole[i], "-n") != 0){
                strcpy(ruote[q], parole[i]);
                i++; q++;
        }
        
        //funzione che controlla se le ruote specificate nella giocata rientrino tra quelle disponibili
        ret = check_correttezza_ruote(ruote, q);
        
        if(ret==0 && q <= max_ruote_specificate){
                controlli_passati++;
        }
        else{
                inviaMessaggio("SERVER: Errore nei nomi delle ruote specificate!\n");
                return -1;
        }
        
        //3) controllo che i numeri giocati siano interi tra 1 e 90, che siano al max 10 e che non ci siano doppioni
        //arrivo qui che l'indice i sta su -n
        char numeri[10][1024];
        
        int k=0;i++;//sposto i sul primo numero giocato
        
        //pulisco numeri
        for(k=0; k < 10; k++)
                strcpy(numeri[k], "");
        
        k=0;
        while(strcmp(parole[i], "-i") != 0){
                strcpy(numeri[k], parole[i]);
                i++; k++;
        }
        
        int max_numeri_accettati=10;
        ret = check_correttezza_numeri(numeri, k);
        
        if(ret==0 && k <= max_numeri_accettati){
                controlli_passati++;
        }
        else{
                inviaMessaggio("SERVER: Errore, i numeri possono essere 10 al massimo, compresi tra 1 e 90 e senza duplicati!\n");
                return -1;
        }
        
        //4) controllo che gli importi scommessi siano al max 5 e che siano tutti positivi
        //arrivo qui che l'indice i è su -i
        char importi[5][1024];        
                
        int h=0;i++;//sposto i sul primo importo
        
        //pulisco importi
        for(h=0; h < 5; h++)
                strcpy(importi[h], "");
        
        h=0;
        while(strcmp(parole[i], "") != 0){
                strcpy(importi[h], parole[i]);
                i++; h++;
        }
        
        int max_importi_accettati=5;
        ret = check_correttezza_importi(importi, h);
        
        if(ret==0 && h <= max_importi_accettati){
                controlli_passati++;
        }
        else{
                inviaMessaggio("SERVER: Errore, gli importi della giocata devono essere al massimo 5 e tutti positivi!\n");
                return -1;
        }
        
        //5) controllo se il numero degli importi specificati non sia superiore
        //alla quantità dei numeri giocati
        if(h<=k){
        	controlli_passati++;
        }
        else{
        	inviaMessaggio("SERVER: Errore, il numero degli importi specificati non può essere maggiore dei numeri giocati!\n");
            return -1;
        }
        
        //se sono stati superati tutti i controlli, creo la schedina e salvo la giocata su file
        if(controlli_passati==5){
                struct schedina s = crea_schedina(ruote, numeri, importi);
                registra_giocata(&s);//registra su file la giocata
                inviaMessaggio("SERVER: giocata registrata correttamente! \n");
                return 0;
        }
        //non si dovrebbe mai entrare in questo if in quanto per ogni caso d'errore c'è un inviaMessaggio con una return
        //ma per sicurezza ne aggiungo uno generico
        else{
                inviaMessaggio("SERVER: Errore nella sintassi del comando !invia_giocata. \n"); 
        }
        
        return 0;
}

//funzione che si occupa di verificare che il sessionID ricevuto dal client corrisponda con quello salvato a inizio sessione
void check_validita_sessionID(char *sessionID_ricevuto){
		//se il sessionID corrisponde, potrei stampare ogni volta un messaggio di successo
		//ma ho deciso di commentarlo per una maggiore leggibilità delle stampe del server
        if(strcmp(sessionID, sessionID_ricevuto)==0);
                //printf("SERVER: Corrispondenza SessionID %s verificata \n", sessionID_ricevuto);
        else
                printf("SERVER: Errore, sessionID ricevuto dal client non corrispondente!\n");
}

//funzione che conta le righe interne a un file, serve per stabilire quante volte il client dovrà fare la riceviComando,
//dato che per ogni riga del file effettuo una inviaComando. Distinguo il conteggio in base al tipo specificato nel comando
//!vedi_giocate 0/1
int conta_righe_file(int tipo, int timestampUltimaEstrazione){
        int quante_righe=0;
  		char *line_buf2 = NULL;
  		size_t line_buf_size2 = 0;
  		ssize_t line_size2;
  	
  		char* path = "./Schedine Utenti/";
        int lunghezza_path_cartella=strlen(path);
        int lunghezza_username=strlen(username_utente_loggato);
        char file_path[lunghezza_path_cartella + lunghezza_username + 5]; // "./Schedine Utenti/" + "user" + ".txt" + "/0"
        sprintf(file_path, "%s%s.txt", path, username_utente_loggato);

  	FILE *fd2 = fopen(file_path, "r"); //apro nome_utente.txt in lettura
  	
  	if (!fd2) {
    	        printf("SERVER: Errore nell’apertura del file %s!\n", file_path);
    	        exit(-1);
  	}
  	
  	
  	//devo confrontare timestampUltimaEstrazione con il timestamp delle giocate dell utente in nomeutente.txt
  	//e incrementare di conseguenza del numero di righe giusto
  	line_size2 = getline(&line_buf2, &line_buf_size2, fd2);
  	while (line_size2 >= 0) { // Loop in cui analizza tutte le righe del file utente 
  	        char separatore2[2] = " ";
  	        int timestampGiocata = atoi(strtok(line_buf2, separatore2)); 
  	        
  	        if(tipo==0){
  	                if(timestampUltimaEstrazione>timestampGiocata){
  	                        quante_righe++;
  	                }
  	        }
  	        else if(tipo==1){
  	                if(timestampUltimaEstrazione<timestampGiocata){
  	                        quante_righe++;
  	                }
  	        }
  	        else{
  	                printf("SERVER: Errore, !vedi_giocate accetta come argomento soltanto 0/1!\n");
  	                return -1;
  	        }
  	                 	            	
            	line_size2 = getline(&line_buf2, &line_buf_size2, fd2); //prendo la prossima linea e continuo
  	}
  	
  	fclose(fd2);
  	return quante_righe;
}

//funzione invocata dalla comando_vedi_giocate per "tradurre" una riga relativa a una giocata presente in Schedine Utenti/nomeutente.txt
// (es. formato: 1598894088 1 1 1 0 0 0 0 0 0 0 0 1 2 3 6 7 11 12 13 14 15 1.00 2.00 3.00 4.00 5.00 ) in un formato leggibile da
//restituire al client (es. Bari Cagliari Firenze 1 2 3 6 7 11 12 13 14 15  * 1.00 estratto * 2.00 ambo * 3.00 terna * 4.00 quaterna * 5.00 cinquina)
void traduzione_schedina_giocata(char parole[30][1024], char stringaFinale[4096]){

        strcpy(stringaFinale,""); //pulisco
        
        int i=0;
        for(i=1; i < 12; i++){
                if(atoi(parole[i])==1){
                        switch(i){
                                case 1:
                                        strcat(stringaFinale,"Bari ");
                                        break;
                                case 2:
                                        strcat(stringaFinale,"Cagliari ");
                                        break;
                                case 3:
                                        strcat(stringaFinale,"Firenze ");
                                        break;
                                case 4:
                                        strcat(stringaFinale,"Genova ");
                                        break;
                                case 5:
                                        strcat(stringaFinale,"Milano ");
                                        break;
                                case 6:
                                        strcat(stringaFinale,"Napoli ");
                                        break;
                                case 7:
                                        strcat(stringaFinale,"Palermo ");
                                        break;
                                case 8:
                                        strcat(stringaFinale,"Roma ");
                                        break;
                                case 9:
                                        strcat(stringaFinale,"Torino ");
                                        break;
                                case 10:
                                        strcat(stringaFinale,"Venezia ");
                                        break;
                                case 11:
                                        strcat(stringaFinale,"Nazionale ");
                                        break;
                        }
               }
        }
        
        for(i=12; i < 22; i++){
                if(atoi(parole[i])!=0){
                        strcat(stringaFinale,parole[i]);
                        strcat(stringaFinale," ");
                }
        }
        
        strcat(stringaFinale," * ");
        
        for(i=22; i < 27; i++){
                if(atof(parole[i])!=0){
                        
                        strcat(stringaFinale,parole[i]);
                        switch(i){
                                case 22:
                                        strcat(stringaFinale," estratto * ");
                                        break;
                                case 23:
                                        strcat(stringaFinale," ambo * ");
                                        break;
                                case 24:
                                        strcat(stringaFinale," terna * ");
                                        break;
                                case 25:
                                        strcat(stringaFinale," quaterna * ");
                                        break;
                                case 26:
                                        strcat(stringaFinale," cinquina * ");
                                        break;
                        }
                }
        }
        
}

//funzione che gestisce il comando !vedi_giocate 0/1. Leggendo dal file personale Schedine Utenti/nomeutente.txt, conto prima
//il numero di righe (giocate) da stampare a video nel client (distinguendo tra giocate attive (1) e scadute(0), invio tale numero
//tramite una prima inviaMessaggio, e infine seguono tante inviaMessaggio (e quindi riceviMessaggio nel client)
// contenenti le righe corrispondenti alle giocate da restituire
void comando_vedi_giocate(int tipo){
        
  	char *line_buf2 = NULL;
  	size_t line_buf_size2 = 0;
  	ssize_t line_size2;
        int timestampUltimaEstrazione=0;
        
        FILE *fd2 = fopen("ultima_estrazione.txt", "r");
        if (!fd2) {
    	        printf("SERVER: Errore nell’apertura del file ultima_estrazione.txt! \n");
    	        exit(-1);
  	}
  	
  	line_size2 = getline(&line_buf2, &line_buf_size2, fd2);
  	while (line_size2>=0) {
		char separatore[2] = "\n";
		timestampUltimaEstrazione = atoi(strtok(line_buf2, separatore)); 
            	break;
  	}
  	fclose(fd2);
  	
        //variabili per la lettura delle righe di un file
	char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;
  	
  	char* path = "./Schedine Utenti/";
    int lunghezza_path_cartella=strlen(path);
    int lunghezza_username=strlen(username_utente_loggato);
    char file_path[lunghezza_path_cartella + lunghezza_username + 5]; // "./Schedine Utenti/" + "user" + ".txt" + "/0"
    sprintf(file_path, "%s%s.txt", path, username_utente_loggato);

  	FILE *fd = fopen(file_path, "r"); //apro nome_utente.txt in lettura

  	if (!fd) {
    	        printf("SERVER: Errore nell’apertura del file %s! \n", file_path);
    	        exit(-1);
  	}
  	
        //funzione che ritorna il numero di righe del file delle giocate utente,
        //in base al tipo (0/1) conta le righe relative alle giocate "scadute" o ancora "valide rispettivamente
        //questo numero mi servirà poi per dire al client quante "riceviComando" dovrà effettuare 
        //dato che ho scelto di inviare con una inviaComando una sola riga di risposta
        // ESEMPIO: 1a inviaComando => conterrà numero righe file e quindi il numero di riceviComando da fare (es.2) => 1a riceviComando
        //          2a inviaComando => "1) Roma 15 19 20 * 10.00 terno * 5.00 ambo" => 2a riceviComando
        //          3a inviaComando => "2) Milano Napoli Palermo 90* 15.00 estratto" => 3a riceviComando
        
	line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

    char quante_righe[1024];
    strcpy(quante_righe,"");
    int r=conta_righe_file(tipo, timestampUltimaEstrazione);
        
        //caso di errore dove l'utente specifica !vedi_giocate n con n != 0,1
        if(r==-1){
                inviaMessaggio("SERVER: Errore sintassi comando !vedi_giocate\n");
                return;
        }
        if(r==0){
                inviaMessaggio("SERVER: Nessun risultato.\n");
                return;
        }
        
        sprintf(quante_righe, "%i", r);
        inviaMessaggio(quante_righe);  	
            	
  	while (line_size >= 0) { // Loop in cui analizza tutte le righe del file utente

  		//Variabili per ottenere le sottostringhe di ogni riga del file che salva le giocate dell'utente
  		int quante_parole = 0;
		char separatore[2] = " ";
		char* pezzetto = strtok(line_buf, separatore); 
		int timestampGiocata=atoi(pezzetto);
		char parole[30][1024];
		int i=0;

            	while (pezzetto != NULL) { //fino alla fine della riga
		        strcpy(parole[quante_parole],pezzetto);
		        quante_parole++;
                	pezzetto = strtok(NULL,separatore); 
            	}
            	
            	char stringaFinale[4096];
		strcpy(stringaFinale, "");
            	traduzione_schedina_giocata(parole, stringaFinale);
            	
        	//qui devo inviare <=> la stringa è di tipo0/1
        	if(tipo==0){
                if(timestampUltimaEstrazione>timestampGiocata){
                        inviaMessaggio(stringaFinale);
                }
  	        }
  	        else if(tipo==1){
  	                if(timestampUltimaEstrazione<timestampGiocata){
  	                        inviaMessaggio(stringaFinale);
  	                }
  	        }
            	
            	for(i=0; i < 30; i++)
		        strcpy(parole[i], "");
            	
            	line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo
  	}
  	
  	fclose(fd);
}


//funzione che si occupa di tradurre una riga del file giocate_in_attesa_di_estrazione.txt in una schedina s
//questo processo servirà per passare poi la schedina s generata alla funzione controllo_vincite_schedina
//che si occuperà di confrontare ogni riga dell'estrazione avvenuta con il contenuto della schedina s
struct schedina traduzione_giocata_in_schedina(char* str){

    char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;
  	
  	int ruote[11][1024];
  	int numeri[10][1024];
  	float importi[5][1024];
  	struct schedina s;

                int i=0;
                for(i=0; i<11; i++){
                        str = strtok(NULL, " ");
                        s.ruote[i] = atoi(str);
                }
                
                for(i=0; i<10; i++){
                        str = strtok(NULL, " ");
                        s.numeri[i] = atoi(str);
                }
                
                for(i=0; i<5; i++){
                        str = strtok(NULL, " ");
                        s.importi[i] = atof(str);
                }
		
  	
  	return s;
}

//funzione che restituisce il fattoriale di un numero, utilizzata nella calcolo_vincita
int fattoriale(int n){
        int i=1; int r=1;
        for(i=1; i<=n; i++)
                r*=i;
        return r;
}

//funzione che dati n e m, restituisce il binomiale, utilizzata nella calcolo_vincita
int binomiale(int n, int m){
    int k=0;
    int numeratore=0; int denominatore=0;
        
  	numeratore=fattoriale(n);
    denominatore=(fattoriale(m)*fattoriale(n-m));
    k=numeratore/denominatore;
        
    return k;
}


//funzione richiamata dalla controllo_vincite_schedina, si occupa di calcolare l'ammontare della vincita
//relativa a una riga (ruota), fa uso delle funzioni ausiliarie fattoriale e binomiale
float calcolo_vincita(int tipo_giocata, float importo, int numeri_giocati, int ruote_selezionate, int numeri_indovinati){
        float vincita=0;

        switch(tipo_giocata){
                case 1:
                        vincita=11.23;
                        break;
                case 2:
                        vincita=250;
                        break;
                case 3:
                        vincita=4500;
                        break;
                case 4:
                        vincita=120000;
                        break;
                case 5:
                        vincita=6000000;
                        break;        
        }
        
        vincita/=binomiale(numeri_giocati, tipo_giocata);
        vincita/=ruote_selezionate;
        vincita*=importo;
        vincita*=binomiale(numeri_indovinati, tipo_giocata);
        return vincita;
}


//funzione richiamata dalla controllo_giocate_in_attesa per ogni schedina s
//presente nel file giocate_in_attesa_di_estrazione.txt. Controlla corrispondenze tra numeri
//di ogni riga (ruota) del file ultima_estrazione.txt e i numeri giocati per quella ruota.
//infine si occupa di inserire le giocate vincenti nei file Schedine Vincenti/nomeutente.txt
void controllo_vincite_schedina(struct schedina s, int timestamp_estrazione_in_corso, char* username_giocata){
    //var per lettura da file
    char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;
  	
  	int i=0; int k=0;
  	int quanti_importi=0;
  	int quanteRuote=0;
  	
  	//variabili per ottenere il timestamp corrente
  	time_t t = time(NULL);
  	struct tm tm = *localtime(&t);
  	char timestamp[50]; //stringa dove verrà salvato il timestamp
  	
  	//var per vittoria
  	//int vincita[5];
  	int vittoria=0;
  	char stringa_vittoria[1024];
  	
  	char* path = "./Schedine Vincenti/";
        int lunghezza_path_cartella=strlen(path);
        int lunghezza_username=strlen(username_giocata);
        char file_path[lunghezza_path_cartella + lunghezza_username + 5]; // "./Schedine Utenti/" + "user" + ".txt" + "/0"
        sprintf(file_path, "%s%s.txt", path, username_giocata);
        
        FILE *fd = fopen("ultima_estrazione.txt", "r");
        FILE *fd1 = fopen(file_path, "a"); //se non esiste lo crea

  	if (!fd) {
    	        printf("SERVER: Errore nell’apertura del file ultima_estrazione.txt! \n");
    	        exit(-1);
  	}
  	
  	if (!fd1) {
    	        printf("SERVER: Errore nell’apertura del file schedine_vincenti.txt! \n");
    	        exit(-1);
  	}
  	
  	//      n = numeri giocati / m = estratto->1, ambo->2, ...
  	//
  	//        (   n   )       n!
  	//    k = (       ) = ----------    
  	//        (   m   )    m!(n-m)!
  	
  	for(i=0; i<11; i++){
  	        if(s.ruote[i]==1)
  	                quanteRuote++;
  	}
  	
  	int n=0;int m=0;
  	//ricavo n per il binomiale
  	for(i=0; i<10; i++){
  	        if(s.numeri[i]!=0)
  	                n++;
  	}
  	
  	//una getline a vuoto per saltare il timestamp dell'ultima estrazione e passare alla matrice
	line_size = getline(&line_buf, &line_buf_size, fd);
	line_size = getline(&line_buf, &line_buf_size, fd);//estraggo la prima giocata in attesa e la inserisco in line_buf
	
	int indiceRuota=0;int numeri_indovinati=0;
	while (line_size >= 0) { // Loop in cui analizza tutte le righe del file ultima_estrazione.txt

  		//Variabili per ottenere le sottostringhe di ogni riga
  		int quante_parole = 0;
		char separatore[2] = " ";
		char* pezzetto = strtok(line_buf, separatore); 
		char parole[30][1024];
		int j=0;
		//resetto le variabili per la vincita ad ogni riga estratta
		numeri_indovinati=0;
		vittoria=0; //variabile ausiliaria per stampare su file solo in caso di vincita
		
            	sprintf(timestamp,"%i ",timestamp_estrazione_in_corso);
            	strcat(stringa_vittoria,timestamp);

            	while (pezzetto != NULL) { //fino alla fine della riga
		        	strcpy(parole[quante_parole],pezzetto);
		        	quante_parole++;
                	pezzetto = strtok(NULL,separatore); 
            	}
            	
            	strcat(stringa_vittoria,parole[0]); //inserisco la ruota attuale (stampo solo in caso di vincita)
            	
            	//arrivo qua con parole che contiene tutta una riga del file ultima_estrazione.txt
            	if(s.ruote[indiceRuota]==1){
            	        for(j=1; j<6; j++){ //for per scorrere tra i numeri estratti parole[1] -> parole[5]
            	                int numeroEstratto = atoi(parole[j]); //faccio il cast del numero contenuto nella stringa
            	                for(i=0;i<10;i++)
            	                        if(s.numeri[i]==numeroEstratto){
            	                            char numero[1024];
								            sprintf(numero," %i",numeroEstratto);
								            strcat(stringa_vittoria,numero); //inserisco i numeri vincenti in stringa_vittoria
				            				numeri_indovinati++;
            	                        }
            	        }
            	        
            	}           	
            	
            	strcat(stringa_vittoria," >> ");
            	
            	//per ogni riga (=ruota scelta)
            	if(numeri_indovinati==5){ //se ho fatto cinquina
            	        if(s.importi[4]!=0){ //e se avevo scommesso sulla cinquina
            	                float vincita; char strVincita[1024];
            	                vincita=calcolo_vincita(5, s.importi[4], n, quanteRuote, 5);
            	                sprintf(strVincita,"%0.2f",vincita);
    							strcat(stringa_vittoria," Cinquina ");
    							strcat(stringa_vittoria,strVincita);
            	                vittoria=1;
            	        }
            	}
            	
            	if(numeri_indovinati>=4){ //se ho fatto quaterna
            	        if(s.importi[3]!=0){ //e se avevo scommesso sulla quaterna
            	                float vincita; char strVincita[1024];
            	                vincita=calcolo_vincita(4, s.importi[3], n, quanteRuote, numeri_indovinati);
            	                sprintf(strVincita,"%0.2f",vincita);
								strcat(stringa_vittoria," Quaterna ");
								strcat(stringa_vittoria,strVincita);
            	                vittoria=1;
            	        }
            	}
            	
            	if(numeri_indovinati>=3){ //se ho fatto terno
            	        if(s.importi[2]!=0){ //e se avevo scommesso sull'terno
            	                float vincita; char strVincita[1024];
            	                vincita=calcolo_vincita(3, s.importi[2], n, quanteRuote, numeri_indovinati);
            	                sprintf(strVincita,"%0.2f",vincita);
								strcat(stringa_vittoria," Terno ");
								strcat(stringa_vittoria,strVincita);
            	                vittoria=1;
            	        }
            	}
            	
            	if(numeri_indovinati>=2){ //se ho fatto ambo
            	        if(s.importi[1]!=0){ //e se avevo scommesso sull'ambo
            	                float vincita; char strVincita[1024];
            	                vincita=calcolo_vincita(2, s.importi[1], n, quanteRuote, numeri_indovinati);
            	                sprintf(strVincita,"%0.2f",vincita);
								strcat(stringa_vittoria," Ambo ");
								strcat(stringa_vittoria,strVincita);
            	                vittoria=1;
            	        }
            	}
            	
            	if(numeri_indovinati>=1){ //se ho fatto estratto
            	        if(s.importi[0]!=0){ //e se avevo scommesso su estratto
            	                float vincita; char strVincita[1024];
            	                vincita=calcolo_vincita(1, s.importi[0], n, quanteRuote, numeri_indovinati);
            	                sprintf(strVincita,"%0.2f",vincita);
								strcat(stringa_vittoria," Estratto ");
								strcat(stringa_vittoria,strVincita);
            	                vittoria=1;
            	        }
            	}
            	
            	
            	if(vittoria == 1)
            	        fprintf(fd1,"%s\n",stringa_vittoria); //scrivo la vittoria sul file
            	        
            	strcpy(stringa_vittoria, "");
                line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima riga e continuo
                indiceRuota++;
	}
	
	fclose(fd);
	fclose(fd1);
}

//effettua il check nel file giocate_in_attesa_di_estrazione.txt, crea una schedina s per ogni giocata (ogni riga)
//e chiama per ogni schedina s la funzione controllo_vincite_schedina, che si occuperà
// di inserire le giocate vincenti nei file Schedine Vincenti/nomeutente.txt
void controlla_giocate_in_attesa(int timestamp_estrazione_in_corso){
    //variabili per leggere una riga per volta da file
    char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;
  	char* usr;//usata per contenere i token generati dalla strtok, e passata alla funzione che crea una schedina

  	FILE *fd = fopen("giocate_in_attesa_di_estrazione.txt", "r");

  	if (!fd) {
    	        printf("SERVER: Errore nell’apertura del file giocate_in_attesa_di_estrazione.txt! \n");
    	        exit(-1);
  	}

	line_size = getline(&line_buf, &line_buf_size, fd);

  	while (line_size >= 0) {
  	        struct schedina s;
  	        usr = strtok(line_buf, " ");
  	        s=traduzione_giocata_in_schedina(usr);
  	        controllo_vincite_schedina(s, timestamp_estrazione_in_corso, usr);
  	        line_size = getline(&line_buf, &line_buf_size, fd);
  	}
  	
  	fclose(fd);
}


//Funzione di gestione per il comando !vedi_estrazione, restituisce il testo formattato intero
//tramite una inviaMessaggio al client, che lo stamperà
void comando_vedi_estrazione(char n[1024], char ruota[1024]){

        char outputstr[8192] = ""; //conterrà la risposta da inviare al client
        int numero_blocchi_estrazione_da_stampare=0;
        numero_blocchi_estrazione_da_stampare=atoi(n);
        int i=0;
		
		FILE* fd = fopen("estrazioni_ordinate.txt", "r");
        	
    	if(!fd){
    		printf("SERVER: Errore nell'apertura del file estrazioni_ordinate.txt!\n");
    		return;        	
    	}
		
		//se ho specificato il nome della ruota nel comando !vedi_estrazione
        if(strcmp(ruota,"") != 0){
        	ruota = strtok(ruota,"\n");
        	strcpy(outputstr, "");

		    const size_t line_size = 1024;
		    char* line = malloc(line_size);
		    int righe=0;
		    while (fgets(line, line_size, fd) != NULL && righe<12*numero_blocchi_estrazione_da_stampare)  {
		    		char tmp[1024];
		    		strcpy(tmp, line);
		    		char* token = strtok(line, " ");
		    		
		    		//faccio lowercase del nome della ruota (Bari->bari)
		    		for(i = 0; token[i]; i++){
						token[i] = tolower(token[i]);
					}
					
					//se trovo corrispondenza tra nome ruota specificato e nome ruota letto, concateno
		    		if(strcmp(token, ruota)==0)
						strcat(outputstr, tmp);
						
					righe++;
		    }
		    
		    if(line)
		            free(line);
	  		
	  		fclose(fd);
        	inviaMessaggio(outputstr);
        }
        //se nel comando !vedi_estrazione non ho specificato alcuna ruota, le restituisco tutte
        else{

		    const size_t line_size = 100;
		    char* line = malloc(line_size);
		    int righe=0;
		    while (fgets(line, line_size, fd) != NULL && righe<12*numero_blocchi_estrazione_da_stampare)  {
					strcat(outputstr, line);
					righe++;
		    }
		    
		    if(line)
		            free(line);
	  		
	  		fclose(fd);
        	inviaMessaggio(outputstr);
        }
}


//funzione che gestisce il comando !vedi_vincite
void comando_vedi_vincite(){

        char stringa_finale[4096]; //restituisce il testo formattato che il client stamperà
        strcpy(stringa_finale, ""); 
        char tmp[30]; //usata per strcat
        
        //variabili per il conteggio delle vincite cumulative su più estrazioni
        int k;
        float importi[5]; 
        for(k=0; k<5; k++) importi[k]=0;
        
        //usato per raggruppare, nel file Schedine Vincenti/nomeutente.txt, le vincite
        //relative a una stessa estrazione
        time_t timestamp_estrazione_riga_precedente;       
        
        //variabili per lettura da file
        char *line_buf = NULL;
  		size_t line_buf_size = 0;
  		ssize_t line_size;
		
		//vado a leggere dal file Schedine Vincenti/nomeutente.txt, dove sono già presenti
		//le vincite dell'utente
        char* path = "./Schedine Vincenti/";
        int lunghezza_path_cartella=strlen(path);
        int lunghezza_username=strlen(username_utente_loggato);
        char file_path[lunghezza_path_cartella + lunghezza_username + 5]; // "./Schedine Vincenti/" + "user" + ".txt" + "/0"
        sprintf(file_path, "%s%s.txt", path, username_utente_loggato);
        
        FILE* f;
        if ((f=fopen(file_path, "r"))==NULL){
            	printf("ERRORE: non è stato possibile aprire il file Schedine Vincenti/%s.txt! \n", username_utente_loggato);
                inviaMessaggio("SERVER: Spiacenti, non è stata trovata alcuna vincita.\n");
                return;
        }
        
        int primo_giro=1;
        line_size = getline(&line_buf, &line_buf_size, f);

  		while (line_size >= 0) { // scorro tutte le linee del file delle vincite dell'utente
  	        int quante_parole = 0;
			char separatore[2] = " ";
			char* pezzetto = strtok(line_buf, separatore);
  	        char parole[20][1024];
            for(k=0; k<20; k++) strcpy(parole[k],""); //pulisco parole
  	        
  	        while (pezzetto != NULL) { //fino alla fine della riga
		        strcpy(parole[quante_parole],pezzetto);
		        quante_parole++;
                pezzetto = strtok(NULL,separatore); 
            }
            	
  	        time_t timestamp_estrazione_riga_attuale; //timestamp dell'estrazione sulla riga da cui leggo
  	        timestamp_estrazione_riga_attuale = atoi(parole[0]);
  	        char timestamp_formato_data[50]; //stringa dove verrà salvato il timestamp nel formato "dd-mm-yyyy hh-mm-ss"        
            struct tm ts = *localtime(&timestamp_estrazione_riga_attuale); //struct per il timestamp
            sprintf(timestamp_formato_data,"%02d-%02d-%d %02d:%02d:%02d",ts.tm_mday, ts.tm_mon + 1,ts.tm_year + 1900, ts.tm_hour, ts.tm_min, ts.tm_sec);
  	        
  	        //faccio la differenza tra i timestamp per vedere se le vincite sono relative a una stessa estrazione o meno
  	        double timestamp_estrazione_uguali = difftime(timestamp_estrazione_riga_precedente,timestamp_estrazione_riga_attuale);
  	        
  	        //organizzo la stringa finale da restituire, formattando
  	        if(primo_giro==1){
  	                timestamp_estrazione_riga_precedente = timestamp_estrazione_riga_attuale;
  	                char str_tmp[1024] = "";
  	                sprintf(str_tmp, "Estrazione del %s \n", timestamp_formato_data);
  	                strcat(stringa_finale, str_tmp);
  	                //printo resto della riga + \n
  	                int i=0;
  	                for(i=1; i < 20; i++)
  	                        if(strcmp(parole[i],"")!=0){
  	                                char str_tmp[1024] = "";
  	                                strcpy(str_tmp, parole[i]);
  	                                if(strcmp(parole[i],"Estratto") == 0) importi[0] = importi[0] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Ambo") == 0) importi[1] = importi[1] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Terno") == 0) importi[2] = importi[2] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Quaterna") == 0) importi[3] = importi[3] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Cinquina") == 0) importi[4] = importi[4] + atof(parole[i+1]);
                                        strcat(stringa_finale, parole[i]);
                                        if(strcmp(parole[i+1],"") != 0)
                                                strcat(stringa_finale, " ");    
  	                        }
  	                                else continue;
  	                primo_giro=0;
  	        }
  	        //se la vincita che sto leggendo è relativa alla stessa estrazione precedente
  	        else if(timestamp_estrazione_uguali==0){
  	                //printo resto della riga + \n
  	                int i=0;
  	                for(i=1; i < 20; i++)
  	                        if(strcmp(parole[i],"")!=0){
                                        if(strcmp(parole[i],"Estratto") == 0) importi[0] = importi[0] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Ambo") == 0) importi[1] = importi[1] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Terno") == 0) importi[2] = importi[2] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Quaterna") == 0) importi[3] = importi[3] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Cinquina") == 0) importi[4] = importi[4] + atof(parole[i+1]);
                                        strcat(stringa_finale, parole[i]);
                                        if(strcmp(parole[i+1],"") != 0)
                                                strcat(stringa_finale, " ");
  	                       } 
  	                                else continue;
  	        }
  	        //se invece è relativa a un estrazione più recente
  	        else if(timestamp_estrazione_uguali!=0){
  	                strcat(stringa_finale, "******************************************\n");
  	                char str_tmp[1024] = "";
  	                sprintf(str_tmp, "Estrazione del %s \n", timestamp_formato_data);
  	                strcat(stringa_finale, str_tmp);
  	                //printo resto della riga + \n
  	                int i=0;
  	                for(i=1; i < 20; i++)
  	                        if(strcmp(parole[i],"")!=0){
                                        if(strcmp(parole[i],"Estratto") == 0) importi[0] = importi[0] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Ambo") == 0) importi[1] = importi[1] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Terno") == 0) importi[2] = importi[2] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Quaterna") == 0) importi[3] = importi[3] + atof(parole[i+1]);
                                        if(strcmp(parole[i],"Cinquina") == 0) importi[4] = importi[4] + atof(parole[i+1]);
                                        strcat(stringa_finale, parole[i]);
                                        if(strcmp(parole[i+1],"") != 0)
                                                strcat(stringa_finale, " ");    
                                }
  	                        else continue;
  	                //aggiorno il timestamp dell'estrazione per il prossimo ciclo
  	                timestamp_estrazione_riga_precedente = timestamp_estrazione_riga_attuale;
  	        }
                
    	        line_size = getline(&line_buf, &line_buf_size, f); //avanzo alla prossima riga
  		}
        strcat(stringa_finale, "******************************************\n\n");
        char str_tmp[1024] = "";
        sprintf(str_tmp,"Vincite su ESTRATTO: %f\nVincite su AMBO: %f\nVincite su TERNO: %f\nVincite su QUATERNA: %f\nVincite su CINQUINA: %f\n",importi[0],importi[1],importi[2],importi[3],importi[4]);
        strcat(stringa_finale, str_tmp);
        
        //se non leggo niente, ritorno al client con errore
        if(strcmp(stringa_finale, "")==0)
                inviaMessaggio("SERVER: Spiacenti, non è stata trovata alcuna vincita.\n");
        //altrimenti il client stamperà tutto il blocco già formattato
        else
                inviaMessaggio(stringa_finale);
                
        fclose(f);
}



//funzione che viene eseguita ciclicamente nel main, da parte del processo figlio, finchè la variabile connesso vale 1.
//si occupa di eseguire la riceviMessaggio che a sua volta contiene una recv, salvare in un buffer, e gestire con diverse funzioni
//i comandi ricevuti dal client
void riceviComando(){
       
        //salvo per prima cosa il comando intero nel buffer
        riceviMessaggio(buffer);
        
        int quante_parole = 0;
		char separatore[2] = " ";
		char* pezzetto = strtok(buffer, separatore);
		char parole[32][1024]; //Array per le substring ottenute tramite strtok

        //pulisco array parole[]
		int i=0;
		for(; i<32; i++) strcpy(parole[i],""); 

        //riempio l'array con le sottostringhe che compongono il comando
		while (pezzetto != NULL) {
	        if(pezzetto[0]=='#')
	                check_validita_sessionID(++pezzetto); //elimino il #
	        else{
		        strcpy(parole[quante_parole],pezzetto);
		        quante_parole++;
		}
                pezzetto = strtok(NULL,separatore); 
        } 
                
        //stampo la prima keyword del comando (ad es. "!signup", "!login"...)
        char comando[30];
        strcpy(comando, parole[0]);
        strtok(comando, "\n");
        //distinguo se sono loggato o meno, in modo da stampare "ricevuto comando <comando> da <nome_username>"
        //oppure semplicemente "ricevuto <comando>"
        if(strcmp(username_utente_loggato, "")!=0)
        	printf("SERVER: ricevuto comando %s da %s \n",comando, username_utente_loggato);
        else
     		printf("SERVER: ricevuto comando %s \n",comando);
        
        //Qua sotto, in base ai contenuti di parole[i], smisto la responsabilità ai vari gestori dei comandi
        
        //SIGNUP
        if(strcmp(parole[0],"!signup")==0){
                if(strcmp(parole[1], "")==0 || strcmp(parole[2], "")==0){
                        inviaMessaggio("SERVER: Errore, specificare !signup <username> <password>. \n");
                        return;
                }
                if(comando_signup(parole[1], parole[2])==1)
                        printf("SERVER: l'utente '%s' è stato registrato correttamente! \n", parole[1]);
        }
        
        
        //LOGIN
        if(strcmp(parole[0],"!login")==0){
                if(strcmp(parole[1], "")==0 || strcmp(parole[2], "")==0){
                        inviaMessaggio("SERVER: Errore, specificare !login <username> <password>. \n");
                        return;
                }
                comando_login(parole[1], parole[2]);
        }
        
        
        //ESCI
        if(strcmp(parole[0],"!esci\n")==0){
                comando_esci(parole[1]); //contiene il sessionID ricevuto dal client
        }
        
        
        //INVIA GIOCATA
        if(strcmp(parole[0],"!invia_giocata")==0){
                        if(comando_invia_giocata(parole)==0)
                                printf("SERVER: la schedina è stata registrata correttamente! \n");
                        else
                                printf("SERVER: Errore, non è stato possibile registrare la schedina! \n");
        }
        
        
        //VEDI GIOCATE
        if(strcmp(parole[0],"!vedi_giocate")==0){
                        int tipo = atoi(parole[1]);
                        comando_vedi_giocate(tipo);
        }
        
        
        //VEDI VINCITE
        if(strcmp(parole[0],"!vedi_vincite\n")==0){
                        comando_vedi_vincite();
        }
        
        
        //VEDI ESTRAZIONE
        if(strcmp(parole[0],"!vedi_estrazione")==0){
                        comando_vedi_estrazione(parole[1], parole[2]);
        }
        
                
        return;
}

//funzione che restituisce 0 se devo vietare la connessione, 1 altrimenti.
//invocata nel main per vedere se la connessione è lecita, o se si rientra nella "Black list", ovvero se si è fallito
//oltre 3 volte il login inserendo credenziali errate. Se l'ip rientra in tale lista, viene settata la variabile connesso=0
//che non permette di entrare nel while del processo figlio, viene stampato un msg di errore e chiuso il socket.
int controllo_blacklist_IP(){ 
	char timestamp[1024];

	//variabili per la lettura delle righe di un file
	char *line_buf = NULL;
  	size_t line_buf_size = 0;
  	ssize_t line_size;

  	FILE *fd = fopen("black_list.txt", "r"); //apro black_list.txt in lettura

  	if (!fd) {
    	        printf("SERVER: Errore nell’apertura del file black_list.txt! \n");
    	        exit(-1);
  	}

	sprintf(timestamp,"%d",(int)time(NULL));

	line_size = getline(&line_buf, &line_buf_size, fd); //prende la prima linea del file fd

  	while (line_size >= 0) { // Loop in cui analizza tutte le linee del file black_list.txt

  		//Variabili per ottenere le sottostringhe tra le '/' di ogni riga del file black_list
  		int quante_parole = 0;
		char separatore[2] = "/";
		char* pezzetto = strtok(line_buf, separatore); 
		char parole[3][1024]; //array per le substrings
		//parole[0] conterrà il timestamp sotto forma di secondi
        //parole[1] conterrà l'IP del client
		//parole[2] conterrà "data ora" (non serve)

            	while (pezzetto != NULL) { //con questo while vado a riempire l'array parole[] con le substring ottenute
		        strcpy(parole[quante_parole],pezzetto);
		        quante_parole++;
                	pezzetto = strtok(NULL,separatore); 
            	}

            	//inet_ntoa(cl_addr.sin_addr) mi restituisce una stringa contenente l'ip del client

            	if(strcmp(inet_ntoa(client_addr.sin_addr),parole[1]) == 0){ 
            		//ho trovato nel file black_list.txt un match con l'IP client che vuole connettersi
            		//devo adesso controllare se il fallimento è abbastanza vecchio da permettere la connessione o meno
            		//tale controllo lo faccio utilizzando il timestamp attuale sotto forma di secondi e quello registrato nel file
            		//se differiscono per meno di 60sec*30min secondi (meno di 30 minuti) devo bloccare la connessione
            		int timestampAttuale = atoi(timestamp);
            		int timestampFile = atoi(parole[0]);
            		int differenza = timestampAttuale - timestampFile;
            		
            		if(differenza <= 1800)
            		        return 0; //1800 è 30*60, ovvero 30 minuti
            	}
           
            	line_size = getline(&line_buf, &line_buf_size, fd); //prendo la prossima linea e continuo

  	}
  	
  	return 1;
}


//funzione che viene chiamata come risposta al segnale di SIGALRM lanciato ogni periodo*60 sec dalla funzione di libreria alarm
//e che si occupa di effettuare una nuova estrazione. Popola quindi i file estrazioni.txt e ultima_estrazione.txt. Infine chiama la
//funzione controlla_giocate_in_attesa che effettua il check nel file giocate_in_attesa_di_estrazione.txt
//(e lo svuota a lavoro ultimato), e inserisce le giocate vincenti nei file presenti in Schedine Vincenti/nome_utente.txt
void nuova_estrazione(int sigalrm){
        //faccio subito ripartire il countdown per l'estrazione successiva
        alarm(periodo);
        
        //COPIO TUTTO IL CONTENUTO DI ESTRAZIONI ORDINATE NEL FILE D'APPOGGIO
        FILE* appoggio = fopen("estrazioni_appoggio.txt", "w");
        FILE* ordinato = fopen("estrazioni_ordinate.txt", "r");
        
        if (!appoggio) {
    	        printf("SERVER: Errore nell’apertura del file estrazioni_appoggio.txt! \n");
    	        exit(-1);
	  	}
	  	
	  	if (!ordinato) {
		        printf("SERVER: Errore nell’apertura del file estrazioni_ordinate.txt! \n");
		        exit(-1);
	  	}

        const size_t line_size = 100;
        char* line = malloc(line_size);
        while (fgets(line, line_size, ordinato) != NULL)  {
			fprintf(appoggio, "%s", line);
        }
        
        if(line)
                free(line);
  		
  	
  		fclose(ordinato);
  		fclose(appoggio);
  		
        
        //faccio uso anche di un file che mantiene solo l'ultima estrazione, e che sovrascrive ogni volta svuotandosi
        FILE* fd2 = fopen("ultima_estrazione.txt", "w");
        //SVUOTO ESTRAZIONI ORDINATE 
        FILE* fd3 = fopen("estrazioni_ordinate.txt", "w");
       
  	
  	if (!fd2) {
    	        printf("SERVER: Errore nell’apertura del file ultima_estrazione.txt! \n");
    	        exit(-1);
  	}
  	
  	if (!fd3) {
    	        printf("SERVER: Errore nell’apertura del file estrazioni_ordinate.txt! \n");
    	        exit(-1);
  	}
  	
  	//salvo il timestamp nel file ultima_estrazione.txt perchè mi aiuta a discriminare le giocate "attive" da quelle "scadute"
  	//e di conseguenza nel gestire il comando !vedi_giocate 0/1 rispettivamente
  	char timestamp[1024];
    sprintf(timestamp,"%d",(int)time(NULL));
    int timestampAttuale = atoi(timestamp);
  	fprintf(fd2, "%i \n", timestampAttuale);
  	
  	//genero 5 numeri casuali, per ogni riga
  	srand(time(NULL));
  	int i=0;int j=0;
  	for(i=0; i<11; i++){
          	//rappresenta una singola riga nel file estrazioni.txt (es. Roma  70   47   57   79   87)
          	char riga_estrazione[1024]="";
          	//rappresenta i numeri estratti su una riga
          	int numeri_riga[11][5];
          	
  	        switch(i+1){
  	                
  	                case 1:
  	                        strcat(riga_estrazione, "Bari      ");
  	                        break;
  	                case 2:
  	                        strcat(riga_estrazione, "Cagliari  ");
  	                        break;
  	                case 3:
  	                        strcat(riga_estrazione, "Firenze   ");
  	                        break;
  	                case 4:
  	                        strcat(riga_estrazione, "Genova    ");
  	                        break;
  	                case 5:
  	                        strcat(riga_estrazione, "Milano    ");
  	                        break;
  	                case 6:
  	                        strcat(riga_estrazione, "Napoli    ");
  	                        break;
  	                case 7:
  	                        strcat(riga_estrazione, "Palermo   ");
  	                        break;
  	                case 8:
  	                        strcat(riga_estrazione, "Roma      ");
  	                        break;
  	                case 9:
  	                        strcat(riga_estrazione, "Torino    ");
  	                        break;
  	                case 10:
  	                        strcat(riga_estrazione, "Venezia   ");
  	                        break;
  	                case 11:
  	                        strcat(riga_estrazione, "Nazionale ");
  	                        break;
  	        }
  	        for(j=0; j<5; j++){
                int random_number = ( rand() % 90 ) + 1;
				int duplicato = 0; //1 in caso sia presente un duplicato
                int k;
                char numero[20]; //contiene la riga di numeri da concatenare alla ruota
		                    
				for(k=0; k<j; k++){ //for controllo duplicati
					if(numeri_riga[i][k] == random_number) duplicato = 1;
				}
			
				if(duplicato == 1)//torno indietro e rieseguo
					    j--; 
				else{
					numeri_riga[i][j] = random_number; //inserisco nell'array dei numeri estratti
					if(numeri_riga[i][j] < 10)//per formattazione i numeri < 10 hanno uno spazio in meno
		            	sprintf(numero," %i   ",numeri_riga[i][j]);
		            else
		                sprintf(numero,"%i   ",numeri_riga[i][j]);
		            
		            strcat(riga_estrazione, numero);
		        }
  	        }
  	        fprintf(fd2, "%s\n", riga_estrazione);
  	        fprintf(fd3, "%s\n", riga_estrazione);
  	}
  	
  	fprintf(fd3, "\n");
  	
  	fclose(fd3);
  	fclose(fd2);
  	
  	//copio tutto il contenuto di estrazioni_appoggio.txt dentro estrazioni_ordinate.txt (sotto all'ultima estrazione)
  	FILE* app = fopen("estrazioni_appoggio.txt", "r");
  	FILE* ord = fopen("estrazioni_ordinate.txt", "a");
  	
  	const size_t line_size2 = 100;
        char* line2 = malloc(line_size2);
        while (fgets(line2, line_size2, app) != NULL)  {
			fprintf(ord, "%s", line2);
        }
        
        if(line2)
                free(line2);
  	
  	fclose(ord);
  	fclose(app);
  	
  	
  	//funzione che per ogni riga del file giocate_in_attesa_di_estrazione.txt, controlla se c'è stata una vincita
  	controlla_giocate_in_attesa(timestampAttuale);
  	//dopo aver controllato le eventuali vincite di tutte le schedine valide per questa estrazione che è avvenuta,
  	//vado a svuota il contenuto del file giocate_in_attesa_di_estrazione.txt, che verrà popolato da altre giocate valide
  	//per la successiva estrazione
  	FILE* fd5 = fopen("giocate_in_attesa_di_estrazione.txt", "w"); 
  	fclose(fd5);
  	
  	printf("SERVER: Nuova estrazione avvenuta con successo!\n");
}

//funzione richiamata nel main, si occupa di creare tutti i file di appoggio necessari
//al corretto funzionamento del gioco. Effettuo accessi in append in modo da crearli, se non esistono.
void creazione_file_supporto(){
	FILE* fd1 = fopen("black_list.txt", "a"); 
  	fclose(fd1);
  	FILE* fd2 = fopen("estrazioni_appoggio.txt", "a"); 
  	fclose(fd2);
  	FILE* fd3 = fopen("estrazioni_ordinate.txt", "a"); 
  	fclose(fd3);
	FILE* fd4 = fopen("giocate_in_attesa_di_estrazione.txt", "a"); 
  	fclose(fd4);
  	FILE* fd5 = fopen("ultima_estrazione.txt", "a"); 
  	fclose(fd5);
  	FILE* fd6 = fopen("utenti_registrati.txt", "a"); 
  	fclose(fd6);

}

//-------------------------------------------------------------------------------------------------------
	

//argc = numero di argomenti passati da terminale
//argv[0]="./lotto_server", argv[1]=porta e argv[2]=periodo (opzionale)
int main(int argc, char* argv[]){
	
	//come prima cosa, richiamo questa funzione per creare tutti i file di appoggio txt
	//se ancora non esistono, in modo da garantire il corretto funzionamento
	creazione_file_supporto();
	//controllo argomenti passati e
	//inizializzo variabili con i valori degli argomenti
	porta = atoi(argv[1]);
	if(argc == 3){
		periodo = atoi(argv[2])*60;
	}
	if(argc > 3){
            	printf("SERVER: Errore! per poter avviare il server devi specificare i seguenti parametri <porta server> e (opzionale) <periodo estrazione>\n");
            	exit(-1);
	}
	
	
	//Creazione socket VUOTO, specifico famiglia (inet) e tipo (TCP)
	socket_server = socket(AF_INET, SOCK_STREAM, 0);
	printf("SERVER: Socket creato con successo\n");
	
	
	//Inizializzazione indirizzo IP, porta e family per la struttura dati server_addr di tipo sockaddr_in
	// Pulizia, metto tutto a zero perchè quando dichiaro una variabile i valori sono casuali
	memset(&server_addr, 0, sizeof(server_addr)); 
	server_addr.sin_family = AF_INET;
	//assegna una porta alla struttura per l'indirizzo
	server_addr.sin_port = htons(porta);
	//assegna un indirizzo IP alla struttura per l'indirizzo
	//va a modificare il campo s_addr di sin_addr di tipo in_addr 
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
	
	
	//Assegno l' indirizzo al socket
	//associo la struttura server_addr di tipo sockaddr_in al socket
	ret = bind(socket_server, (struct sockaddr*)&server_addr, sizeof(server_addr));
	
	if(ret){
		perror("Errore in fase di binding: ");
		exit(-1);
	}
	printf("SERVER: Bind avvenuta con successo\n");
	
	//Specifica che il socket è usato per ricevere richieste di connessione, socket passivo = server. Coda di backlog = 10
	ret = listen(socket_server, 10);
	
	if(ret){
		perror("Errore in fase di listen: ");
		exit(-1);
	}
	
	printf("SERVER: Server in attesa di connessione... \n");
	
	//serve come parametro per la accept
	len = sizeof(client_addr);
	
	
	signal(SIGALRM, nuova_estrazione); 
    //Ridefinisce l'handler per il segnale SIGALRM
    //quando verrà chiamata la alarm e saranno passati i secondi verrà chiamata la funzione estrazione()
    alarm(periodo);
        
	while(1){
		
		//bloccante in caso di coda richieste connessione vuota, mi metto in attesa di una richiesta di connessione
		nuovo_socket = accept(socket_server, (struct sockaddr*)&client_addr, &len);
		printf("SERVER: Richiesta connessione client accettata \n");
		connesso=1;
		
		if(controllo_blacklist_IP() == 0){ //Controllo se il client è nella blacklist per troppi tentativi falliti di login
        	        printf("SERVER: Errore, la connessione con client è stata bloccata \n");
        	        inviaMessaggio("SERVER: l'accesso è stato bloccato per aver superato il limite di tentativi, riprova più tardi\n");
        	        connesso = 0; //Faccio tornare connesso a 0 in modo da non entrare neanche nel while e chiudere subito il socket
                } else {
                	printf("SERVER: Connessione con il client stabilita con successo \n");
                        inviaMessaggio("SERVER: Connessione col client stabilita correttamente\n");
                }
                
                pid = fork();
                
                //Processo Figlio
                if(pid == 0){
                    
                    close(socket_server);
                
                    while(connesso == 1){
		        //contiene una riceviMessaggio, che a sua volta contiene una recv
		        //che essendo bloccante, fa attendere al server che arrivi un comando
		        //da parte del client
		        riceviComando();
		    }
		    
		    close(nuovo_socket);
                    exit(1);
	        }
	        
	        else {
                    // Processo padre
                    close(nuovo_socket);
                }
	}
	
	return 0;
}
