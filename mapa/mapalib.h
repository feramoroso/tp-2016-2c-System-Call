#include <stdint.h>

#define RUTA_POKEDEX "/home/utnso/TP/tp-2016-2c-System-Call/PokeDex"
#define COMPLEMENTO_RUTA_MAPA "%s/Mapas/%s/metadata"

#define PATH_LOG_MAP "../../pruebas/log_mapa.txt"

typedef struct {
	uint32_t  tiempoDeadlock;
	uint32_t  batalla;
	char     *algoritmo;
	uint32_t  quantum;
	uint32_t  retardo;
	char     *ip;
	uint32_t  puerto;
} tMapaMetadata;

tMapaMetadata * getMapaMetadata(char * nomMapa, char *ruta);
