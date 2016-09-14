#include<stdint.h>
#include<commons/collections/list.h>

#define MAP_METADAMA "/Mapas/PuebloPaleta/metadata.txt"
#define PATH_LOG_MAP "../../pruebas/log_mapa.txt"
#define PROGRAM_MAP "mapa"

struct confMap{
	uint32_t  time_check_deadlock;
	uint32_t   battle;
	char      *algoritmo;
	uint32_t  time_retardo;
	char      *ip;
	char      *puerto;
}confMap;

void get_config_map(struct confMap *configMap, char *path);