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
#include <commons/collections/queue.h>

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

/* Libreria con funciones del Mapa */
#include "mapalib.h"


/* Recibe el nombre del Mapa y la Ruta del PokeDex y retorna una estructura con la metadata del mapa */
tMapaMetadata *getMapaMetadata(char *nomMapa, char *rutaPokeDex) {

	printf("\nRuta PokeDex Cliente: \t%s\n", rutaPokeDex);
	tMapaMetadata *mapaMetadata = malloc(sizeof(tMapaMetadata));
	strcpy(mapaMetadata->nombre, nomMapa);
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/metadata", rutaPokeDex, mapaMetadata->nombre);

	t_config *mapConfig = config_create(ruta);
	if (mapConfig == NULL) return NULL; // En caso de error devuelvo NULL
	printf("\nRuta Mapa:\n%s", ruta);
	mapaMetadata->tiempoDeadlock  = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla         = config_get_int_value(mapConfig, "Batalla");
	strcpy(mapaMetadata->algoritmo, config_get_string_value(mapConfig, "algoritmo"));
	mapaMetadata->quantum         = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo         = config_get_int_value(mapConfig, "retardo");
	strcpy(mapaMetadata->ip,        config_get_string_value(mapConfig, "IP"));
	mapaMetadata->puerto          = config_get_int_value(mapConfig, "Puerto");
	config_destroy(mapConfig);

	printf("\nNombre del Mapa:                 %s", mapaMetadata->nombre);
	printf("\nTiempo de chequeo DeadLock (ms): %d", mapaMetadata->tiempoDeadlock);
	printf("\nBatalla:                         %d", mapaMetadata->batalla);
	printf("\nAlgoritmo:                       %s", mapaMetadata->algoritmo);
	printf("\nQuantum:                         %d", mapaMetadata->quantum);
	printf("\nRetardo entre Quantums (ms):     %d", mapaMetadata->retardo);
	printf("\nIP Mapa:                         %s", mapaMetadata->ip);
	printf("\nPuerto:                          %d\n\n",mapaMetadata->puerto);

	return mapaMetadata;
}

/* Recibe el nombre del mapa, el nombre del PokeNest y la Ruta del PokeDex
 *  y retorna una estructura con la metadata del  */
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex) {
	tPokeNestMetadata *pokeNestMetadata = malloc(sizeof(tPokeNestMetadata));
	strcpy(pokeNestMetadata->nombre, nomPokeNest);
	t_config *pokeNestConfig = config_create(string_from_format("%s/Mapas/%s/PokeNests/%s/metadata", rutaPokeDex, nomMapa, nomPokeNest));
	if (pokeNestConfig == NULL) return NULL; //Chequeo Errores
	strcpy(pokeNestMetadata->tipo, config_get_string_value(pokeNestConfig, "Tipo"));
	pokeNestMetadata->posx = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[0]);
	pokeNestMetadata->posy = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[1]);
	pokeNestMetadata->id    = config_get_string_value(pokeNestConfig, "Identificador")[0];
	config_destroy(pokeNestConfig);
	return pokeNestMetadata;
}

/** Recibe el nombre de la PokeNest, el numero de orden y la Ruta del **
 ** PokeNest y retorna una estructura con la metadata del Pokemon *****/
tPokemonMetadata *getPokemonMetadata(char * nomPokeNest, int ord, char *rutaPokeNest) {
	tPokemonMetadata *pokemonMetadata = malloc(sizeof(tPokemonMetadata));
	t_config *pokemonConfig = config_create(string_from_format("%s/%s%03d.dat", rutaPokeNest, nomPokeNest, ord));
	if (pokemonConfig == NULL) return NULL; //Chequeo Errores
	pokemonMetadata->id = nomPokeNest[0];
	pokemonMetadata->nivel = config_get_int_value(pokemonConfig, "Nivel");
	config_destroy(pokemonConfig);
	return pokemonMetadata;
}
/* Recibe el nombre del mapa, el nombre del Pokemon, su numero de orden y la Ruta del PokeDex
 *  y retorna una estructura con la metadata del Pokemon ***********************************/
/*tPokemonMetadata *getPokemonMetadata(char *nomMapa, char * nomPokemon, int ord, char *rutaPokeDex) {

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
*/

void imprimirInfoPokeNest(tPokeNestMetadata *pokeNestArray[]) {
	int i = 0;
	while(pokeNestArray[i]) {
		printf("\nPokeNest:         %s", pokeNestArray[i]->nombre);
		printf("\nTipo:             %s", pokeNestArray[i]->tipo);
		printf("\nPosición en x:    %d", pokeNestArray[i]->posx);
		printf("\nPosición en y:    %d", pokeNestArray[i]->posy);
		printf("\nIdentificador:    %c\n", pokeNestArray[i]->id);
		if(!queue_is_empty(pokeNestArray[i]->pokemons)) {
			printf("\nInstancias:       %d", queue_size(pokeNestArray[i]->pokemons));
			printf("\nNivel 1º en cola: %d\n\n\n", (*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).nivel);
		}
		i++;
	}
}

void sumarRecurso(t_list* items, char id) {
    ITEM_NIVEL* item = _search_item_by_id(items, id);
    item->quantity = item->quantity + 1;
}

void devolverPokemons(t_list *items, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]) {
	tPokemonMetadata *pokemon;
	while(!list_is_empty(entrenador->pokemons)) {
		pokemon = (tPokemonMetadata*)list_remove(entrenador->pokemons, 0);
		int i = 0;
		while(pokeNestArray[i]->id != pokemon->id)
			i++;
		if(pokeNestArray[i] != NULL) {
			queue_push(pokeNestArray[i]->pokemons, pokemon);
			sumarRecurso(items, pokeNestArray[i]->id);
		}
	}
}

int distanciaObjetivo(tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]) {
	int x, y, i = 0;
	if (pokeNestArray[i] && entrenador->obj) {
		while(pokeNestArray[i]->id != entrenador->obj)
			i++;
		if(pokeNestArray[i] != NULL) {
			x = abs(pokeNestArray[i]->posx - entrenador->posx);
			y = abs(pokeNestArray[i]->posy - entrenador->posy);
			return x + y;
		}
		else
			return -2; // Objetivo no encontrado o PokeNest Vacia
	}
	return -1; // Objetivo no establecido o PokeNest Vacia
}
