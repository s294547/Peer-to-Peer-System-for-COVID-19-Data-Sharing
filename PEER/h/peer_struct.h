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