#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "mapalib.h"
#include "entrenador.h"

int main ( int argc , char * argv []) {
	t_log *log = log_create(PATH_LOG_COACH , PROGRAM_COACH , true , 3);

	if (argc != 3) {
		printf("Error en cantidad de parametros\n");
		return EXIT_FAILURE;
	}
		printf("Nombre del Entrenador: \t%s\n", argv[1]);
		printf("Path Pokedex Cliente: \t%s\n\n", argv[2]);

	char *path = calloc(1, strlen(argv[2])+strlen(COACH_METADAMA));
	strncpy(path, argv[2],strlen(argv[2]));
	strncpy(path + strlen(argv[2]), COACH_METADAMA ,strlen(COACH_METADAMA));
	struct confCoach *configCoach = calloc(1,sizeof(struct confCoach));

	//LEVANTA METADATA DEL ENTRENADOR
	get_config(configCoach, path);

	//LEVANTA LA METADATA DEL MAPA
	tMapaMetadata *mapaMetadata = getMapaMetadata(configCoach->hojaDeViaje[0], argv[2]);

	// CONECTARSE AL MAPA
	/* Preparando la estructura */
	struct sockaddr_in direccionMapa;
	direccionMapa.sin_family = AF_INET;
	direccionMapa.sin_port = htons(mapaMetadata->puerto);   // Puerto extraido del archivo metadata
	inet_aton(mapaMetadata->ip, &(direccionMapa.sin_addr)); // IP extraida del archivo metadata
	memset(&(direccionMapa.sin_zero), '\0', 8);             // Pongo en 0 el resto de la estructura

		int entrenador = socket(AF_INET, SOCK_STREAM, 0);
			if (connect(entrenador, (void*) &direccionMapa, sizeof(direccionMapa)) == -1) {
				perror("No se pudo conectar");
				return EXIT_FAILURE;
			}
		printf("Entrenador conectado.\n");

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

	configCoach->hojaDeViaje = config_get_array_value(coachConfig, "hojaDeViaje");

	while(configCoach->hojaDeViaje[i] != NULL){
		if (configCoach->hojaDeViaje[i+1] == NULL){
			configCoach->hojaDeViaje[i][strlen(configCoach->hojaDeViaje[i])-1]='\0';
		}
		printf("\nHoja de Viaje[%d]: %s",i+1,configCoach->hojaDeViaje[i]);
		i++;
	}
	for (j = 0 ; j<i; j++){
		char *mapa = malloc(128);
		strcpy(mapa, "obj[");
		strcpy(mapa+4,configCoach->hojaDeViaje[j]);
		strcpy(mapa+strlen(mapa), "]");
		printf("\n%s:",mapa);
		configCoach->objetivos = config_get_array_value(coachConfig, mapa);
		int x = 0;
		while(configCoach->objetivos[x]){
			if (configCoach->objetivos[x+1] == NULL){
				configCoach->objetivos[x][strlen(configCoach->objetivos[x])-1]='\0';
			}
			printf("%s - ",configCoach->objetivos[x]);
			x++;
		}
		free(mapa);
	}
	configCoach->vidas = config_get_int_value(coachConfig, "vidas");
	configCoach->reintentos = config_get_int_value(coachConfig, "reintentos");
	printf("\nVidas: %d - Reintentos: %d\n\n",configCoach->vidas, configCoach->reintentos);

}


