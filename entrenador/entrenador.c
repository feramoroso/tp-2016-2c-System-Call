/***************************************************************************************************************************************************/
/************************************************         PROCESO MAPA         *********************************************************************/
/***************************************************************************************************************************************************/

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

/***************************************************************************************************************************************************/
/************************************************         DEFINICIONES         *********************************************************************/
/***************************************************************************************************************************************************/

#define PATH_LOG     "log.txt"
#define NOM_PROGRAMA "entrenador"
#define TAM_MENSAJE  256

typedef struct {
	char     id;
	int  x;
	int  y;
} tObj;

typedef struct {
	int     socket;
	char   *nombre;
	char	id;
	char   *dirBill;
	char   *dirMedallas;
	char    vidas;
	char    vidasDisponibles;
	char    reintentos;
	char    deadlocks;
	char    muertes;
	time_t  time;
	time_t  timeBlocked;
	t_list *mapas;
	uint32_t  x;
	uint32_t  y;
	tObj    obj;
} tEntrenador;

typedef struct {
	char     *nombre;
	char     *ip;
	uint32_t  puerto;
	t_list   *objetivos;
} tMapa;

/***************************************************************************************************************************************************/
/************************************************      VARIABLES GLOBALES      *********************************************************************/
/***************************************************************************************************************************************************/
tEntrenador  *entrenador;        // Estructura con la Metadata del Entrenador
char *rutaPokeDex;

t_log *logger;

/***************************************************************************************************************************************************/
/************************************************         FUNCIONES            *********************************************************************/
/***************************************************************************************************************************************************/
tMapa *getMapa(char *nomMapa) {
	tMapa *mapa = malloc(sizeof(tMapa));
	mapa->nombre = strdup(nomMapa);
//	printf("%s",mapa->nombre );
	t_config *mapaConfig = config_create(string_from_format("%s/Mapas/%s/metadata", rutaPokeDex, mapa->nombre));
	if (mapaConfig == NULL) {
		puts("No se encontro el mapa.");
		exit(EXIT_FAILURE);
	}
	mapa->ip     = strdup( config_get_string_value(mapaConfig, "IP") );
	mapa->puerto = config_get_int_value(mapaConfig, "Puerto");
	config_destroy(mapaConfig);
	mapa->objetivos = list_create();
	return mapa;
}

void getEntrenador(char *nomEntrenador) {
	int i, j;
	char **hojaDeViaje;
	char **objetivos;
	char  *objetivo;
	tMapa *mapa;
	entrenador = malloc(sizeof(tEntrenador));
	entrenador->nombre      = nomEntrenador;
	entrenador->dirBill     = string_from_format("%s/Entrenadores/%s/Dir de Bill/", rutaPokeDex, entrenador->nombre);
	entrenador->dirMedallas = string_from_format("%s/Entrenadores/%s/medallas/"   , rutaPokeDex, entrenador->nombre);
	t_config *entrenadorConfig = config_create(string_from_format("%s/Entrenadores/%s/metadata", rutaPokeDex, entrenador->nombre));
	if (entrenadorConfig == NULL) {
		puts("No se encontro el entrenador.");
		exit(EXIT_FAILURE);
	}
	entrenador->id = config_get_string_value(entrenadorConfig, "simbolo")[0];
	printf("\nNombre: %s [%c]", entrenador->nombre, entrenador->id);
	hojaDeViaje = config_get_array_value(entrenadorConfig, "hojaDeViaje");
	printf("\n\nHOJA DE VIAJE");
	entrenador->mapas = list_create();
	i = 0;
	while( hojaDeViaje[i] ) {
		mapa = getMapa(hojaDeViaje[i]);
		printf("\n\nMapa [%d]: %s", i+1, mapa->nombre);
		printf("\nIP         : %s", mapa->ip);
		printf("\nPuerto     : %d", mapa->puerto);
		printf("\nObjetivos  :");
		objetivos = config_get_array_value(entrenadorConfig, string_from_format("obj[%s]", mapa->nombre));
		j = 0;
		while(objetivos[j]) {
			objetivo = malloc(sizeof(objetivo));
			*objetivo = objetivos[j][0];
			printf(" %c", *objetivo);
			list_add(mapa->objetivos, objetivo);
			j++;
		}
		list_add(entrenador->mapas, mapa);
		i++;
	}
	entrenador->vidas      = config_get_int_value(entrenadorConfig, "vidas");
	entrenador->reintentos = config_get_int_value(entrenadorConfig, "reintentos");
}

void copiar(char *rutaOrigen, char *dirDestino) {
	char   ch;
	char  *archivoDestino;
	char **aux;
	FILE *source, *target;

	aux = string_split(rutaOrigen, "/");
	int i = 0;
	while (aux[i])
		i++;
	archivoDestino = string_from_format("%s%s", dirDestino, aux[i-1]);

	source = fopen(rutaOrigen,     "r");
	target = fopen(archivoDestino, "w");
	while( ( ch = fgetc(source) ) != EOF )
		fputc(ch, target);
	fclose(source);
	fclose(target);
}

void pressEnter() {
	printf("\n\nPresione enter para continuar...");
	getchar();
}

void desconectarMapa() {
	close(entrenador->socket);
}

void muerte() {
	printf("\n\n\n\t\t\t************  MUERTE  ************");
	printf(    "\n\t\t\t  %s has perdido una vida!"         , entrenador->nombre);
	printf(    "\n\t\t\t*********************************");
	entrenador->vidasDisponibles--;
	desconectarMapa();
}

void maestroPokemon() {
	printf("\n\n\n\t\t\t****************  FELICITACIONES  ****************");
	printf("    \n\t\t\t  %s te has convertido en Maestro Pokemon!", entrenador->nombre);
	printf("\n\n\n\t\t\t**************************************************");
	entrenador->vidasDisponibles = 0;
}

void conectarMapa(tMapa *mapa) {
	int bRecibidos;
	char mensaje[TAM_MENSAJE];
	struct sockaddr_in mapaDir;
	mapaDir.sin_family = AF_INET;
	mapaDir.sin_port = htons(mapa->puerto);    // Puerto extraido del archivo metadata
	inet_aton(mapa->ip, &(mapaDir.sin_addr));  // IP extraida del archivo metadata
	memset(&(mapaDir.sin_zero), '\0', 8);      // Pongo en 0 el resto de la estructura
	entrenador->socket = socket(AF_INET, SOCK_STREAM, 0);
	if ( connect(entrenador->socket, (void*) &mapaDir, sizeof(mapaDir)) == -1) {
		perror("MAPA");
		exit(EXIT_FAILURE);
	}
	printf("\n\nConexión Exitosa con %s!\n", mapa->nombre);
	bRecibidos = recv(entrenador->socket, mensaje, TAM_MENSAJE ,0);
	if( bRecibidos > 0) {
		mensaje[bRecibidos] = '\0';
		printf("%s", mensaje);
	}
	/* Manda el simbolo al Mapa */
	send(entrenador->socket, string_from_format("%c", entrenador->id), 1, 0);
	bRecibidos = recv(entrenador->socket, mensaje, TAM_MENSAJE, 0);
	bRecibidos = recv(entrenador->socket, mensaje, TAM_MENSAJE, 0);
}

void irPosicionInicial() {
	entrenador->x = 1;
	entrenador->y = 1;
}

void obtenerCoordenadas() {
	char coordenadas[6+1];  // en formato String XXXYYY
	/* El protocolo indica solicitar CX siendo X el id de la PokeNest */
	send(entrenador->socket, string_from_format("C%c", entrenador->obj.id), 2, 0);
	/* Recibo las coordenadas en formato XXXYYY */
	recv(entrenador->socket, coordenadas, 7, 0);
	/* Transformo los tres primeros caracteres XXX en int */
	entrenador->obj.x = atoi(string_substring(coordenadas, 0, 3));
	/* Transformo los tres primeros caracteres YYY en int */
	entrenador->obj.y = atoi(string_substring(coordenadas, 3, 3));
	printf("\nCoordenadas de %c -  x: %d, y: %d\n\n", entrenador->obj.id, entrenador->obj.x, entrenador->obj.y);
	fflush(stdout);
}

int irPosicionPokenest() {
	char msj[2+1];
	while ( (entrenador->x != entrenador->obj.x) || (entrenador->y != entrenador->obj.y) ) {
		/*  Movimiento en X */
		if (entrenador->x != entrenador->obj.x) {
			if (entrenador->x < entrenador->obj.x) {
				send(entrenador->socket, "MR", 2, 0);
				entrenador->x++;
			}
			else {
				send(entrenador->socket, "ML", 2, 0);
				entrenador->x--;
			}
			recv(entrenador->socket, msj, 2, 0);
		}
		/*  Movimiento en Y */
		if (entrenador->y != entrenador->obj.y) {
			if (entrenador->y < entrenador->obj.y) {
				send(entrenador->socket, "MD", 2, 0);
				entrenador->y++;
			}
			else {
				send(entrenador->socket, "MU", 2, 0);
				entrenador->y--;
			}
			recv(entrenador->socket, msj, 2, 0);
		}
	}
	return 0;
}

int deadlock() {
	char mensaje[256];
	recv(entrenador->socket, mensaje, 256, 0);
	while ( mensaje[0] != 'R' ){
		if ( mensaje[0] != 'K' )
			return 1;
		printf("%s", mensaje);
		recv(entrenador->socket, mensaje, 256, 0);
	}
	return 0;
}

int capturarPokemon() {
	int bRecibidos;
	char mensaje[256];
	send(entrenador->socket, string_from_format("G%c", entrenador->obj.id), 2, 0);
	printf("Capturando %c...\n", entrenador->obj.id);
	bRecibidos = recv(entrenador->socket, mensaje, 256, 0);
	mensaje[bRecibidos] = 0;
	if ( mensaje[0] == 'D' )
		if ( deadlock() )
			return 1;
	printf("%s", mensaje);
	copiar(mensaje, entrenador->dirBill);
	return 0;
}

int jugarObjetivo() {
	printf("\nObjetivo: %c", entrenador->obj.id);
	obtenerCoordenadas();
//	pressEnter();
	irPosicionPokenest();
	if ( capturarPokemon() )
		return 1;
//	pressEnter();
	return 0;
}

int jugarMapa(tMapa *mapa) {
	printf("\nMapa en Juego : %s", mapa->nombre);
	conectarMapa(mapa);
	irPosicionInicial();
	int objCumplidos = 0;
	while( objCumplidos < list_size(mapa->objetivos) ) {
		entrenador->obj.id = *(char*)list_get(mapa->objetivos, objCumplidos);
		if( jugarObjetivo() )
			return 1;
		objCumplidos++;
	}
	desconectarMapa();
	return 0;
}

int jugarVida() {
	printf("\n\n\nVidas Disponibles : %d", entrenador->vidasDisponibles);
	tMapa *mapa;
	int mapasJugados = 0;
	while( mapasJugados < list_size(entrenador->mapas) ) {
		mapa = list_get(entrenador->mapas, mapasJugados);
		if( jugarMapa(mapa) )
			return 1;
		mapasJugados++;
	}
	return 0;
}

/***************************************************************************************************************************************************/
/************************************************             MAIN             *********************************************************************/
/***************************************************************************************************************************************************/
int main(int argc , char *argv[]) {
	system("clear");
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: entrenador <nombre_entrenador> <ruta PokeDex>");
		return EXIT_FAILURE;
	}
/**************************************************   SECCION METADATA   ***************************************************************************/
	/* OBTENER METADATA */
	rutaPokeDex        = argv[2];
	getEntrenador(argv[1]);

	/* LOG */
	logger = log_create("log.txt", entrenador->nombre, false, LOG_LEVEL_INFO);

/**************************************************  SECCION PROGRAMA   ****************************************************************************/
	char jugar = 's';
	while( jugar == 's' || jugar == 'S') {
		pressEnter();
		system("clear");
		printf("\n\n\n\t\t\t********* BIENVENIDO AL JUEGO *********");
		printf("\nNumero de Reintentos : %d", entrenador->reintentos);
		entrenador->vidasDisponibles = entrenador->vidas;
		while(entrenador->vidasDisponibles) {
			if( jugarVida() )
				muerte();
			else
				maestroPokemon();
		}
//		printf("\nPresione enter para continuar...");
		printf("\n\nIngrese S para reintentar... ");
		jugar = getchar();
		getchar();
		if( jugar == 's' || jugar == 'S') entrenador->reintentos++;
	}

	close(entrenador->socket);
	free(entrenador);
	log_destroy(logger);
	return EXIT_SUCCESS;
}
