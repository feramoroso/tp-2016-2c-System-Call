/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"


/* Recibe el nombre del Mapa y la Ruta del PokeDex y retorna una estructura con la metadata del mapa */
tMapaMetadata *getMapaMetadata(char *nomMapa, char *rutaPokeDex) {

	printf("\nRuta PokeDex Cliente: \t%s\n", rutaPokeDex);
	tMapaMetadata *mapaMetadata = malloc(sizeof(tMapaMetadata));
	strcpy(mapaMetadata->nombre, nomMapa);
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/metadata", rutaPokeDex, mapaMetadata->nombre);
	printf("\nRuta Mapa:\n%s", ruta);


	t_config *mapConfig = config_create(ruta);
	if (mapConfig == NULL) return NULL; // En caso de error devuelvo NULL
	mapaMetadata->tiempoDeadlock = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla        = config_get_int_value(mapConfig, "Batalla");
	strcpy(mapaMetadata->algoritmo, config_get_string_value(mapConfig, "algoritmo"));
	mapaMetadata->quantum        = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo        = config_get_int_value(mapConfig, "retardo");
	strcpy(mapaMetadata->ip, config_get_string_value(mapConfig, "IP"));
	mapaMetadata->puerto         = config_get_int_value(mapConfig, "Puerto");
	config_destroy(mapConfig);

	printf("\nNombre del Mapa:            %s", mapaMetadata->nombre);
	printf("\nTiempo de chequeo DeadLock: %d", mapaMetadata->tiempoDeadlock);
	printf("\nBatalla:                    %d", mapaMetadata->batalla);
	printf("\nAlgoritmo:                  %s", mapaMetadata->algoritmo);
	printf("\nQuantum:                    %d", mapaMetadata->quantum);
	printf("\nIP Mapa:                    %s", mapaMetadata->ip);
	printf("\nPuerto:                     %d\n\n",mapaMetadata->puerto);

	return mapaMetadata;
}

/* Recibe el nombre del mapa, el nombre del PokeNest y la Ruta del PokeDex
 *  y retorna una estructura con la metadata del  */
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex) {

	tPokeNestMetadata *pokeNestMetadata = malloc(sizeof(tPokeNestMetadata));
	strcpy(pokeNestMetadata->nombre, nomPokeNest);

	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/PokeNests/%s/metadata", rutaPokeDex, nomMapa, nomPokeNest);
	t_config *pokeNestConfig = config_create(ruta);
	if (pokeNestConfig == NULL) return NULL; //Chequeo Errores
	printf("\nRuta PokeNest:\n%s", ruta);
	strcpy(pokeNestMetadata->tipo, config_get_string_value(pokeNestConfig, "Tipo"));
	pokeNestMetadata->posx = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[0]);
	pokeNestMetadata->posy = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[1]);
	pokeNestMetadata->ID    = config_get_string_value(pokeNestConfig, "Identificador")[0];
	config_destroy(pokeNestConfig);
	return pokeNestMetadata;
}

/* Recibe el nombre del mapa, el nombre del Pokemon, su numero de orden y la Ruta del PokeDex
 *  y retorna una estructura con la metadata del Pokemon ***********************************/

tPokemonMetadata *getPokemonMetadata(char *nomMapa, char * nomPokemon, int ord, char *rutaPokeDex) {

	tPokemonMetadata *pokemonMetadata = malloc(sizeof(tPokemonMetadata));
	strcpy(pokemonMetadata->nombre, nomPokemon);

	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/PokeNests/%s/%s%03d.dat", rutaPokeDex, nomMapa, nomPokemon, nomPokemon, ord);
	printf("\nRuta Pokemon:\n%s", ruta);

	t_config *pokemonConfig = config_create(ruta);
	if (pokemonConfig == NULL) return NULL; //Chequeo Errores
	pokemonMetadata->nivel = config_get_int_value(pokemonConfig, "Nivel");
	config_destroy(pokemonConfig);

	printf("\nNombre del Pokemon: %s", nomPokemon);
	printf("\nNivel Pokemon:      %d\n\n\n\n\n", pokemonMetadata->nivel);

	return pokemonMetadata;
}

