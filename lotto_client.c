#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//VAR GLOBALI
uint16_t lmsg; //lunghezza inviare/ricevere
int connesso = 0; //variabile per la connessione, se il client è connesso è a 1 altrimenti a 0
int utente_loggato = 0; //variabile per il login, se è andato a buon fine è a 1 altrimenti a 0
char sessionID[10]; //stringa casuale di 10 caratteri generata dal server che identifica la sessione
int ret, socket_client;	
//serve solo la struttura dati per l'indirizzo del server
//perchè la connect associa indirizzo server al socket client
struct sockaddr_in server_addr;
//argomenti passati al main
char* IPServer;
int portaServer;
char buffer[4096]; //buffer per scambio dati send/rcv con server
char stringa_comando[1024];

//-------------------------------------------------------------------------------------------------------

//funzione di invio messaggio (stringa) al server, contiene la primitiva send
//si occupa di inviare prima la lunghezza della stringa
void inviaMessaggio(char* buffer){ 
	int ret;
	int len = strlen(buffer) + 1; // Voglio inviare anche il carattere di fine stringa
    lmsg = htons(len); //endianess
	ret = send(socket_client, (void*) &lmsg, sizeof(uint16_t), 0); //prima passo la lunghezza
    ret = send(socket_client, (void*) buffer, len, 0);
    if(ret < 0){ //gestione errori
            perror("CLIENT: Errore in fase di invio messaggio!\n");
    exit(-1);
    }
}

 //funzione per ricevere un messaggio (stringa) dal server, contiene due primitive recv
 //una per la lunghezza e la seconda per la stringa effettiva
void riceviMessaggio(char* buffer){
	int len;
	int ret;
	ret = recv(socket_client, (void*)&lmsg, sizeof(uint16_t), 0);
 	len = ntohs(lmsg); // Rinconverto in formato host
	ret = recv(socket_client, (void*)buffer, len, 0);
	if(ret < 0){ //gestione errori
            perror("CLIENT: Errore in fase di ricezione messaggio!\n");
    exit(-1);
    }
}


//funzione che viene richiamata nel main a seguito di avvenuta connessione
void messaggioBenvenuto(){
	printf("\n\n***************************** GIOCO DEL LOTTO *****************************\n"
		   "Sono disponibili i seguenti comandi: \n"
		   "1)!help <comando> --> mostra i dettagli di un comando\n"
		   "2)!signup <username> <password> --> crea un nuovo utente\n"
		   "3)!login <username> <password> --> autentica un utente\n"
		   "4)!invia_giocata g --> invia una giocata g al server\n"
		   "5)!vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1}\n"
		   "                         e permette di visualizzare le giocate passate ‘0’\n"
 		   "                         oppure le giocate attive ‘1’ (ancora non estratte)\n"
 		   "6)!vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni\n"
 		   "                                    sulla ruota specificata\n"
 		   "7)!esci --> termina il client\n\n"
		   );
}

//Gestore per il comando !help, fa distinzione se si specifica l'argomento o meno.
//Si tratta dell'unico comando che viene interamente gestito lato client.
void comando_help(char* argomento_help){
        //scorro e prendo il token successivo a "!help"
        argomento_help = strtok (NULL, " ");
        
        //faccio distinzione sull'argomento che segue help, se esiste stampo descrizione specifica altrimenti generica
        if(!argomento_help){
                printf("HELP: di seguito viene riportata la lista dei comandi disponibili:\n"
		        "1)!signup <username> <password> => registra un nuovo utente\n"
		        "2)!login <username> <password> => autentica un utente precedentemente registrato\n"
		        "3)!invia_giocata g => invia una giocata g al server (solo utente loggato)\n"
		        "4)!vedi_giocate tipo => tipo può valere 0 oppure 1\n"
		        "  se tipo vale 0 allora potrai visualizzare le giocate passate\n"
		        "  se tipo vale 1 allora potrai visualizzare le giocate in attesa di estrazione\n"
		        "  (solo utente loggato)\n"
		        "5)!vedi_estrazione <n> <ruota> => ti permette di visualizzare le n estrazioni più recenti\n"
		        "  se la ruota non è specificata le visualizza tutte, altrimenti solo la ruota selezionata\n"
		        "  (solo utente loggato)\n"
		        "6)!vedi_vincite => ti permette di visualizzare le tue vincite (solo utente loggato)\n"
		        "7)!esci => l'utente loggato viene disconnesso e il client viene chiuso\n"
		);
        }
        //stampo descrizione comando specifico
        else{
                if(strcmp(argomento_help,"signup\n") == 0) 
                        printf("**** !signup <username> <password> ****\n"
			"  registra un nuovo utente con username e password specificati\n"
			"  se è già presente un utente con lo stesso username, verrà chiesto di sceglierne un altro.\n");           
			           
	        else if(strcmp(argomento_help,"login\n") == 0)
	        	printf("**** !login <username> <password> ****\n"
	                "  autentica l'utente con username e password specificati (se corretti)\n"
			"  in caso vengano inseriti dati che non corrispondono ad alcun utente, il login fallirà.\n"
			"  L'utente potrà riprovare per altre 2 volte, esauriti i tentativi verrà bloccato per 30 min.\n");

	        else if(strcmp(argomento_help,"invia_giocata\n") == 0)
	                 printf("**** !invia_giocata g ****\n"
	                 "  se l'utente ha eseguito il login potrà inviare giocate al server\n"
			 "  la giocata ha la seguente formattazione:\n"
			 "  !invia_giocata -r bari roma . . -n 22 33 44 . . -i 0 1 2 3\n"
			 "  -r indica che a seguire ci saranno le ruote, posso specificare solo ruote esistenti\n"
			 "  -n indica che a seguire ci saranno i numeri puntati nel range [1,90]\n"
			 "  -i indica che a seguire ci saranno gli importi delle puntate, la prima per l'estratto etc\n"
			 "  la giocata verrà inserita nella scheda utente e sarà in attesa di estrazione.\n");
			 
	        else if(strcmp(argomento_help,"vedi_giocate\n") == 0) 
	                printf("**** !vedi_giocate <tipo> ****\n"
			"  se l'utente ha eseguito il login potrà visualizzare le giocate effettuate\n"
			"  se tipo=0 visualizzerà le giocate passate (già state estratte)\n"
			"  se tipo=1 visualizzerà le giocate in attesa di estrazione.\n");
			
	        else if(strcmp(argomento_help,"vedi_estrazione\n") == 0) 
	                printf("**** !vedi_estrazione <n> <ruota> ****\n"
			"  se l'utente ha eseguito il login potrà vedere le estrazioni più recenti\n"
			"  <n> va sempre specificato e indica quante estrazioni voglio visualizzare\n"
			"  se <ruota> non è specificata visualizzerò l'intera estrazione\n"
			"  altrimenti solo la riga dell'estrazione corrispondente alla ruota selezionata.\n");
			
	        else if(strcmp(argomento_help,"vedi_vincite\n") == 0) 
	                printf("**** !vedi_vincite ****\n"
			"  se l'utente ha eseguito il login potrà vedere il suo storico vincite.\n");
			
	        else if(strcmp(argomento_help,"esci\n") == 0) 
	                printf("**** !esci ****\n"
			"  l'utente attualmente loggato viene disconnesso e il client viene chiuso.\n");
	        
	        else
                        printf("ERRORE: argomento del comando help non riconosciuto.\n");
                
        }
}


//funzione che si limita a chiamare una inviaMessaggio, che a sua volta contiene le
//primitive send. viene richiamata dalla gestore_comando
void inviaComando(char* comando){
        char* comando_spezzato;
        inviaMessaggio(comando);
}

//funzione che stampa le risposte adeguate in base al contenuto di buffer, 
//ovvero dove viene salvato il msg di risposta inviato dal server
void gestore_risposte_ai_comandi(char *comando_spezzato){

        //LOGIN
        //se il login va a buon fine devo salvare il sessionID nella var globale corrispondente
        if(strcmp(comando_spezzato,"!login")==0 && strcmp(buffer,"SERVER: Login effettuato correttamente\n")==0){
                riceviMessaggio(buffer);
                strcpy(sessionID, buffer);
                utente_loggato = 1; //l'utente è stato loggato con successo, serve per vedere se inviare sessionID nei comandi
        	printf("CLIENT: SessionID ricevuto con successo %s \n", sessionID);
        }
        //in caso di 3 tentativi di login falliti il server invierà questo messaggio e devo chiudere la connessione
     	if(strcmp(buffer,"SERVER: Superato limite di tentativi login falliti, disconnessione...\n") == 0){ 
     		connesso = 0;
     	}
     	
     	//risposta al comando !esci
        if(strcmp(comando_spezzato,"!esci\n")==0 && strcmp(buffer,"SERVER: disconnessione avvenuta con successo.\n")==0){
     	    connesso=0;
     	}       
}

//funzione inserita nel while interno al main, e eseguito finchè connesso==1. Si occupa di gestire il comando ricevuto da tastiera
//tramite la fgets, smistando alle giuste funzioni di competenza. La help è l'unica che viene gestita localmente senza coinvolgere
//il server. gestisco localmente anche il controllo sul tentativo di effettuare certi comandi che necessitano di essere loggati.
int gestore_comando(char* comando){
        if(comando[0] != '!'){
                printf("ERRORE: il comando deve iniziare con !<comando>\n");
                return -1;
        }
        
        //mi salvo l'intera riga del comando perchè la strtok modifica comando
        char comando_originario[1024];
        strcpy(comando_originario, comando);
        
        //check_sintassi_comando(comando);
        
        char *comando_spezzato;
        comando_spezzato = strtok (comando," ");
        
        while (comando_spezzato != NULL){
            
            //corrisponde al comando help SENZA argomenti || corrisponde al comando help CON argomenti
            if(strcmp(comando_spezzato,"!help\n")==0 || strcmp(comando_spezzato,"!help")==0){ 
                comando_help(comando);
                return 0;
            }
             
            //se il comando NON è help, coinvolgo anche il server           
            else if(strcmp(comando_spezzato,"!signup")==0 ||
                    strcmp(comando_spezzato,"!login")==0 || 
                    strcmp(comando_spezzato,"!invia_giocata")==0 ||
                    strcmp(comando_spezzato,"!vedi_giocate")==0 ||
                    strcmp(comando_spezzato,"!vedi_estrazione")==0 ||
                    strcmp(comando_spezzato,"!vedi_vincite\n")==0 ||
                    strcmp(comando_spezzato,"!esci\n")==0){
                
                //ESCI
                if(strcmp(comando_spezzato,"!esci\n")==0 && utente_loggato==0){
                        printf("CLIENT: Errore, è necessario aver effettuato il login per potersi disconnettere! \n"); 
                        return -1;
                }
                if(strcmp(comando_spezzato,"!esci\n")==0){
                        strcat(comando_originario, " ");
                        strcat(comando_originario, sessionID);
                        inviaComando(comando_originario);
                        riceviMessaggio(buffer);
                        gestore_risposte_ai_comandi(comando_spezzato);
                        return 0;
                }
                
                
                //INVIA GIOCATA
                if(strcmp(comando_spezzato,"!invia_giocata")==0 && utente_loggato==0){
                        printf("CLIENT: Errore, è necessario aver effettuato il login per poter inviare una giocata! \n"); 
                        return -1;
                }
                //invio infondo al comando anche "#sessionID"
                if(strcmp(comando_spezzato,"!invia_giocata")==0){
                        strcat(comando_originario, " ");
                        strcat(comando_originario, "#");
                        strcat(comando_originario, sessionID);
                        inviaComando(comando_originario);
                        riceviMessaggio(buffer);
                        printf(buffer);
                        gestore_risposte_ai_comandi(comando_spezzato);
                        return 0;
                }
                
                
                //VEDI GIOCATE
                if(strcmp(comando_spezzato,"!vedi_giocate")==0 && utente_loggato==0){
                        printf("CLIENT: Errore, è necessario aver effettuato il login per poter visualizzare le giocate effettuate! \n"); 
                        return -1;
                }
                if(strcmp(comando_spezzato,"!vedi_giocate")==0){
                        strcat(comando_originario, " ");
                        strcat(comando_originario, "#");
                        strcat(comando_originario, sessionID);
                        inviaComando(comando_originario);
                        riceviMessaggio(buffer);
                        //se il primo ricevi è un messaggio di errore dal server invece che il numero di righe da stampare
                        if(strcmp(buffer,"SERVER: Errore sintassi comando !vedi_giocate\n")==0){
                                printf("SERVER: Errore sintassi comando !vedi_giocate\n");
                                return 0;
                        }
                        if(strcmp(buffer,"SERVER: Nessun risultato.\n")==0){
                                printf("SERVER: Nessun risultato.\n");
                                return 0;
                        }
                        int quante_volte_ricevi=atoi(buffer);
                        //printf("quante_volte_ricevi: %i\n", quante_volte_ricevi);
                        int i=0;
                        for(i=0; i < quante_volte_ricevi; i++){
                                riceviMessaggio(buffer);
                                printf("%i) ", i+1);
                                printf(buffer);
                                printf("\n");
                                strcpy(buffer, "");
                        }
                        gestore_risposte_ai_comandi(comando_spezzato);
                        return 0;
                }
                
                
                
                //VEDI ESTRAZIONE
                if(strcmp(comando_spezzato,"!vedi_estrazione")==0 && utente_loggato==0){
                        printf("CLIENT: Errore, è necessario aver effettuato il login per poter visualizzare le estrazioni! \n"); 
                        return -1;
                }
                if(strcmp(comando_spezzato,"!vedi_estrazione")==0){
                        strcat(comando_originario, " ");
                        strcat(comando_originario, "#");
                        strcat(comando_originario, sessionID);
                        inviaComando(comando_originario);
                        riceviMessaggio(buffer);
                        if(strcmp(buffer, "")!=0)
                                printf(buffer);
                        else
                                printf("SERVER: Spiacenti, non sono presenti estrazioni da mostrare.\n");
                        gestore_risposte_ai_comandi(comando_spezzato);
                        return 0;
                }
                
                
                //VEDI VINCITE
                if(strcmp(comando_spezzato,"!vedi_vincite\n")==0 && utente_loggato==0){
                        printf("CLIENT: Errore, è necessario aver effettuato il login per poter visualizzare le vincite! \n"); 
                        return -1;
                }
                if(strcmp(comando_spezzato,"!vedi_vincite\n")==0){
                        strcat(comando_originario, " ");
                        strcat(comando_originario, "#");
                        strcat(comando_originario, sessionID);
                        inviaComando(comando_originario);
                        riceviMessaggio(buffer);
                        if(strcmp(buffer, "")!=0)
                        printf(buffer);
                        gestore_risposte_ai_comandi(comando_spezzato);
                        return 0;
                }
                
                //LOGIN E SIGNUP RICADONO NELL'ELSE
                if(strcmp(comando_spezzato,"!login")==0 && utente_loggato==1){
                        printf("CLIENT: Errore, è necessario disconnettersi prima di poter effettuare nuovamente il login! \n"); 
                        return -1;
                }
                if(strcmp(comando_spezzato,"!signup")==0 && utente_loggato==1){
                        printf("CLIENT: Errore, è necessario disconnettersi prima di poter effettuare una registrazione! \n"); 
                        return -1;
                }
                else
                        inviaComando(comando_originario);
                  
                  
                        
                riceviMessaggio(buffer);
                printf(buffer);
                gestore_risposte_ai_comandi(comando_spezzato);
                
                return 0;
            }       
                        
            //caso in cui l'utente specifica un comando preceduto da ! ma non riconosciuto
            else{
                printf("ERRORE: comando non riconosciuto. Digitare !help per consultare i comandi disponibili\n");
                return -1;
            }
                        
            comando_spezzato = strtok (NULL, " ");
        }
        
}



//-------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]){
		
	//Sintassi avvio client: ./lotto_client <IP server> <porta server>
	//argc deve valere 3 con argv[1]=<IP server>  e argv[2]=<porta server>
	if(argc != 3) {
    	        printf("CLIENT: Errore! per poter avviare il client devi specificare i seguenti parametri <IP server> e <porta server>\n");
    	        exit(-1);
        }
    
	IPServer = argv[1]; //ip del server passato come argomento
        portaServer = atoi(argv[2]); //passaggio argomenti del main, richiesto cast ad int
    
	
	//Creazione socket client
        socket_client = socket(AF_INET, SOCK_STREAM, 0);
    
        //Creazione indirizzo server
	memset(&server_addr, 0, sizeof(server_addr)); 
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portaServer); 
	inet_pton(AF_INET, IPServer, &server_addr.sin_addr);
	
	//connect permette al socket locale client
	//di inviare una richiesta di connessione al socket del server.
	//E' bloccante finchè il server non fa una accept
	ret = connect(socket_client, (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        if(ret){
              	perror("CLIENT: Errore in fase di connessione \n");
                exit(-1);
        } 
        else 
                connesso = 1;
            
        //ricevo un messaggio che mi comunica se la connessione è andata a buon fine
        riceviMessaggio(buffer); 
        //stampo il msg ricevuto dal server
        printf("%s",buffer);
        //se la connessione è bloccata, non faccio neanche stampare il msg di benvenuto e faccio subito il kick-out
        if(strcmp(buffer, "SERVER: l'accesso è stato bloccato per aver superato il limite di tentativi, riprova più tardi\n")==0)
                connesso = 0;
        
        if(connesso==1)
                messaggioBenvenuto();
        
        while(connesso==1){
                //attendo text input da tastiera
                fgets(stringa_comando, 1024, stdin); 
                gestore_comando(stringa_comando);
        }
	
        printf("CLIENT: Sei stato disconnesso\n");
        close(socket_client);
        return 0;
}
