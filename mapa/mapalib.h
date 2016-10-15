#include <stdint.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

/* PKMN BATTLE */
#include <pkmn/battle.h>
#include <pkmn/factory.h>

#define PATH_LOG_MAP "../../pruebas/log_mapa.txt"

/* Estructura para METADATA MAPA */
typedef struct {
	char     *nombre;
	char     *medalla;    // Ruta del archivo de la medalla del Mapa
//	char      medalla[256];    // Ruta del archivo de la medalla del Mapa
	uint32_t  pokeNestCant;
	uint32_t  tiempoDeadlock;
	uint32_t  batalla;
	char     *algoritmo;
	uint32_t  quantum;
	uint32_t  retardo;
	char     *ip;
	uint32_t  puerto;
} tMapaMetadata;

/* Estructura para METADATA POKENEST */
typedef struct {
	char     *nombre;
	char     *tipo;
	char      id;
	uint32_t  posx;
	uint32_t  posy;
	t_queue  *pokemons;
} tPokeNestMetadata;

/* Estructura para METADATA POKEMON */
typedef struct {
	char       id;
	uint32_t   ord;
	t_pokemon *data;
} tPokemonMetadata;

/* Estructura para ENTRENADOR */
typedef struct {
	int       socket;
	char      id;
	char      obj;        // Proximo objetivo dentro del Mapa
	time_t    time;
	uint32_t  posx;
	uint32_t  posy;
	t_list   *pokemons;   // Lista de Pokemons ordenada por nivel
} tEntrenador;


int getMapaMetadata(tMapaMetadata *mapaMetadata, char *nomMapa, char *rutaPokeDex);
void imprimirInfoMapa(tMapaMetadata *mapaMetadata);
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex);
tPokemonMetadata *getPokemonMetadata(char * nomPokeNest, char id, int ord, char *rutaPokeNest);
int getPokeNestArray(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex);
int getPokemonsQueue(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex);
void imprimirInfoPokeNest(tPokeNestMetadata *pokeNestArray[]);
void sumarRecurso(t_list* items, char id);
void devolverPokemons(t_list *items, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]);
void desconectarEntrenador(t_list *items, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[], char *nomMapa);
int distanciaObjetivo(tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]);
