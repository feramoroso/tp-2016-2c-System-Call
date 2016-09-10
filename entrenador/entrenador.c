#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "entrenador.h"


int main ( int argc , char * argv []) {
	t_log *log = log_create(PATH_LOG_COACH , PROGRAM_COACH , true , 3);

	if (argc != 3) {
		printf("Error en cantidad de parametros\n");
		return EXIT_FAILURE;
	}else{
		printf("Nombre del Entrenador: \t%s\n", argv[1]);
		printf("Path Pokedex Cliente: \t%s\n\n", argv[2]);
	}
	char *path = calloc(1, strlen(argv[2])+strlen(COACH_METADAMA));
	strncpy(path, argv[2],strlen(argv[2]));
	strncpy(path + strlen(argv[2]), COACH_METADAMA ,strlen(COACH_METADAMA));
	struct confCoach *configCoach = calloc(1,sizeof(struct confCoach));

	get_config(configCoach, path);


	free(configCoach);
	log_destroy(log);
	free(path);
	return EXIT_SUCCESS;
}

void get_config(struct confCoach *configCoach, char *path){
	int i = 0;
	int j = 0;
	t_config *coachConfig = config_create(path);
	configCoach->nombre = config_get_string_value(coachConfig, "nombre");
	configCoach->simbolo = config_get_string_value(coachConfig, "simbolo")[0];

	configCoach->nombre[strlen(configCoach->nombre)-1]='\0';
	printf("\nNombre: %s [%c]",configCoach->nombre, configCoach->simbolo);

	configCoach->hojadeviaje = config_get_array_value(coachConfig, "hojaDeViaje");
//nada
	while(configCoach->hojadeviaje[i] != NULL){
		if (configCoach->hojadeviaje[i+1] == NULL){
			configCoach->hojadeviaje[i][strlen(configCoach->hojadeviaje[i])-1]='\0';
		}
		printf("\nHoja de Viaje[%d]: %s",i+1,configCoach->hojadeviaje[i]);
		i++;
	}
	configCoach->objetivos = list_create();
	for (j = 0 ; j<i; j++){
		char *mapa = calloc(1,sizeof(char*));
		strcpy(mapa, "obj[");
		strcpy(mapa+4,configCoach->hojadeviaje[j]);
		strcpy(mapa+strlen(mapa), "]");
		printf("\n%s:",mapa);
		char **obj = config_get_array_value(coachConfig, mapa);
		int x = 0;
		while(obj[x] != NULL){
			if (obj[x+1] == NULL){
				obj[x][strlen(obj[x])-1]='\0';
			}
			printf("%s - ",obj[x]);
			x++;
		//list_add(configCoach->objetivos,);
		}
		free(mapa);
	}
	configCoach->vidas = config_get_int_value(coachConfig, "vidas");
	configCoach->reintentos = config_get_int_value(coachConfig, "reintentos");
	printf("\nVidas: %d - Reintentos: %d\n\n",configCoach->vidas, configCoach->reintentos);

	config_destroy(coachConfig);
}
