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
	char    reintentos;
	char    deadlocks;
	char    muertes;
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
/***************************************************************************************************************************************************/
/************************************************         FUNCIONES            *********************************************************************/
/***************************************************************************************************************************************************/
tMapa *getMapa(char *nomMapa) {
	tMapa *mapa = malloc(sizeof(tMapa));
	mapa->nombre = strdup(nomMapa);
	t_config *mapaConfig = config_create(string_from_format("%s/Mapas/%s/metadata", rutaPokeDex, mapa->nombre));
	if (mapaConfig == NULL) {
		puts("No se encontro el mapa.");
		exit(EXIT_FAILURE);
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
	tMapa *mapa;
	entrenador = malloc(sizeof(tEntrenador));
	entrenador->nombre      = nomEntrenador;
	entrenador->dirBill     = string_from_format("%s/Entrenadores/%s/Dir de Bill/", rutaPokeDex, entrenador->nombre);
	entrenador->dirMedallas = string_from_format("%s/Entrenadores/%s/medallas/"   , rutaPokeDex, entrenador->nombre);
	t_config *entrenadorConfig = config_create(string_from_format("%s/Entrenadores/%s/metadata", rutaPokeDex, entrenador->nombre));
	if (entrenadorConfig == NULL) {
		puts("\n\nNo se encontro el entrenador.");
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
		printf("\n\nMapa [%d]  %s", i+1, mapa->nombre);
		printf(  "\nIP       : %s", mapa->ip);
		printf(  "\nPuerto   : %d", mapa->puerto);
		printf(  "\nObjetivos:");
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
	config_destroy(entrenadorConfig);
}
struct tm tiempo(int seg) {
	struct tm t;
	t.tm_hour = (int)(seg / 3600);
	t.tm_min  = (int)((seg - t.tm_hour * 3600) / 60);
	t.tm_sec  = seg - (t.tm_hour * 3600 + t.tm_min * 60);
	return t;
}
void signalRutina (int n) {
	switch (n) {
		case SIGUSR1:
			printf("\n\n*****  %s has recibido una vida! *****\n\n", entrenador->nombre);
			entrenador->vidasDisponibles++;
		break;
		case SIGTERM:
			if (entrenador->vidasDisponibles > 0) {
				printf("\n\n*****  %s has perdido una vida! *****\n\n", entrenador->nombre);
				entrenador->vidasDisponibles--;
			}
		break;
	}
}
void copiar(char *rutaOrigen, char *dirDestino) {
	char   ch;
	char  *archivoDestino;
	char **aux;
	FILE  *source, *target;

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
	free(archivoDestino);
	free(aux);
}
void borrar() {
	char aux[256];
	sprintf(aux, "rm \"%s\"*", entrenador->dirBill);
	system(aux);
	sprintf(aux, "rm %s*", entrenador->dirMedallas);
	system(aux);
}
char getCaracter(char *msj) {
	char c;
	puts(msj);
	tcflush(STDIN_FILENO, TCIFLUSH);
	c = getc(stdin);
	tcflush(STDIN_FILENO, TCIFLUSH);
	return c;
}
void desconectarMapa() {
	close(entrenador->socket);
}
void sinVidas() {
	printf("\n\n\n\t\t******************************************************");
	printf("\n\t\t\t\t%s has quedado sin vidas!"                             , entrenador->nombre);
	printf(    "\n\t\t******************************************************");
}
void muerte() {
	printf("\n\n\n\t\t**********************  MUERTE  **********************");
	printf("\n\t\t\t\t%s has perdido una vida!"                             , entrenador->nombre);
	printf(    "\n\t\t******************************************************");
	entrenador->muertes++;
	entrenador->vidasDisponibles--;
	desconectarMapa();
}
void maestroPokemon() {
	struct tm tTotal, tBlocked;
	tTotal   = tiempo( time(0) - entrenador->time );
	tBlocked = tiempo( entrenador->timeBlocked );
	printf("\n\n\n\t\t******************  FELICITACIONES  ******************");
	printf(    "\n\t\t                       %s"                             , entrenador->nombre);
	printf(    "\n\t\t      Te has convertido en Maestro Pokemon!"          );
	printf(  "\n\n\t\t      Tiempo total :                 %2dh %2dm %2ds"   , tTotal.tm_hour, tTotal.tm_min, tTotal.tm_sec);
	printf(    "\n\t\t      Cantidad de Muertes:                     %d"     , entrenador->muertes);
	printf(    "\n\t\t      Cantidad de Deadlocks:                   %d"     , entrenador->deadlocks);
	printf(    "\n\t\t      Tiempo bloqueado en Pokenests: %2dh %2dm %2ds"   , tBlocked.tm_hour, tBlocked.tm_min, tBlocked.tm_sec);
	printf(    "\n\t\t******************************************************");
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
		exit(EXIT_FAILURE);
	}
	printf("\n\nConexión Exitosa con %s!\n", mapa->nombre);
	/* Recibo la bienvenida */
	if( (nB = recv(entrenador->socket, mensaje, TAM_MENSAJE ,0)) < 1) {
		perror("recv");
		exit(EXIT_FAILURE);
	}
	mensaje[nB] = '\0';
	printf("\n%d", nB);
	printf("\n%s\n", mensaje);
	/* Manda el simbolo al Mapa */
	send(entrenador->socket, string_from_format("%c", entrenador->id), 1, 0);
}
void irPosicionInicial() {
	entrenador->x = 1;
	entrenador->y = 1;
}
void obtenerMedalla() {
	int nB;
	char rutaMedalla[TAM_MENSAJE], aux[TAM_MENSAJE];
	send(entrenador->socket, "O", 1, 0);
	nB = recv(entrenador->socket, rutaMedalla, TAM_MENSAJE, 0);
	rutaMedalla[nB] = '\0';
	printf("\n%d", nB);
	printf("\n%s\n", rutaMedalla);
	sprintf(aux, "cp %s%s %s", rutaPokeDex, rutaMedalla, entrenador->dirMedallas);
	system(aux);
//	copiar(rutaMedalla, entrenador->dirMedallas);
}
void obtenerCoordenadas() {
	int nB;
	char coordenadas[6+1];  // en formato String XXXYYY
	/* El protocolo indica solicitar CX siendo X el id de la PokeNest */
	send(entrenador->socket, string_from_format("C%c", entrenador->obj.id), 2, 0);
	/* Recibo las coordenadas en formato XXXYYY */
	nB = recv(entrenador->socket, coordenadas, 6, 0);
	coordenadas[nB] = '\0';
	printf("\n%d", nB);
	printf("\n%s\n", coordenadas);

	/* Transformo los tres primeros caracteres XXX en int */
	entrenador->obj.x = atoi(string_substring(coordenadas, 0, 3));
	/* Transformo los tres ultimos caracteres YYY en int */
	entrenador->obj.y = atoi(string_substring(coordenadas, 3, 3));
	printf("\nCoordenadas de %c    x: %d  y: %d\n", entrenador->obj.id, entrenador->obj.x, entrenador->obj.y);
	fflush(stdout);
}
void irPosicionPokenest() {
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
}
int deadlock() {
	int nB;
	char mensaje[256];
	entrenador->deadlocks++;
	while (mensaje[0] != 'R' ) {
		nB = recv(entrenador->socket, mensaje, 1, 0);
		mensaje[nB] = '\0';
		printf("\n%d", nB);
		printf("\n%s\n\n", mensaje);
		if ( mensaje[0] == 'K' )
				return 1;
	}
	return 0;
}
int capturarPokemon() {
	int nB;
	char mensaje[TAM_MENSAJE], aux[TAM_MENSAJE];
	send(entrenador->socket, string_from_format("G%c", entrenador->obj.id), 2, 0);
	printf("Capturando %c...\n", entrenador->obj.id);
	nB = recv(entrenador->socket, mensaje, TAM_MENSAJE, 0);
	mensaje[nB] = '\0';
	printf("\n%d", nB);
	printf("\n%s\n\n", mensaje);
	while ( mensaje[0] == 'D' ) {
		printf("\n\t\t**********  %s estas en DEADLOCK!  *********\n", entrenador->nombre);
		if ( deadlock() )
			return 1;
		printf("\n\t**********  %s has sobrevivido a la Batalla Pokemon!  *********\n\n", entrenador->nombre);
		nB = recv(entrenador->socket, mensaje, TAM_MENSAJE, 0);
		mensaje[nB] = '\0';
		printf("\n%d", nB);
		printf("\n%s\n\n", mensaje);
	}

	sprintf(aux, "cp %s%s \"%s\"", rutaPokeDex, mensaje, entrenador->dirBill);
	system(aux);
//	copiar(mensaje, entrenador->dirBill);
	return 0;
}
int jugarObjetivo() {
	time_t tBloqueado;
	printf("\n\n\nObjetivo: %c", entrenador->obj.id);
	fflush(stdout);
	obtenerCoordenadas();
	irPosicionPokenest();
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
	printf("\nMapa en Juego : %s", mapa->nombre);
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
	printf("\nVidas Disponibles : %d", entrenador->vidasDisponibles);
	fflush(stdout);
	tMapa *mapa;
	int mapasJugados = 0;
	while( mapasJugados < list_size(entrenador->mapas) ) {
		mapa = list_get(entrenador->mapas, mapasJugados);
		if( jugarMapa(mapa) ) {
			muerte();
			return 1;
		}
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
	rutaPokeDex = realpath(argv[2], NULL);
	getEntrenador(argv[1]);
/**************************************************   SECCION SEÑALES   ****************************************************************************/
	signal(SIGUSR1, signalRutina);
	signal(SIGTERM, signalRutina);
/**************************************************  SECCION PROGRAMA   ****************************************************************************/
	/******** MODIFICACION CONSOLA ********/
	struct termios oldt, newt;
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
		printf("\n\t\t\t**********  BIENVENIDO AL JUEGO %s  **********", entrenador->nombre);
		printf("\n\nNumero de Reintentos : %d", entrenador->reintentos);
		entrenador->vidasDisponibles = entrenador->vidas;
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
			borrar();
			sinVidas();
		}
		jugar = getCaracter("\n\nPresione R para reintentar, cualquier otra tecla para salir...");
		entrenador->reintentos++;
	}
	free(entrenador);
	/******** RESTAURACION CONSOLA ********/
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	/**************************************/
	system("setterm -cursor on");
	return EXIT_SUCCESS;
}
