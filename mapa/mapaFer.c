#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mapa.h"


int main ( int argc , char * argv []) {
	t_log *log = log_create(PATH_LOG_MAP , PROGRAM_MAP , true , 3);

	if (argc != 3) {
		printf("Error en cantidad de parametros\n");
		return EXIT_FAILURE;
	}else{
		printf("Nombre del Mapa: \t%s\n", argv[1]);
		printf("Path Pokedex Cliente: \t%s\n\n", argv[2]);
	}
	char *path = calloc(1, strlen(argv[2])+strlen(MAP_METADAMA));
	strncpy(path, argv[2],strlen(argv[2]));
	strncpy(path + strlen(argv[2]), MAP_METADAMA ,strlen(MAP_METADAMA));
	struct confMap *configMap = calloc(1,sizeof(struct confMap));

	get_config_map(configMap, path);


	free(configMap);
	log_destroy(log);
	free(path);
	return EXIT_SUCCESS;
}

void get_config_map(struct confMap *configMap, char *path){
	t_config *mapConfig = config_create(path);
	configMap->time_check_deadlock = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	configMap->battle = config_get_int_value(mapConfig, "Batalla");
	configMap->algoritmo = config_get_string_value(mapConfig, "algoritmo");
	configMap->quantum = config_get_int_value(mapConfig, "quantum");
	configMap->ip = config_get_int_value(mapConfig, "IP");
	configMap->puerto = config_get_int_value(mapConfig, "Puerto");

	printf("\nTiempo de chaqueo DeadLock: %d",configMap->time_check_deadlock);
	printf("\nBatalla: %d - Algoritmo: %s - quantum: %d",configMap->battle, configMap->algoritmo, configMap->quantum);
	printf("\nIP Mapa: %s - Puerto: %s",configMap->ip, configMap->puerto);

	config_destroy(mapConfig);
}