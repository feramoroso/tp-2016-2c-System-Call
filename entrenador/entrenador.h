#include <stdint.h>
#include <commons/collections/list.h>

#define COACH_METADAMA "/Entrenadores/Red/metadata.txt"
#define PATH_LOG_COACH "log_entrenador.txt"
#define PROGRAM_COACH "entrenador"

#define SOLICITA_POKENEST 1
#define AVANZAR_POSICION 2
#define ATRAPAR_POKEMON 3
#define FINALIZO_MAPA 4

struct confCoach{
	char 	*nombre;
	char	simbolo;
	char	**hojaDeViaje;
	char	**objetivos;
	uint32_t vidas;
	uint32_t reintentos;
}confCoach;

void get_config(struct confCoach *entrenadorMetadata, char *path);

char* avanzarPosicion(int, int);

char* moverDerecha();
char* moverIzquierda();
char* moverAbajo();
char* moverArriba();
