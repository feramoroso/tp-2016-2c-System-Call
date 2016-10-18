/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

/* PKMN BATTLE */
#include <pkmn/battle.h>
#include <pkmn/factory.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"

/* Recibe el nombre del Mapa y la Ruta del PokeDex y retorna una estructura con la metadata del mapa */
int getMapaMetadata(tMapaMetadata *mapaMetadata, char *nomMapa, char *rutaPokeDex) {
	//printf("\nRuta PokeDex Cliente: \t%s\n", rutaPokeDex);
	t_config *mapConfig = config_create(string_from_format("%s/Mapas/%s/metadata", rutaPokeDex, mapaMetadata->nombre));
	if (mapConfig == NULL) return EXIT_FAILURE; // En caso de error devuelvo NULL
	//printf("\nRuta Mapa:\n%s", ruta);
	mapaMetadata->tiempoDeadlock  = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla         = config_get_int_value(mapConfig, "Batalla");
	mapaMetadata->algoritmo       = strdup( config_get_string_value(mapConfig, "algoritmo") );
	mapaMetadata->quantum         = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo         = config_get_int_value(mapConfig, "retardo");
	mapaMetadata->ip              = strdup( config_get_string_value(mapConfig, "IP") );
	mapaMetadata->puerto          = config_get_int_value(mapConfig, "Puerto");
	config_destroy(mapConfig);
	return EXIT_SUCCESS;
}
void imprimirInfoMapa(tMapaMetadata *mapaMetadata) {
	printf("\nNombre del Mapa:                 %s", mapaMetadata->nombre);
	printf("\nRuta de la Medalla               %s", mapaMetadata->medalla);
	printf("\nTiempo de chequeo DeadLock (ms): %d", mapaMetadata->tiempoDeadlock);
	printf("\nBatalla:                         %d", mapaMetadata->batalla);
	printf("\nAlgoritmo:                       %s", mapaMetadata->algoritmo);
	printf("\nQuantum:                         %d", mapaMetadata->quantum);
	printf("\nRetardo entre Quantums (ms):     %d", mapaMetadata->retardo);
	printf("\nIP Mapa:                         %s", mapaMetadata->ip);
	printf("\nPuerto:                          %d\n\n",mapaMetadata->puerto);
}
/* Recibe el nombre del mapa, el nombre del PokeNest y la Ruta del PokeDex
 *  y retorna una estructura con la metadata del  */
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex) {
	tPokeNestMetadata *pokeNestMetadata = malloc(sizeof(tPokeNestMetadata));
	pokeNestMetadata->nombre = strdup( nomPokeNest );
	t_config *pokeNestConfig = config_create(string_from_format("%s/Mapas/%s/PokeNests/%s/metadata", rutaPokeDex, nomMapa, nomPokeNest));
	if (pokeNestConfig == NULL) return NULL; //Chequeo Errores
	pokeNestMetadata->tipo = strdup( config_get_string_value(pokeNestConfig, "Tipo") );
	pokeNestMetadata->posx = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[0]);
	pokeNestMetadata->posy = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[1]);
	pokeNestMetadata->id    = config_get_string_value(pokeNestConfig, "Identificador")[0];
	config_destroy(pokeNestConfig);
	return pokeNestMetadata;
}
/** Recibe el nombre de la PokeNest, el numero de orden y la Ruta del **
 ** PokeNest y retorna una estructura con la metadata del Pokemon *****/
tPokemonMetadata *getPokemonMetadata(char *nomPokeNest, char id, int ord, char *rutaPokeNest) {
	t_pkmn_factory* pokemon_factory = create_pkmn_factory();
	tPokemonMetadata *pokemonMetadata = malloc(sizeof(tPokemonMetadata));
	t_config *pokemonConfig = config_create(string_from_format("%s/%s%03d.dat", rutaPokeNest, nomPokeNest, ord));
	if (pokemonConfig == NULL) return NULL; //Chequeo Errores
	pokemonMetadata->data = create_pokemon(pokemon_factory, nomPokeNest, config_get_int_value(pokemonConfig, "Nivel"));
	pokemonMetadata->id   = id;
	pokemonMetadata->ord  = ord;
	config_destroy(pokemonConfig);
	destroy_pkmn_factory(pokemon_factory);
	return pokemonMetadata;
}
int getPokeNestArray(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex) {
	int i = 0;
	DIR *dir;
	struct dirent *dirPokeNest;
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/PokeNests/", rutaPokeDex, nomMapa);
	dir = opendir(ruta);
	while ((dirPokeNest = readdir(dir))) {
		if ( (dirPokeNest->d_type == DT_DIR) && (strcmp(dirPokeNest->d_name, ".")) && (strcmp(dirPokeNest->d_name, "..")) ) {
			if ( (pokeNestArray[i] = getPokeNestMetadata(nomMapa, dirPokeNest->d_name, rutaPokeDex)) == NULL) {
				closedir(dir);
				return EXIT_FAILURE;
			}
			i++;
		}
	}
	pokeNestArray[i] = NULL;
	closedir(dir);
	return EXIT_SUCCESS;
}
int getPokemonsQueue(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex) {
	int i = 0, j;
	tPokemonMetadata *pokemonMetadata;
	while (pokeNestArray[i]) {
		pokeNestArray[i]->pokemons = queue_create();
		char ruta[256];
		sprintf(ruta, "%s/Mapas/%s/PokeNests/%s", rutaPokeDex, nomMapa, pokeNestArray[i]->nombre);
		j = 1;
		while ((pokemonMetadata = getPokemonMetadata(pokeNestArray[i]->nombre, pokeNestArray[i]->id, j, ruta))) {
			queue_push(pokeNestArray[i]->pokemons, pokemonMetadata);
			j++;
		}
		i++;
	}
	return EXIT_SUCCESS;
}
void imprimirInfoPokeNest(tPokeNestMetadata *pokeNestArray[]) {
	int i = 0;
	while(pokeNestArray[i]) {
		printf("\nPokeNest:         %s", pokeNestArray[i]->nombre);
		printf("\nTipo:             %s", pokeNestArray[i]->tipo);
		printf("\nPosición en x:    %d", pokeNestArray[i]->posx);
		printf("\nPosición en y:    %d", pokeNestArray[i]->posy);
		printf("\nIdentificador:    %c\n", pokeNestArray[i]->id);
		if(!queue_is_empty(pokeNestArray[i]->pokemons)) {
			printf("\nInstancias:        %d", queue_size(pokeNestArray[i]->pokemons));
			printf("\nNombre 1º en cola: %s", (*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).data->species);
			printf("\nTipo 1º en cola:   %s", pkmn_type_to_string((*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).data->type));
			printf("\nTipo 1º en cola:   %s", pkmn_type_to_string((*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).data->second_type));
			printf("\nNivel 1º en cola:  %d\n\n\n", (*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).data->level);
		}
		i++;
	}
}
