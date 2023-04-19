#include <stdlib.h>
#include <stdio.h>


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

void crea_dati_peer(int day, int month, int year, int peer_id){
	int tot_tamponi, tot_casi, j, k=0, z=0,  count=0, no_cont=0, startday, startmonth, startyear, tot_aggr=0;
	int aggr_tamponi=0, aggr_casi=0, tamponi_prec, casi_prec;
	char filename[20], app=' ';
	FILE* file_saved;
	FILE * aggr_saved;
	startday=day, startmonth=month, startyear=year;
	for(j=0; !(day==1 && month==1 && year==2021); j++){
		k++;
		if(no_cont==0){
			count++;
			printf("%d/%d/%d\n", day, month, year);
			if(k>=2)
				tot_aggr+=1;
		}
		if(k==5 && z==0){
			k=0;
			tot_aggr+=2;
			no_cont=(no_cont==0)?1:0;
			z=1;
		}
		else{
			if(k==20 && z==1){
				k=0;
				no_cont=(no_cont==0)?1:0;
				z=0;
			}
		}
		day=get_next_day(day, month, year);
		month=get_next_month(day, month);
		year=get_next_year(day, month, year);
	}
	//salvataggio di registri su file
	sprintf(filename, "%s%d", "peer", peer_id);
	//apertura del file il scrittura
	file_saved=fopen(filename, "w");
	sprintf(filename, "%s%d", "aggr", peer_id);
	//apertura del file il scrittura
	aggr_saved=fopen(filename, "w");
	fprintf(aggr_saved, "%d%c", tot_aggr , ' ');
	fprintf(file_saved, "%d%c", count , ' ');
	k=0, no_cont=0, z=0;
	day=startday, month=startmonth, year=startyear;
	for(j=0; !(day==1 && month==1 && year==2021); j++){
		if(k==0)
			startday=day, startmonth=month, startyear=year;
		k++;
		tot_tamponi = rand() % 100 + 1;
		tot_casi = rand() % 100 + 1;
		if(no_cont==0){
			aggr_tamponi+=tot_tamponi;
			aggr_casi+=tot_casi;
			fprintf(file_saved,"%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, 1, app);
			if(k>=2){
				if(k%2==0)
					fprintf(aggr_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", get_prev_day(day, month, year), ' ', get_prev_month(day, month),' ', get_prev_year(day, month, year), ' ', day, ' ', month, ' ', year, 'V', 1, 'T', tot_tamponi-tamponi_prec, ' ');
				else
					fprintf(aggr_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", get_prev_day(day, month, year), ' ', get_prev_month(day, month),' ', get_prev_year(day, month, year), ' ', day, ' ', month, ' ', year, 'V', 1, 'N', tot_casi-casi_prec, ' ');
			}
			tamponi_prec=tot_tamponi;
			casi_prec=tot_casi;
		}
		if(k==5 && z==0){
			k=0;
			no_cont=(no_cont==0)?1:0;
			z=1;
			fprintf(aggr_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", startday, ' ', startmonth,' ',startyear, ' ', day, ' ', month, ' ', year, 'T', 1, 'T', aggr_tamponi, ' ');
			fprintf(aggr_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", startday, ' ', startmonth,' ',startyear, ' ', day, ' ', month, ' ', year, 'T', 1, 'N', aggr_casi, ' ');
			aggr_tamponi=0;
			aggr_casi=0;
		}
		else{
			if(k==20 && z==1){
				k=0;
				no_cont=(no_cont==0)?1:0;
				z=0;
			}
		}
		day=get_next_day(day, month, year);
		month=get_next_month(day, month);
		year=get_next_year(day, month, year);
	}
	fclose(file_saved);
	fclose(aggr_saved);
}

int main (){
	crea_dati_peer(1,2,2020,5001);
	crea_dati_peer(6,2,2020, 5002);
	crea_dati_peer(11,2,2020, 5003);
	crea_dati_peer(16,2,2020, 5004);
	crea_dati_peer(21,2,2020, 5005);
	return 1;
}