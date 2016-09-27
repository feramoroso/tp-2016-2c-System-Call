#include<stdint.h>
#include<commons/collections/list.h>

#define COACH_METADAMA "/Entrenadores/Red/metadata.txt"
#define PATH_LOG_COACH "log_entrenador.txt"
#define PROGRAM_COACH "entrenador"

struct confCoach{
	char 	*nombre;
	char	simbolo;
	char	**hojaDeViaje;
	char	**objetivos;
	uint32_t vidas;
	uint32_t reintentos;
}confCoach;

void get_config(struct confCoach *entrenadorMetadata, char *path);
