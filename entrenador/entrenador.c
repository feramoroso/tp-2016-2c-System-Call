/***************************************************************************************************************************************************/
/************************************************      PROCESO ENTRENADOR      *********************************************************************/
/***************************************************************************************************************************************************/
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
/***************************************************************************************************************************************************/
/************************************************         DEFINICIONES         *********************************************************************/
/***************************************************************************************************************************************************/
#define TAM_MENSAJE  256

/* COLORES */
#define RESET        "\033[0m"
#define SUPER        "\033[1m"              /* Bright White */
#define BOLDCYAN     "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDRED      "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN    "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW   "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDCYAN     "\033[1m\033[36m"      /* Bold Cyan */
#define _RED         "\033[91m"             /* Bright Red */
#define _MAGENTA     "\033[95m"             /* Bright Magenta */


/* TIPOS */
typedef struct {
	char  id;
	int    x;
	int    y;
} tObj;

typedef struct {
	int     socket;
	char   *nombre;
	char	id;
	char   *dirBill;
	char   *dirMedallas;
	char    vidas;
	char    vidasDisponibles;
	char    mapasJugados;
	char    reintentos;
	char    deadlocks;
	char    muertes;
	char    sigterm;
	int     x;
	int     y;
	tObj    obj;
	time_t  time;
	time_t  timeBlocked;
	t_list *mapas;
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
tEntrenador  *entrenador;
char *rutaPokeDex;
/* Data Consola */
struct termios oldt, newt;
/***************************************************************************************************************************************************/
/************************************************         FUNCIONES            *********************************************************************/
/***************************************************************************************************************************************************/
void salir(int status) {
	list_clean_and_destroy_elements(entrenador->mapas, free);
	free(entrenador->dirBill);
	free(entrenador->dirMedallas);
	free(entrenador);
	free(rutaPokeDex);
	/******** RESTAURACION CONSOLA ********/
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	system("setterm -cursor on");
	/**************************************/
	exit(status);
}
void borrar() {
	char *aux = string_from_format("rm -f \"%s\"*", entrenador->dirBill);
	system(aux);
	free(aux);
	aux = string_from_format("rm -f %s*", entrenador->dirMedallas);
	system(aux);
	free(aux);
}
tMapa *getMapa(char *nomMapa) {
	tMapa *mapa = malloc(sizeof(tMapa));
	mapa->nombre = strdup(nomMapa);
	t_config *mapaConfig = config_create(string_from_format("%s/Mapas/%s/metadata", rutaPokeDex, mapa->nombre));
	if (mapaConfig == NULL) {
		puts("No se encontro el mapa.");
		salir(EXIT_FAILURE);
	}
	mapa->ip        = strdup( config_get_string_value(mapaConfig, "IP") );
	mapa->puerto    = config_get_int_value(mapaConfig, "Puerto");
	mapa->objetivos = list_create();
	config_destroy(mapaConfig);
	return mapa;
}
void getEntrenador(char *nomEntrenador) {
	int i, j;
	char **hojaDeViaje;
	char **objetivos;
	char  *objetivo;
	char  *createDir;
	tMapa *mapa;
	entrenador = malloc(sizeof(tEntrenador));
	entrenador->nombre      = nomEntrenador;
	/* Crea el directorio "Dir de Bill" y guarda la ruta en la estructura */
	entrenador->dirBill     = string_from_format("%s/Entrenadores/%s/Dir de Bill/", rutaPokeDex, entrenador->nombre);
	createDir = string_from_format("mkdir -p \"%s\"", entrenador->dirBill);
	system(createDir);
	free(createDir);
	/* Crea el directorio "medallas" y guarda la ruta en la estructura */
	entrenador->dirMedallas = string_from_format("%s/Entrenadores/%s/medallas/"   , rutaPokeDex, entrenador->nombre);
	createDir = string_from_format("mkdir -p \"%s\"", entrenador->dirMedallas);
	system(createDir);
	free(createDir);
	borrar();
	t_config *entrenadorConfig = config_create(string_from_format("%s/Entrenadores/%s/metadata", rutaPokeDex, entrenador->nombre));
	if (entrenadorConfig == NULL) {
		puts("\n\nNo se encontro el entrenador.");
		salir(EXIT_FAILURE);
	}
	entrenador->id = config_get_string_value(entrenadorConfig, "simbolo")[0];
	printf(SUPER "\nNombre: " RESET "%s " "[" BOLDGREEN "%c" RESET "]", entrenador->nombre, entrenador->id);
	hojaDeViaje = config_get_array_value(entrenadorConfig, "hojaDeViaje");
	printf(BOLDYELLOW "\n\nHOJA DE VIAJE" RESET);
	entrenador->mapas = list_create();
	i = 0;
	while( hojaDeViaje[i] ) {
		mapa = getMapa(hojaDeViaje[i]);
		printf(SUPER "\n\nMapa [%d]:  " RESET "%s", i+1, mapa->nombre);
		printf(SUPER    "\nIP:        " RESET "%s", mapa->ip);
		printf(SUPER    "\nPuerto:    " RESET "%d", mapa->puerto);
		printf(SUPER    "\nObjetivos:"  RESET);
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
	entrenador->reintentos = 0; //config_get_int_value(entrenadorConfig, "reintentos");
	config_destroy(entrenadorConfig);
	free(hojaDeViaje);
}
struct tm tiempo(int seg) {
	struct tm t;
	t.tm_hour = (int)(seg / 3600);
	t.tm_min  = (int)((seg - t.tm_hour * 3600) / 60);
	t.tm_sec  = seg - (t.tm_hour * 3600 + t.tm_min * 60);
	return t;
}
void desconectarMapa() {
	close(entrenador->socket);
}
void sinVidas() {
	printf(BOLDRED "\n\n\n\t\t*******************" RESET SUPER "  GAME OVER  " BOLDRED "*********************" RESET);
	printf(SUPER     "\n\t\t\t%s [%c] has quedado sin vidas!"                         , entrenador->nombre, entrenador->id);
	printf(BOLDRED     "\n\t\t*****************************************************" RESET);
}
void muerte() {
	printf(BOLDRED "\n\n\n\t\t**********************" RESET SUPER "  MUERTE  " BOLDRED "**********************" RESET);
	printf(SUPER     "\n\t\t\t%s [%c] has perdido la vida!"                          , entrenador->nombre, entrenador->id);
	printf(BOLDRED     "\n\t\t******************************************************\n\n" RESET);
	entrenador->muertes++;
	entrenador->vidasDisponibles--;
	desconectarMapa();
}
void signalRutina (int n) {
	switch (n) {
		case SIGUSR1:
			printf(BOLDRED "\n\n\n\t\t**********************" RESET SUPER "  MUERTE  " BOLDRED "**********************" RESET);
			printf(SUPER     "\n\t\t\t%s [%c] has recibido una vida!"                          , entrenador->nombre, entrenador->id);
			printf(BOLDRED     "\n\t\t******************************************************\n\n" RESET);
			entrenador->vidasDisponibles++;
			break;
		case SIGTERM:
			entrenador->sigterm = 1;
			/*
			if (entrenador->vidasDisponibles == 1)
				entrenador->sigterm = 1;
			else if (entrenador->vidasDisponibles > 1) {
				printf(BOLDRED "\n\n\n\t\t**********************" RESET SUPER "  MUERTE  " BOLDRED "**********************" RESET);
				printf(SUPER     "\n\t\t\t%s [%c] has perdido una vida!"                          , entrenador->nombre, entrenador->id);
				printf(BOLDRED     "\n\t\t******************************************************\n\n" RESET);
				entrenador->vidasDisponibles--;
			}*/
			break;
		case SIGINT:
			salir(EXIT_FAILURE);
			break;
	}
}
void copiar(char *rutaOrigen, char *dirDestino) {
	char   ch;
	char  *archivoOrigen,  *archivoDestino;
	char **aux;
	FILE  *source, *target;

	archivoOrigen = string_from_format("%s%s", rutaPokeDex, rutaOrigen);

	aux = string_split(rutaOrigen, "/");
	int i = 0;
	while (aux[i])
		i++;
	archivoDestino = string_from_format("%s%s", dirDestino, aux[i-1]);

	source = fopen(archivoOrigen,     "r");
	target = fopen(archivoDestino, "w");
	while( ( ch = fgetc(source) ) != EOF )
		fputc(ch, target);
	fclose(source);
	fclose(target);

	free(archivoDestino);
	free(archivoOrigen);
	free(aux);
}
char getCaracter(char *msj) {
	char c;
	printf(SUPER "%s\n" RESET, msj);
	tcflush(STDIN_FILENO, TCIFLUSH);
	c = getc(stdin);
	tcflush(STDIN_FILENO, TCIFLUSH);
	return c;
}
void maestroPokemon() {
	struct tm tTotal, tBlocked;
	tTotal   = tiempo( time(0) - entrenador->time );
	tBlocked = tiempo( entrenador->timeBlocked );
	printf(BOLDGREEN  "\n\n\n\t\t******************" RESET SUPER "  FELICITACIONES  " BOLDGREEN "******************" RESET);
	printf(SUPER      "\n\t\t                       %s [%c]"                             , entrenador->nombre, entrenador->id);
	printf(BOLDYELLOW "\n\t\t      Te has convertido en Maestro Pokemon!" RESET);
	printf(SUPER  "\n\n\t\t      Tiempo total :                 " RESET "%2dh %2dm %2ds"   , tTotal.tm_hour, tTotal.tm_min, tTotal.tm_sec);
	printf(SUPER    "\n\t\t      Cantidad de Muertes:           " RESET "         %d"     , entrenador->muertes);
	printf(SUPER    "\n\t\t      Cantidad de Deadlocks:         " RESET "         %d"     , entrenador->deadlocks);
	printf(SUPER    "\n\t\t      Tiempo bloqueado en Pokenests: " RESET "%2dh %2dm %2ds"   , tBlocked.tm_hour, tBlocked.tm_min, tBlocked.tm_sec);
	printf(BOLDGREEN    "\n\t\t******************************************************" RESET);
}
void conectarMapa(tMapa *mapa) {
	int nB;
	char mensaje[TAM_MENSAJE];
	struct sockaddr_in mapaDir;
	mapaDir.sin_family = AF_INET;
	mapaDir.sin_port = htons(mapa->puerto);    // Puerto extraido del archivo metadata
	inet_aton(mapa->ip, &(mapaDir.sin_addr));  // IP extraida del archivo metadata
	memset(&(mapaDir.sin_zero), '\0', 8);      // Pongo en 0 el resto de la estructura
	entrenador->socket = socket(AF_INET, SOCK_STREAM, 0);
	if ( connect(entrenador->socket, (void*) &mapaDir, sizeof(mapaDir)) == -1) {
		perror("\n\nMAPA");
		salir(EXIT_FAILURE);
	}
	printf("\n\nConexión Exitosa con %s!\n", mapa->nombre);
	/* Recibo la bienvenida */
	if( (nB = recv(entrenador->socket, mensaje, TAM_MENSAJE ,0)) < 1) {
		perror("recv");
		salir(EXIT_FAILURE);
	}
	mensaje[nB] = '\0';
	printf(SUPER "\n%s\n" RESET, mensaje);
	/* Manda el simbolo al Mapa */
	mensaje[0] = entrenador->id;
	send(entrenador->socket, mensaje, 1, 0);
}
void irPosicionInicial() {
	entrenador->x = 1;
	entrenador->y = 1;
}
void obtenerMedalla() {
	int nB;
	char rutaMedalla[TAM_MENSAJE], *aux;
	send(entrenador->socket, "O", 1, 0);
	nB = recv(entrenador->socket, rutaMedalla, TAM_MENSAJE, 0);
	rutaMedalla[nB] = '\0';
//	printf("\n%d", nB);
	puts(BOLDGREEN "\n\n\nMapa Terminado!" RESET);
	puts("Ruta de la medalla obtenida...");
	printf("%s\n", rutaMedalla);
	aux = string_from_format("cp %s%s %s", rutaPokeDex, rutaMedalla, entrenador->dirMedallas);
	system(aux);
	free(aux);
//	copiar(rutaMedalla, entrenador->dirMedallas);
}
void obtenerCoordenadas() {
	int nB;
	char coordenadas[6+1], *aux;  // en formato String XXXYYY
	/* El protocolo indica solicitar CX siendo X el id de la PokeNest */
	aux = string_from_format("C%c", entrenador->obj.id);
	send(entrenador->socket, aux, 2, 0);
	free(aux);
	/* Recibo las coordenadas en formato XXXYYY */
	nB = recv(entrenador->socket, coordenadas, 6, 0);
	coordenadas[nB] = '\0';
//	printf("\n%d", nB);
//	printf("\n%s\n", coordenadas);
	if( coordenadas[0] == 'N') {
		puts("No existe el Pokemon solicitado en el Mapa.");
		fflush(stdout);
		salir(EXIT_FAILURE);
	}
	/* Transformo los tres primeros caracteres XXX en int */
	aux = string_substring(coordenadas, 0, 3);
	entrenador->obj.x = atoi(aux);
	free(aux);
	/* Transformo los tres ultimos caracteres YYY en int */
	aux = string_substring(coordenadas, 3, 3);
	entrenador->obj.y = atoi(aux);
	free(aux);
	printf("\nCoordenadas\n" SUPER "x: " RESET "%d  -  " SUPER "y: " RESET "%d\n", entrenador->obj.x, entrenador->obj.y);
	puts(_MAGENTA "Buscando..." RESET);
	fflush(stdout);
}
int irPosicionPokenest() {
	char msj[2+1];
	while ( (entrenador->x != entrenador->obj.x) || (entrenador->y != entrenador->obj.y) ) {
		if ( entrenador->sigterm )
			return 1;
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
int capturarPokemon() {
	int nB;
	char mensaje[TAM_MENSAJE], *aux;
	aux = string_from_format("G%c", entrenador->obj.id);
	send(entrenador->socket, aux, 2, 0);
	free(aux);
	printf(_RED "Capturando %c...\n" RESET, entrenador->obj.id);
	while (1) {
		nB = recv(entrenador->socket, mensaje, TAM_MENSAJE, 0);
		if ( entrenador->sigterm )
			return 1;
		mensaje[nB] = '\0';
//		printf("\n%d", nB);
//		printf("\n%s\n", mensaje);
		switch ( mensaje[0] ) {
		case 'D':
			printf(BOLDRED "\n\t\t**********  " RESET SUPER "%s estas en " BOLDRED "DEADLOCK!  *********\n" RESET, entrenador->nombre);
			entrenador->deadlocks++;
			break;
		case 'R':
			printf(BOLDGREEN "\n\t**********  " RESET SUPER "%s has sobrevivido a la Batalla Pokemon!" BOLDGREEN "  *********\n\n" RESET, entrenador->nombre);
			break;
		case 'K':
			return 1;
		default:
			puts(BOLDYELLOW "Pokemón Capturado!" RESET);
			puts("Ruta del Pokemon capturado...");
			printf("%s\n", mensaje);
			aux = string_from_format("cp %s%s \"%s\"", rutaPokeDex, mensaje, entrenador->dirBill);
			system(aux);
			free(aux);
//			copiar(mensaje, entrenador->dirBill);
			return 0;
		}
	}
}
int jugarObjetivo() {
	time_t tBloqueado;
	printf(BOLDCYAN "\n\nObjetivo: " RESET SUPER "%c" RESET, entrenador->obj.id);
	fflush(stdout);
	obtenerCoordenadas();
	if ( irPosicionPokenest() )
		return 1;
	tBloqueado = time(0);
	if ( capturarPokemon() ) {
		entrenador->timeBlocked += time(0) - tBloqueado;
		return 1;
	}
	entrenador->timeBlocked += time(0) - tBloqueado;
	return 0;
}
int jugarMapa(tMapa *mapa) {
	conectarMapa(mapa);
//	printf("\nMapa en Juego : %s", mapa->nombre);
	fflush(stdout);
	irPosicionInicial();
	int objCumplidos = 0;
	while( objCumplidos < list_size(mapa->objetivos) ) {
		entrenador->obj.id = *(char*)list_get(mapa->objetivos, objCumplidos);
		if( jugarObjetivo() )
			return 1;
		objCumplidos++;
	}
	obtenerMedalla();
	desconectarMapa();
	return 0;
}
int jugarVida() {
	entrenador->sigterm = 0;
	printf(SUPER "\nVidas Disponibles: " RESET "%d", entrenador->vidasDisponibles);
	fflush(stdout);
	tMapa *mapa;
	while( entrenador->mapasJugados < list_size(entrenador->mapas) ) {
		mapa = list_get(entrenador->mapas, entrenador->mapasJugados);
		if( jugarMapa(mapa) ) {
			muerte();
			return 1;
		}
		entrenador->mapasJugados++;
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
	rutaPokeDex = realpath(argv[2], NULL);
	getEntrenador(argv[1]);
/**************************************************   SECCION SEÑALES   ****************************************************************************/
	signal(SIGUSR1, signalRutina);
	signal(SIGTERM, signalRutina);
/**************************************************  SECCION PROGRAMA   ****************************************************************************/
	/******** MODIFICACION CONSOLA ********/
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	system("setterm -cursor off");
	setbuf(stdin, NULL);
	/**************************************/
	getCaracter("\n\nPresione una tecla para continuar...");
	char jugar = 'r';
	while( jugar == 'r' || jugar == 'R') {
		system("clear");
		printf(BOLDCYAN "\n\t\t**********************************************" RESET);
		printf(BOLDCYAN "\n\t\t**********************************************" RESET);
		printf(BOLDCYAN "\n\t\t**********************************************" RESET);
		printf(BOLDCYAN "\n\t\t**********                          **********" RESET);
		printf(BOLDCYAN "\n\t\t**********                          **********" RESET);
		printf(BOLDCYAN "\n\t\t**********   " RESET SUPER "BIENVENIDO AL JUEGO" BOLDCYAN "    **********" RESET);
		printf(BOLDCYAN "\n\t\t**********                          **********" RESET);
		printf(BOLDCYAN "\n\t\t**********       " BOLDGREEN "%8s" BOLDCYAN "           **********" RESET, entrenador->nombre);
		printf(BOLDCYAN "\n\t\t**********                          **********" RESET);
		printf(BOLDCYAN "\n\t\t**********                          **********" RESET);
		printf(BOLDCYAN "\n\t\t**********************************************" RESET);
		printf(BOLDCYAN "\n\t\t**********************************************" RESET);
		printf(BOLDCYAN "\n\t\t**********************************************\n\n" RESET);
		borrar();
		entrenador->vidasDisponibles = entrenador->vidas;
		entrenador->mapasJugados     = 0;
		entrenador->deadlocks        = 0;
		entrenador->muertes          = 0;
		entrenador->time             = time(0);
		entrenador->timeBlocked      = 0;
		while(entrenador->vidasDisponibles) {
			if( !jugarVida() ) {
				maestroPokemon();
				break;
			}
		}
		if( entrenador->vidasDisponibles == 0) {
			sinVidas();
			borrar();
			puts(RESET SUPER "\n\nArchivos de Objetivos cumplidos borrados." RESET);
		}
		printf(SUPER "\n\nNumero de Reintentos: " RESET "%d", entrenador->reintentos);
		jugar = getCaracter("\n\nPresione R para reintentar, cualquier otra tecla para salir...");
		entrenador->reintentos++;
	}
	salir(EXIT_SUCCESS);
}
