/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"

/* Recibe el nombre del Mapa y la Ruta del PokeDex y retorna una estructura con la metadata del mapa */
tMapaMetadata *getMapaMetadata(char *nomMapa, char *rutaPokeDex) {

	short int largoRuta = strlen(COMPLEMENTO_RUTA_MAPA)+strlen(rutaPokeDex)+strlen(nomMapa);
	char ruta[largoRuta+1];
	sprintf(ruta, COMPLEMENTO_RUTA_MAPA, rutaPokeDex, nomMapa);
	printf("\nRuta Mapa:\n%s\n", ruta);

	tMapaMetadata *mapaMetadata = calloc(1,sizeof(tMapaMetadata));
	t_config *mapConfig = config_create(ruta);
	mapaMetadata->tiempoDeadlock = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla        = config_get_int_value(mapConfig, "Batalla");
	mapaMetadata->algoritmo = (char*) malloc(10);
	strcpy(mapaMetadata->algoritmo, config_get_string_value(mapConfig, "algoritmo"));
	//mapaMetadata->algoritmo = config_get_string_value(mapConfig, "algoritmo");
	mapaMetadata->quantum        = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo        = config_get_int_value(mapConfig, "retardo");
	mapaMetadata->ip = (char*) malloc(16);
	strcpy(mapaMetadata->ip, config_get_string_value(mapConfig, "IP"));
	//mapaMetadata->ip             = config_get_string_value(mapConfig, "IP");
	mapaMetadata->puerto         = config_get_int_value(mapConfig, "Puerto");
	config_destroy(mapConfig);

	printf("\nNombre del Mapa: \t%s", nomMapa);
	printf("\nRuta PokeDex Cliente: \t%s\n", rutaPokeDex);
	printf("\nTiempo de chequeo DeadLock: \t%d", mapaMetadata->tiempoDeadlock);
	printf("\nBatalla: \t%d",   mapaMetadata->batalla);
	printf("\nAlgoritmo: \t%s", mapaMetadata->algoritmo);
	printf("\nQuantum: \t%d",   mapaMetadata->quantum);
	printf("\nIP Mapa: \t%s",   mapaMetadata->ip);
	printf("\nPuerto: \t%d\n\n",mapaMetadata->puerto);

	return mapaMetadata;
}
