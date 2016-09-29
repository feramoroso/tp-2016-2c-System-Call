#include <stdint.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

#define PATH_LOG_MAP "../../pruebas/log_mapa.txt"

/* Estructura para METADATA MAPA */
typedef struct {
	char      nombre[30];
	uint32_t  tiempoDeadlock;
	uint32_t  batalla;
	char      algoritmo[10];
	uint32_t  quantum;
	uint32_t  retardo;
	char      ip[30];
	uint32_t  puerto;
} tMapaMetadata;

/* Estructura para METADATA POKENEST */
typedef struct {
	char      nombre[30];
	char      tipo[20];
	char      id;
	uint32_t  posx;
	uint32_t  posy;
	t_queue  *pokemons;
} tPokeNestMetadata;

/* Estructura para METADATA POKEMON */
typedef struct {
	char      id;
	uint32_t  nivel;
	//char     *art;
} tPokemonMetadata;

/* Estructura para ENTRENADOR */
typedef struct {
	uint32_t  threadID;
	int       socket;
	char      id;
	time_t    time;
	uint32_t  posx;
	uint32_t  posy;
	char      obj;        // Proximo objetivo dentro del Mapa
	t_list   *pokemons;   // Lista de Pokemons ordenada por nivel
} tEntrenador;


tMapaMetadata *getMapaMetadata(char *nomMapa, char *rutaPokeDex);
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex);
tPokemonMetadata *getPokemonMetadata(char * nomPokeNest, int ord, char *rutaPokeNest);
//tPokeNestMetadata **getPokeNestArray(char *nomMapa, char *rutaPokeDex);
int getPokeNestArray(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex);
int getPokemonsQueue(tPokeNestMetadata *pokeNestArray[], char *nomMapa, char *rutaPokeDex);

void imprimirInfoPokeNest(tPokeNestMetadata *pokeNestArray[]);
void sumarRecurso(t_list* items, char id);
void devolverPokemons(t_list *items, tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]);
int distanciaObjetivo(tEntrenador *entrenador, tPokeNestMetadata *pokeNestArray[]);
