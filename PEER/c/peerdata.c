#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

//variabile globale contente il numero di porta del peer
int peer_id;
//variabile globale con l'indirizzo ip del peer
char* peer_ip="127.0.0.1";
//numero di porta e indirizzo ip del server a cui il peer si connetterà
int server_port;
char* server_ip="127.0.0.1";
//socket tcp usato per la comunicazione con il server
int server_sock;
//mantengo memorizzati i possibili comandi
const char* comandi[4]={"start", "add", "get", "stop"};
//descrittore e indirizzo del socket attualmente utilizzato dal peer
int peer_sock;
struct sockaddr_in peer_addr;
//numeri di porta dei vicini, se corrispondo a -1 non c'è il vicino
int neigh[2];
//la funzione start può essere chiamata (con successo) una sola volta dal boot
int start_called=0;
//registro della giornata attuale, caratterizzato da giorno, mese, anno, totale tamponi, totale casi(entrambi calcolati dalle entry) e la lista delle entry aggiunte
//mantiene anche il puntatore alla prossimo daily register e un intero che indica se il daily register è chiuso o aperto
struct daily_register{
	int day;
	int month;
	int year;
	int tot_tamponi;
	int tot_casi;
	int closed;
	struct daily_register* next_register;
};
//lista dei daily registers
struct daily_register* registers=NULL;
//dato aggregato aggrt calcolato tra la data 1 e la data 2, di tipo type 
struct data_aggr{
	int day1;
	int month1;
	int year1;
	int day2;
	int month2;
	int year2;
	char aggrt;
	char type;
	int data;
	struct data_aggr* next_aggr;
};
//lista dei dai aggregati disponibili sul peer
struct data_aggr* aggr=NULL;
int fdmax; // Numero max di descrittori
fd_set master; /* Set principale, deve essere condiviso dato che in certe funzioni è possibile definire dei set più piccoli*/
//lunghezza massima dei messaggi, tutti avranno questa lunghezza anche se in certi casi la lunghezza effettiva potrebbe essere minore
#define CODE_LEN 6


//funzione per la creazione di un nuovo registro
struct daily_register* create_register(int day, int month, int year, int tot_tamponi, int tot_casi, int closed){
	//creazione e inizializzazione del daily register
	struct daily_register* new_register=malloc(sizeof(struct daily_register));
	struct daily_register* appoggio=registers;
	struct daily_register* register_prec=NULL;
	new_register->day=day;
	new_register->month=month;
	new_register->year=year;	
	new_register->tot_tamponi=tot_tamponi;
	new_register->tot_casi=tot_casi;
	new_register->closed=closed;
	new_register->next_register=NULL;
	//se il peer non ha registri, lo metto in testa
	if(registers==NULL){
		registers=new_register;
		return new_register;
	}
	//se il peer ha registri, cerco la posizione in cui metterlo
	while(appoggio!=NULL){
		//i registri sono ordinati in ordine cronologico
		if((year<appoggio->year)||(year==appoggio->year && month<appoggio->month)||(year==appoggio->year && month==appoggio->month && day<appoggio->day)){
			if(register_prec==NULL){
				registers=new_register;
			}
			else{
				register_prec->next_register=new_register;
			}
			new_register->next_register=appoggio;
			return new_register;
		}
		register_prec=appoggio;
		appoggio=appoggio->next_register;	
	}
	//se il registro deve essere inserito in coda alla lista dei registri
	register_prec->next_register=new_register;
	new_register->next_register=NULL;
	return new_register;
}

//funzione per la creazione di un nuovo dato aggregato
struct data_aggr* create_aggr(int day1, int month1, int year1,int day2, int month2, int year2, char aggrt, char type, int data){
	//creazione e inizializzazione della struttura contenente il dato aggregato
	struct data_aggr* new_aggr=malloc(sizeof(struct data_aggr));
	struct data_aggr* appoggio=aggr;
	struct data_aggr* aggr_prec=NULL;
	new_aggr->day1=day1;
	new_aggr->month1=month1;
	new_aggr->year1=year1;	
	new_aggr->day2=day2;
	new_aggr->month2=month2;
	new_aggr->year2=year2;
	new_aggr->aggrt=aggrt;
	new_aggr->type=type;
	new_aggr->data=data;
	new_aggr->next_aggr=NULL;
	//se il peer non ha dati aggregati, lo metto in testa
	if(aggr==NULL){
		aggr=new_aggr;
		return new_aggr;
	}
	//se il peer ha dati aggregati, cerco la posizione in cui metterlo
	while(appoggio!=NULL){
		//ordinati in ordine cronologico, se le prima date coincidono allora le seconde date devono essere in ordine cronologico
		if(((year1<appoggio->year1)||(year1==appoggio->year1 && month1<appoggio->month1)||(year1==appoggio->year1 && month1==appoggio->month1 && day1<appoggio->day1)) || (year1==appoggio->year1 && month1==appoggio->month1 && day1==appoggio->day1 && ((year2<appoggio->year2)||(year2==appoggio->year2 && month2<appoggio->month2)||(year2==appoggio->year2 && month2==appoggio->month2 && (day2<appoggio->day2 || day2==appoggio->day2))))) {
			if(aggr_prec==NULL){
				aggr=new_aggr;
			}
			else{
				aggr_prec->next_aggr=new_aggr;
			}
			new_aggr->next_aggr=appoggio;
			return new_aggr;
		}
		aggr_prec=appoggio;
		appoggio=appoggio->next_aggr;	
	}
	aggr_prec->next_aggr=new_aggr;
	new_aggr->next_aggr=NULL;
	return new_aggr;
}

//funzione per la creazione del registro giornaliero
struct daily_register* create_daily_register(int day, int month, int year){
	//creazione e inizializzazione del daily register
	struct daily_register* new_register=malloc(sizeof(struct daily_register));
	struct daily_register* appoggio=registers;
	new_register->day=day;
	new_register->month=month;
	new_register->year=year;	
	new_register->tot_tamponi=0;
	new_register->tot_casi=0;
	new_register->closed=0;
	new_register->next_register=NULL;
	//se il peer non ha registri, lo metto in testa
	if(registers==NULL){
		registers=new_register;
		return new_register;
	}
	//se il peer ha registri, lo metto in coda essendo il regitro giornaliero
	while(appoggio->next_register!=NULL)
		appoggio=appoggio->next_register;
	appoggio->next_register=new_register;
	return new_register;
}


//funzione per trovare il registro del giorno day/month/year
struct daily_register* find_daily_register(int day, int month, int year){
	struct daily_register* appoggio=registers;
	//controllo se è presente il daily register
	while(appoggio!=NULL){
		if(appoggio->day==day && (appoggio->month)==month && appoggio->year==year){
			break;
		}
		else
			appoggio=appoggio->next_register;
	}
	return appoggio;
}

//funzione per trovare se è presente il dato aggregato di tipo 'aggrt' sulle entry di tipo 'type' calcolato dal giorno day1/month1/year1 al giorno day2/month2/year2 
struct data_aggr* find_aggr(int day1, int month1, int year1, int day2, int month2, int year2, char aggrt, char type){
	struct data_aggr* appoggio=aggr;
	//controllo se è presente il daily register
	while(appoggio!=NULL){
		if(appoggio->day1==day1 && (appoggio->month1)==month1 && appoggio->year1==year1 && appoggio->day2==day2 && (appoggio->month2)==month2 && appoggio->year2==year2 && appoggio->type==type && appoggio->aggrt==aggrt){
			break;
		}
		else
			appoggio=appoggio->next_aggr;
	}
	return appoggio;
}

//funzione che ritorna il numero di registri presenti
int count_registers(){
	struct daily_register* appoggio=registers;
	int tot=0;
	while(appoggio!=NULL){
		tot++;
		appoggio=appoggio->next_register;
	}
	appoggio=registers;
	return tot;
}

//funzione per ottenere il numero di giorno precedente a quello specificato
int get_prev_day(int day, int month, int year){
	if(day==1){
		if(month==12 || month==5 || month==7 || month==10)
			return 30;
		if(month==3){
			//Controlla s el'anno è bisestile
			if(year%4==0){
				if(year%100!=0)
					return 29;
				else{
					if(year%400==0)
						return 29;
					else
						return 28;
				}
			}
			else
				return 28;
		}
	}
	return day-1;
}

//funzione per ottenere il numero di giorno successivo a quello specificato
int get_next_day(int day, int month, int year){
	if(day==30)
		if(month==11 || month==4 || month==6 || month==9)
			return 1;
	if(day==28){
		if(month==2){
			//Controlla se l'anno è bisestile
			if(year%4==0){
				if(year%100!=0)
					return 29;
				else{
					if(year%400==0)
						return 29;
					else
						return 1;
				}
			}
			else
				return 1;
		}
	}
	if(day==29){
		if(month==2){
			return 1;
		}
	}
	if(day==31)
		return 1;
	return day+1;
}

//ritorna il numero di mese del giorno nextday a quello specificato
int get_next_month(int nextday, int month){
	if(month==12 && nextday==1)
		return 1;
	else
		if(nextday==1)
			return month+1;
		else
			return month;
}

//ritorna il numero di anno del giorno successivo a quello specificato
int get_next_year(int nextday, int nextmonth, int year){
	if(nextday==1 && nextmonth==1)
		return year+1;
	else
		return year;
}

//ritorna il numero di mese del giorno precedente a quello specificato
int get_prev_month(int day, int month){
	if(month==1 && day==1)
		return 12;
	else
		if(day==1)
			return month-1;
		else
			return month;
}

//ritorna l'anno del giorno precedente a quello specificato
int get_prev_year(int day, int month, int year){
	if(month==1 && day==1)
		return year-1;
	else
		return year;
}

//funzione che ritorna il numero di dati aggregati presenti
int count_aggr(){
	struct data_aggr* appoggio=aggr;
	int tot=0;
	while(appoggio!=NULL){
		tot++;
		appoggio=appoggio->next_aggr;
	}
	return tot;
}