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

/* PKMN BATTLE */
#include <pkmn/battle.h>
#include <pkmn/factory.h>

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

/* Libreria con funciones del Mapa */
#include "mapalib.h"


/* Recibe el nombre del Mapa y la Ruta del PokeDex y retorna una estructura con la metadata del mapa */
int getMapaMetadata(tMapaMetadata *mapaMetadata, char *nomMapa, char *rutaPokeDex) {
	//printf("\nRuta PokeDex Cliente: \t%s\n", rutaPokeDex);
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/metadata", rutaPokeDex, mapaMetadata->nombre);
	t_config *mapConfig = config_create(ruta);
	if (mapConfig == NULL) return EXIT_FAILURE; // En caso de error devuelvo NULL
	//printf("\nRuta Mapa:\n%s", ruta);
	mapaMetadata->tiempoDeadlock  = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla         = config_get_int_value(mapConfig, "Batalla");
	strcpy(mapaMetadata->algoritmo, config_get_string_value(mapConfig, "algoritmo"));
	mapaMetadata->quantum         = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo         = config_get_int_value(mapConfig, "retardo");
	strcpy(mapaMetadata->ip,        config_get_string_value(mapConfig, "IP"));
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

void sumarRecurso(t_list* items, char id) {
	((ITEM_NIVEL*)(_search_item_by_id(items, id)))->quantity++;
	/*ITEM_NIVEL* item = _search_item_by_id(items, id);
    item->quantity = item->quantity + 1;*/
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

void desconectarEntrenador(t_list *items, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[], char *nomMapa) {
	devolverPokemons(items, entrenador, pokeNestArray);
	BorrarItem(items, entrenador->id);
	printf("Entrenador %c Desconectado!                                 ", entrenador->id);
	fflush(stdout);
    nivel_gui_dibujar(items, nomMapa);
	close(entrenador->socket);
	free(entrenador);
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

void moverEntrenador(t_list *items, tEntrenador *entrenador, char eje, char *nomMapa) {
	switch (eje) {
	case 'R':
		entrenador->posx++;
		break;
	case 'L':
		entrenador->posx--;
		break;
	case 'D':
		entrenador->posy++;
		break;
	case 'U':
		entrenador->posy--;
		break;
	}
	MoverPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
	nivel_gui_dibujar(items, nomMapa);
	send(entrenador->socket, "OK\n", 3, 0);
}

int enviarCoordenadasEntrenador(tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[], char pokeNest) {
	int pos = getPokeNestFromID(pokeNest);
	char mensaje[128];
	if (pokeNestArray[pos] != NULL) {
		entrenador->obj = pokeNest;
		sprintf(mensaje,"%3d%3d\nDistancia: %d\n", pokeNestArray[pos]->posx, pokeNestArray[pos]->posy, distanciaObjetivo(entrenador, pokeNestArray));
		send(entrenador->socket, mensaje, strlen(mensaje), 0);
		return 1;
	}
	else {
		sprintf(mensaje,"No existe la PokeNest\n");
		send(entrenador->socket, mensaje, strlen(mensaje), 0);
		return 0;
	}
}

int entregarPokemon(t_list *eBlocked, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[], char pokeNest) {
	int pos, dis;
	char mensaje[128];
	if (entrenador->obj == pokeNest) {
		pos = getPokeNestFromID(pokeNest);
		if (pokeNestArray[pos]) {
			dis = distanciaObjetivo(entrenador, pokeNestArray);
			if (dis == 0) {
					list_add(eBlocked, entrenador);
					sprintf(mensaje,"Entrenador %c a cola de Bloqueados!\n", entrenador->id);
					send(entrenador->socket, mensaje, strlen(mensaje), 0);
					printf("Entrenador %c a solicitado Pokemon %s.", entrenador->id, pokeNestArray[pos]->nombre);
					fflush(stdout);
					return 1;
			}
			else {
				sprintf(mensaje,"Aun se encuentra a %d de la PokeNest %s!\n", dis, pokeNestArray[pos]->nombre);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);
			}
		}
		else {
			sprintf(mensaje,"No existe la PokeNest!\n");
			send(entrenador->socket, mensaje, strlen(mensaje), 0);
		}
	}
	else {
		sprintf(mensaje,"Solicitar el objetivo primero!\n");
		send(entrenador->socket, mensaje, strlen(mensaje), 0);
	}
	return 0;
}
