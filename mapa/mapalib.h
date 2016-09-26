#include <stdint.h>

//#define RUTA_POKEDEX "/home/utnso/TP/tp-2016-2c-System-Call/PokeDex"
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
	uint32_t  posx;
	uint32_t  posy;
	char      id;
	t_queue    *pokemons;
} tPokeNestMetadata;

/* Estructura para METADATA POKEMON */
typedef struct {
	uint32_t  nivel;
	//char     *art;
} tPokemonMetadata;

tMapaMetadata *getMapaMetadata(char *nomMapa, char *rutaPokeDex);
//tPokeNestMetadata **getPokeNestArray(char *nomMapa, char *rutaPokeDex);
tPokeNestMetadata *getPokeNestMetadata(char *nomMapa, char * nomPokeNest, char *rutaPokeDex);
tPokemonMetadata *getPokemonMetadata(char * nomPokeNest, int ord, char *rutaPokeNest);
//tPokemonMetadata *getPokemonMetadata(char *nomMapa, char * nomPokemon, int ord, char *rutaPokeDex);

//int obtenerCoordenadas(char *sCoordenadas, tPokeNestMetadata *pokeNestMetadata);
