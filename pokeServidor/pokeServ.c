#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <commons/log.h>
#include "pokeServ.h"
//#include "socks_fs.h"


fs_osada_t fs_tmp;
//lista_pokeCli *pokeClis;
void * pokedex, *dirBlock;
uint32_t data_offset;
int32_t dirPokedex;

void *procesar_cliente(void *socket);
void check_state();

/*
 * free_blocks: Devuelve la cantidad de bloques de datos libres
 */
uint32_t free_blocks(uint8_t *bitmap){
	uint32_t free = 0;
	int i=0;
	for (i=0; i<fs_tmp.header.data_blocks ; i++){
		if (bit_bitmap(bitmap, (i+data_offset)) == 0)
			free ++;
	}
	return free;
}

/*
 * osada_read: Funcion FUSE
 */
static int osada_read(const char *path, uint32_t size, uint32_t offset,
		osada_socket sock)
{
	log_info(fs_tmp.log, "OSADA read: %s-%d-%d", path, (int)offset, (int)size);
	//printf("  READ: %s\n",path);

	sem_wait(&fs_tmp.mux_osada);

	uint16_t pos = is_parent(fs_tmp.file_table,(uint8_t *) path);
	if (pos == MAX_FILES || fs_tmp.file_table[pos].state != REGULAR){
		log_error(fs_tmp.log, "  %s",strerror(ENOENT));
		//printf("fallo is parent  %s",strerror(ENOENT));
		//fflush(stdout);
		sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}
	sem_post(&fs_tmp.mux_osada);
	//if (size == 0) return 0;
	/*if (pthread_rwlock_tryrdlock(&fs_tmp.mux_files[pos]) != 0){
		return -EBUSY;
	}*/

	int j = 0;
	uint32_t otro = fs_tmp.file_table[pos].first_block;
	while (otro != 0xFFFFFFFF){
		//printf("  Control: %d-%X\n", j, otro);
		otro = fs_tmp.fat_osada[otro];
		j++;
	}
	
	if( fs_tmp.file_table[pos].file_size < offset ){
		//printf("\n---QUILOMBO EN LECTURA---\n");
		sleep(5);
	}
	if( fs_tmp.file_table[pos].file_size < (offset+size) ){
		size = fs_tmp.file_table[pos].file_size - offset;
	}

	osada_packet mensaje;
	uint32_t block_start = fs_tmp.file_table[pos].first_block;
	div_t block_offset = div(offset, OSADA_BLOCK_SIZE);
	uint32_t i =0;
	for ( i=0 ; i< block_offset.quot ; i++ ){
		block_start = fs_tmp.fat_osada[block_start];
	}
	int32_t copied=0, size_to_copy=0, copiedMSG=0;
	dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE + block_offset.rem;

	size_to_copy = (OSADA_BLOCK_SIZE - block_offset.rem);
	if (size < size_to_copy) size_to_copy = size;
	memcpy(mensaje.path, dirBlock , size_to_copy);
	//printf("mensaje: %s\n", mensaje.path);
	copied += size_to_copy;
	copiedMSG = size_to_copy;
	i=0;
	while (copied < size ){
		i += 1;
		if (i%4 ==0){
			// ENVIAR DATOS
			mensaje.type = OP_READ;
			mensaje.len = 290;
			mensaje.size = 1;//Mas datos
			mensaje.cod_return = copiedMSG;
			send_socket(&mensaje, sock);
			copiedMSG = 0;
		}
		if (block_start == 0xFFFFFFFF ) {
			//return copied;
			break;
		}
		block_start = fs_tmp.fat_osada[block_start];
		if ((size - copied) < OSADA_BLOCK_SIZE)
			size_to_copy = (size - copied);
		else
			size_to_copy = OSADA_BLOCK_SIZE;

		dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE;
		memcpy(mensaje.path + copiedMSG, dirBlock , size_to_copy);
		//printf("mensaje: %s\n", mensaje.path);
		copied = copied + size_to_copy;
		copiedMSG = copiedMSG + size_to_copy;
	}
	if (i==0){
		mensaje.type = OP_READ;
		mensaje.len = 290;
		mensaje.size = 0;//NO mas datos
		mensaje.cod_return = copied;
		send_socket(&mensaje, sock);
	}
	else
		if(copiedMSG != 0){
			mensaje.type = OP_READ;
			mensaje.len = 290;
			mensaje.size = 0;//NO mas datos
			mensaje.cod_return = copiedMSG;
			send_socket(&mensaje, sock);
		}
	//pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);
	return copied;
}

static int osada_getattr(osada_packet *mensaje)
{
	log_info(fs_tmp.log, "  GETATTR %s", mensaje->path);
//printf("  GETATTR %s\n", mensaje->path);
	int i=0, pos, parent = 0xFFFF;
	int8_t *path2 = calloc(strlen((char *)mensaje->path) + 1, 1);
	strcpy((char *)path2, (char *)mensaje->path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;


	//sem_wait(&fs_tmp.mux_osada);
/*
 * Desde aca comienza la validacion que el PATH sea correcto
 * y recupera la posicion en el vector de la tabla
 */
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table, path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "    No existe la ruta indicada");
			//printf("    No existe la ruta indicada");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){

			if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir, 17))
			//if ( (0 == memcmp(fs_tmp.file_table[i].fname, nombre_dir, strlen(nombre_dir)))
					&& (fs_tmp.file_table[i].parent_directory == parent)
					&& (fs_tmp.file_table[i].state != DELETED)
				){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			//printf("    No existe Archivo o Carpeta\n");
			log_error(fs_tmp.log, "    No existe la carpeta o archivo");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
		pos = i;
	}
	//sem_post(&fs_tmp.mux_osada);
 /* Termina la validacion
 * Esto deberia copiarse en una funcion que devuelta el numero del vector (pos)
 * o el error correspondiente.
 */
	//pthread_rwlock_rdlock(&fs_tmp.mux_files[pos]);
	mensaje->type = OP_GETATTR;
	mensaje->len = 17;
	mensaje->size = fs_tmp.file_table[pos].file_size;
	mensaje->lastmod = fs_tmp.file_table[pos].lastmod;
	mensaje->file_state = fs_tmp.file_table[pos].state;
	mensaje->cod_return = 0;
	//pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);

	return 0;
}

int is_parent(osada_file table[], int8_t *path){
	int i=0;
	uint16_t parent;
	////log_info(fs_tmp.log, "       PARENT: %s", path);
	int8_t *path2 = calloc(strlen((char *)path) + 1, (size_t) 1);
    strcpy((char *)path2, (char *)path);
    int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
    nombre_dir[0] = '\0';
    nombre_dir++;
	//printf("\n\nPath: (%s) - File: (%s)\n",path2, nombre_dir);
	if(!strcmp((char *)path2, "")){
    	while(i<MAX_FILES && 
			  (0 != strncmp((char *)table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH) 
			  || table[i].parent_directory != 0xFFFF)){
    		//printf("    File: (%s)\n",table[i].fname);
			i++;
		}
    	return i;
	}else{
		parent = is_parent(table, path2);
		if ( parent == MAX_FILES ){
			return MAX_FILES;
		}else{
			while(i<MAX_FILES && (0 != strncmp((char *)table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH) ||
					parent != table[i].parent_directory))
				i++;
			return i;
		}
	}
}

static int osada_readdir(osada_packet mens, osada_socket sock)
{
	log_info(fs_tmp.log, "Osada READDIR: %s - ", mens.path);
	//printf("\nREADDIR: %s", mens.path);fflush(stdout);
	//int i=0, pos=0, parent = 0xFFFF;

	/*int8_t *path2 = calloc(strlen((char *)mens.path) + 1, 1);
	strcpy((char *)path2, (char *)mens.path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
    nombre_dir++;

    
    *
	 * Aca se repite el proceso de validacion
	 *
	    sem_wait(&fs_tmp.mux_osada);
		if(!strcmp((char *)path2, "")){
	    	parent = 0xFFFF;
		}else{
			parent = is_parent(fs_tmp.file_table,path2);
			if (parent == MAX_FILES){
				//log_error(fs_tmp.log, "No existe la ruta indicada");
				sem_post(&fs_tmp.mux_osada);
				return -ENOENT;
			}
		}

		// Busco la posicion en el vector de la tabla de archivos
		i=0;
		if(!strcmp((char *)nombre_dir, "")){
			pos = parent;
		}else {
			while (i<MAX_FILES){
				if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH)) &&
				//if ( (0 == memcmp(fs_tmp.file_table[i].fname, nombre_dir, strlen(nombre_dir))) &&
						(fs_tmp.file_table[i].parent_directory == parent) &&
							(fs_tmp.file_table[i].state == DIRECTORY)){
					break;
				}
				i++;
			}
			if ( i == MAX_FILES){
				//log_error(fs_tmp.log, "No existe la carpeta");
				sem_post(&fs_tmp.mux_osada);
				return -ENOENT;
			}
			pos = i;
		}
		sem_post(&fs_tmp.mux_osada);
	*
	 * Termina proceso de validacion
	 */
	
	//sem_wait(&fs_tmp.mux_osada);
	uint16_t pos ;
	
	if(!strcmp((char *)mens.path, "/")){
		pos = 0xFFFF;
		//printf("Carpeta Raiz - ");fflush(stdout);
		
	}else{
		pos = is_parent(fs_tmp.file_table,(uint8_t *) mens.path);
		if (pos == MAX_FILES || fs_tmp.file_table[pos].state == DELETED){
			log_error(fs_tmp.log, "  %s",strerror(ENOENT));
			//printf("    Error READDIR: %s\n",strerror(ENOENT));fflush(stdout);
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
	//sem_post(&fs_tmp.mux_osada);
	//printf("Carpeta Pos: %d - ", pos);fflush(stdout);
	
	/*if (pos != 0xFFFF)
		if (fs_tmp.file_table[pos].state == REGULAR){
			printf("    Error READDIR: %s\n",strerror(EISNAM));
			return -EISNAM;
		}*/
	
	int i=0;
	uint32_t files[2048],cant=0;
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].parent_directory == pos && fs_tmp.file_table[i].state != DELETED){
			files[cant] = i;
			cant ++;
			//printf("\n    Pos[%d]: %s - State %d - Parent: %X\n", i, fs_tmp.file_table[i].fname, fs_tmp.file_table[i].state, fs_tmp.file_table[i].parent_directory);
		}
		i++;
	}
	int sended = 0;
	//printf("    Total Files: %d\n", cant);fflush(stdout);
	osada_packet mensaje;
	for (i=0 ; i<cant ; i++){
		mensaje.type = OP_READDIR;
		mensaje.len = 34;
		mensaje.cod_return = 0;
		mensaje.size = fs_tmp.file_table[files[i]].file_size;
		mensaje.offset = cant; // Uso para informar que hay mas registros para enviar
		mensaje.lastmod = fs_tmp.file_table[files[i]].lastmod;
		strncpy((char *)mensaje.fname, (char *)fs_tmp.file_table[files[i]].fname, OSADA_FILENAME_LENGTH);
		mensaje.file_state = fs_tmp.file_table[files[i]].state;
		sended = send_socket(&mensaje, sock);
		//printf("Send(%d): %d - ",sended, i);fflush(stdout);
		//usleep(5000);
		//printf("Paso Pausa\n");fflush(stdout);
	}
	if (cant == 0){
		//printf("Envio carpeta vacia - ");fflush(stdout);
		mensaje.type = OP_READDIR;
		mensaje.len = 13;
		mensaje.cod_return = 0;
		mensaje.file_state = DELETED;
		mensaje.size = 0;
		mensaje.offset = 0;
		sended = send_socket(&mensaje, sock);
		//printf("Send(%d)",sended);fflush(stdout);
	}
	//printf("Fin ReadDir\n");fflush(stdout);
	return 0;
}

int osada_mkdir (const char *path)
{
	log_info(fs_tmp.log, "OSADA mkdir: %s", path);
	//printf("  MKDIR %s - ", path);
	int i=0, libre, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;
	//printf("    Len: %d", strlen((char *)nombre_arch));
	if(strlen((char *)nombre_arch) > OSADA_FILENAME_LENGTH)
	{
		log_error(fs_tmp.log, "El nombre '%s' excede el limite de %d caracteres impuesto por el enunciado.", nombre_arch, OSADA_FILENAME_LENGTH);
		free(path2);
		return -ENAMETOOLONG;
	}
	//sem_wait(&fs_tmp.mux_osada);
/*
 * Comienza proceso de validacion, en este caso solo valida el PATH
 */
	 //  Busco ubicacion del padre
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "  %s", strerror(ENOENT));
			free(path2);
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
/*
 * Fina validacion
 */
	sem_wait(&fs_tmp.mux_osada);
	// Busco posicion libre en la Tabla de Archivos
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].state == DELETED){
			libre = i;
			break;
		}
		i++;
	}
	if (i == MAX_FILES){
		//log_error(fs_tmp.log, "No Hay espacio disponible en la Tabla de Archivos");
		free(path2);
		sem_post(&fs_tmp.mux_osada);
		return -ENOSPC;
	}
	
	
	// Valido que no se repita el nombre
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_arch, OSADA_FILENAME_LENGTH)) &&
		//if ( (0 == memcmp(fs_tmp.file_table[i].fname, nombre_arch, strlen(nombre_arch))) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == DIRECTORY)){
			log_error(fs_tmp.log, "Ya existe una carpeta con el nombre \"%s\" en \"%s\".\n",nombre_arch, path2);
			sem_post(&fs_tmp.mux_osada);
			return -EEXIST;
		}
		i++;
	}

	// Inserto el nuevo directorio en la Tabla de Archivos
	fs_tmp.file_table[libre].state = DIRECTORY;
	strncpy((char *)fs_tmp.file_table[libre].fname, (char *)nombre_arch, OSADA_FILENAME_LENGTH);
	fs_tmp.file_table[libre].parent_directory = parent;
	fs_tmp.file_table[libre].file_size = 0;
	time((time_t *)&(fs_tmp.file_table[libre].lastmod));
	fs_tmp.file_table[libre].first_block = 0xFFFFFFFF;
	sem_post(&fs_tmp.mux_osada);
	
	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	//printf("Fin MKDIR\n");
	return 0;
}

int osada_rmdir (const char *path)
{
	log_info(fs_tmp.log, "OSADA rmdir: %s", path);
	//printf("RMDIR %s - ", path);
	int i=0, pos, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;
/*
 * Comienza proceso de validacion
 */
	//sem_wait(&fs_tmp.mux_osada);
	/* Valido que exista el path */
	if(!strcmp((char *)path2, "")){
		//log_info(fs_tmp.log, "Carpeta en el raiz");
		parent = 0xFFFF;
	}else{
		//log_info(fs_tmp.log, "Sub-carpeta");
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "  %s", strerror(ENOENT));
			//printf("No existe la ruta indicada");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}

	/* Busco la posicion en el vector de la tabla de archivos */
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH)) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == DIRECTORY)){
			break;
		}
		i++;
	}
	if ( i == MAX_FILES){
		//printf("No existe la carpeta a eliminar");
		//sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}
	pos = i;
/*
 * Fin proceso validacion
 */
	/* Valido que el direcrorio a eliminar este vacio */
	i=0;
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].parent_directory == pos && fs_tmp.file_table[i].state != DELETED){
			log_error(fs_tmp.log, "  %s", strerror(ENOTEMPTY));
			//printf("La carpeta no esta vacia %d",pos);
			//sem_post(&fs_tmp.mux_osada);
			return -ENOTEMPTY;
		}
		i++;
	}
	/* Modifico datos en la Tabla de Archivos */
	fs_tmp.file_table[pos].state = DELETED;

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	//sem_post(&fs_tmp.mux_osada);
	//printf("Fin RMDIR\n");
	return 0;
}

int osada_rename (const char *from, const char *to)
{
	log_info(fs_tmp.log, "OSADA rename: %s - %s", from, to);
	//printf("RENAME: %s - %s - ", from, to); fflush(stdout);
	int i =0;
	uint32_t posFrom, posTo, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(from) + 1, 1);
	strcpy((char *)path2, from);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	int8_t *pathnew = calloc(strlen(to) + 1, 1);
	strcpy((char *)pathnew, to);
	int8_t *nombre_new = (int8_t *)strrchr((char *)pathnew, '/');
	nombre_new[0] = '\0';
	nombre_new++;

	if(strlen((char *)nombre_new) > OSADA_FILENAME_LENGTH)
	{
		log_error(fs_tmp.log, "El nombre '%s' excede el limite de %d caracteres impuesto por el enunciado.",
				nombre_new, OSADA_FILENAME_LENGTH);
		free(path2);
		return -ENAMETOOLONG;
	}
	//printf("Largo Correcto - "); fflush(stdout);
/*
 * Comienza proceso de validacion
 *
	//sem_wait(&fs_tmp.mux_osada);
	* Valido que exista el path 
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			//log_error(fs_tmp.log, "No existe la ruta indicada");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}

	* Busco la posicion en el vector de la tabla de archivos 
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH)) &&
		//if ( (0 == memcmp(fs_tmp.file_table[i].fname, nombre_dir, strlen(nombre_dir))) &&
				(fs_tmp.file_table[i].parent_directory == parent) ){
			break;
		}
		i++;
	}
	if ( i == MAX_FILES){
		//log_error(fs_tmp.log, "No existe el archivo %d",i);
		//sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}
	pos = i;
*
 * Fin proceso de validacion
 */
	if(!strcmp((char *)from, "/")){
		posFrom = 0xFFFF;
		//printf("Carpeta Raiz - ");fflush(stdout);
		
	}else{
		posFrom = is_parent(fs_tmp.file_table,(uint8_t *) from);
		if (posFrom == MAX_FILES || fs_tmp.file_table[posFrom].state == DELETED){
			log_error(fs_tmp.log, "  %s",strerror(ENOENT));
			//printf("    Error RENAME: %s\n",strerror(ENOENT));fflush(stdout);
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
	//printf("From OK - "); fflush(stdout);
	// Busco que exista la ruta destino
	if(!strcmp((char *)pathnew, "")){
		posTo = 0xFFFF;
		//printf("Carpeta Raiz - ");fflush(stdout);
		
	}else{
		//printf("Antes Parten %s - ",pathnew); fflush(stdout);
		posTo = is_parent(fs_tmp.file_table,(uint8_t *) pathnew);
		//printf("Despues Parent %d - ",posTo); fflush(stdout);
		if (posTo == MAX_FILES || fs_tmp.file_table[posTo].state == DELETED){
			log_error(fs_tmp.log, "  %s",strerror(ENOENT));
			//printf("    Error RENAME: %s\n",strerror(ENOENT));fflush(stdout);
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
	//printf("To OK - "); fflush(stdout);
	i=0;
	while (i < MAX_FILES){
		if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_new, OSADA_FILENAME_LENGTH)) &&
				(fs_tmp.file_table[i].parent_directory == posTo) &&
					(fs_tmp.file_table[i].state != DELETED)){
			//printf("Ya esxiste\n");
			return -EEXIST;
		}
		i++;
	}
	//printf("No se Repite - "); fflush(stdout);
	//printf("  Parent: %X \n",fs_tmp.file_table[pos].parent_directory);
	
	if (pthread_rwlock_trywrlock(&fs_tmp.mux_files[posFrom]) != 0 )
		return -EBUSY;
	strncpy((char *)fs_tmp.file_table[posFrom].fname, (char *)nombre_new, OSADA_FILENAME_LENGTH);
	fs_tmp.file_table[posFrom].parent_directory = posTo;
	time((time_t *)&(fs_tmp.file_table[posFrom].lastmod));
	pthread_rwlock_unlock(&fs_tmp.mux_files[posFrom]);
	//printf("Grabo memoria - "); fflush(stdout);
	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	//sem_post(&fs_tmp.mux_osada);
	//printf("  Parent: %X \n",fs_tmp.file_table[posFrom].parent_directory);
	//printf("Fin RENAME\n");
	return 0;
}

static int osada_release(const char *path)
{
	log_info(fs_tmp.log, "OSADA release: %s", path);
	//printf("  RELEASE: %s - ", path);
	int i=0, pos=0, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;
/*
 * comienza proceso de validacion
 */
	//sem_wait(&fs_tmp.mux_osada);
	//  Valido Path
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			//log_error(fs_tmp.log, "No existe la ruta indicada");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}

	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){
			if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir,OSADA_FILENAME_LENGTH)) &&
					(fs_tmp.file_table[i].parent_directory == parent) &&
						(fs_tmp.file_table[i].state == REGULAR)){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			//log_error(fs_tmp.log, "    No existe el archivo");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
		pos = i;
	}
	//sem_post(&fs_tmp.mux_osada);
	/*printf("\n Antes RELEASE   Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
			fs_tmp.mux_files[pos].__data.__writer);*/

	pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);

	/*printf("\n Despues RELEASE   Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
			fs_tmp.mux_files[pos].__data.__writer);*/
	//printf("Fin RELEASE\n");
	return 0;
}
static int osada_open(const char *path, int flags)
{
	log_info(fs_tmp.log, "OSADA open: %s", path);
	//printf("OPEN: %s - %X - ", path, flags);
	int i=0, pos=0, parent = 0xFFFF, ret=0;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;
/*
 * comienza proceso de validacion
 */
	//sem_wait(&fs_tmp.mux_osada);
	//  Valido Path
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			//log_error(fs_tmp.log, "No existe la ruta indicada");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}

	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){
			if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir, OSADA_FILENAME_LENGTH)) &&
					(fs_tmp.file_table[i].parent_directory == parent) &&
						(fs_tmp.file_table[i].state == REGULAR)){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			//log_error(fs_tmp.log, "    No existe el archivo");
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
		pos = i;
	}
	//sem_post(&fs_tmp.mux_osada);
	/*printf("\n Antes OPEN    Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
				fs_tmp.mux_files[pos].__data.__writer);*/
	if (flags == 0){
		//printf("    Lectura - ");
		if (pthread_rwlock_tryrdlock(&fs_tmp.mux_files[pos]) != 0 )
			return -EBUSY;
	}else{
		if (pthread_rwlock_trywrlock(&fs_tmp.mux_files[pos]) != 0)
			return -EBUSY;
		if(flags == 2){
			//printf("    Trunc 0 - ");
			ret = osada_ftruncate(path, 0, 0);
			if (ret<0)
				return ret;
		}/*else{
			printf("    Escritura - ");
		}*/
	}
	/*printf("\n Despues OPEN    Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
				fs_tmp.mux_files[pos].__data.__writer);*/

/*
 * fin proceso de validacion
 */
	//printf("Fin OPEN\n");
	return 0;
}

int osada_create (char *path)
{
	log_info(fs_tmp.log, "OSADA create: %s", path);
	//printf("CREATE %s - ", path);	fflush(stdout);
	int i=0, libre, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;

	if(strlen((char *)nombre_arch) > OSADA_FILENAME_LENGTH)
	{
		log_error(fs_tmp.log, "  %s", strerror(ENAMETOOLONG));
		free(path2);
		return -ENAMETOOLONG;
	}
/*
 * comienza proceso de validacion
 */
	//sem_wait(&fs_tmp.mux_osada);
	 //  Busco ubicacion del padre
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "  %s", strerror(ENOENT));
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
/*
 * Fin proceso validacion, solo PATH
 */
	// Busco posicion libre en la Tabla de Archivos
	sem_wait(&fs_tmp.mux_osada);
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].state == DELETED){
			libre = i;
			break;
		}
		i++;
	}
	if (i == MAX_FILES){
		log_error(fs_tmp.log, "  %s", strerror(ENOSPC));
		sem_post(&fs_tmp.mux_osada);
		return -ENOSPC;
	}
	// Valido que no se repita el nombre
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strncmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_arch, OSADA_FILENAME_LENGTH)) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == REGULAR)){
			log_error(fs_tmp.log, "  %s", strerror(EEXIST));
			sem_post(&fs_tmp.mux_osada);
			return -EEXIST;
		}
		i++;
	}
	// Pido bloque de datos libre
	int32_t first_block = free_bit_bitmap(fs_tmp.bitmap);
	if (first_block == 0xFFFFFFFF){
		log_error(fs_tmp.log, "  %s", strerror(ENOSPC));
		sem_post(&fs_tmp.mux_osada);
		return -ENOSPC;
	}
	// Inserto el nuevo archivo en la Tabla de Archivos
	fs_tmp.file_table[libre].state = REGULAR;
	strncpy((char *)fs_tmp.file_table[libre].fname, (char *)nombre_arch, OSADA_FILENAME_LENGTH);
	//memcpy(fs_tmp.file_table[libre].fname, nombre_arch, OSADA_FILENAME_LENGTH);
	fs_tmp.file_table[libre].parent_directory = parent;
	fs_tmp.file_table[libre].file_size = 0;
	time((time_t *)&(fs_tmp.file_table[libre].lastmod));
	set_bitmap(fs_tmp.bitmap, first_block + data_offset);
	fs_tmp.file_table[libre].first_block = first_block;
	fs_tmp.fat_osada[first_block] = 0xFFFFFFFF;
	// Actualizo BitMap
	dirBlock = pokedex + OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
	msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
	// Actualizo File Table
	dirBlock = pokedex + (1+fs_tmp.header.bitmap_blocks)*OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	// Actualizo FAT
	dirBlock = pokedex + fs_tmp.header.allocations_table_offset * OSADA_BLOCK_SIZE;
	memcpy(dirBlock, fs_tmp.fat_osada, (fs_tmp.header.data_blocks * 4));
	msync(dirBlock, (fs_tmp.header.data_blocks * 4), MS_ASYNC);
	sem_post(&fs_tmp.mux_osada);
	
	//printf("Fin CREATE\n");
	return 0;
}

int osada_statfs(osada_socket sock)
{
	//log_info(fs_tmp.log,"OSADA statfs");
	osada_packet mensaje;
	mensaje.type = OP_STATFS;
	mensaje.len = 13;
	mensaje.size = fs_tmp.header.fs_blocks;
	mensaje.file_state = DELETED;
	//sem_wait(&fs_tmp.mux_osada);
	mensaje.offset = free_blocks(fs_tmp.bitmap);
	mensaje.cod_return = 0;
	//sem_post(&fs_tmp.mux_osada);
	//printf("\nSTATFS: FS:%d - Free:%d de %d\n", mensaje.size, mensaje.offset, fs_tmp.header.data_blocks);
	send_socket(&mensaje, sock);
	//check_state();
	//check_mutex();
	return 0;
}

int osada_write (char *path, uint32_t size, uint32_t offset, char *buf)
{
	log_info(fs_tmp.log, "OSADA write: %s - Size:%d - Off:%d", path, size, offset);

	//sem_wait(&fs_tmp.mux_osada);
	uint16_t pos = is_parent(fs_tmp.file_table,(uint8_t *) path);
	if (pos == MAX_FILES || fs_tmp.file_table[pos].state == DELETED){
		log_error(fs_tmp.log, "  %s",strerror(ENOENT));
		//sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}
	//sem_post(&fs_tmp.mux_osada);
	if(fs_tmp.file_table[pos].file_size < (offset + size)){
		//log_trace(fs_tmp.log, "    size: %d trunc a: %d", fs_tmp.file_table[pos].file_size, (offset + size));
		int ret = osada_ftruncate (path, ((offset) + (size)), 1);
		if (ret != 0){
			log_error(fs_tmp.log, "  %s",strerror(-ret));
			return ret;
		}
		//log_trace(fs_tmp.log, "    size: %d", fs_tmp.file_table[pos].file_size);
	}

	//pthread_rwlock_wrlock(&fs_tmp.mux_files[pos]);

	uint32_t block_start = fs_tmp.file_table[pos].first_block;
	div_t block_offset = div(offset, OSADA_BLOCK_SIZE);
	//printf("    Block Offset:%X\n",block_offset);
	//printf("    Block 0:%X\n",block_start);
	uint32_t i =0;
	for ( i=0 ; i < block_offset.quot ; i++ ){
		block_start = fs_tmp.fat_osada[block_start];
		//printf("    Block %d:%X\n", i+1, block_start);
	}
	//sem_wait(&fs_tmp.mux_osada);
	size_t copied=0, size_to_copy=0;
	size_to_copy = (OSADA_BLOCK_SIZE - block_offset.rem);
	if (size < size_to_copy) size_to_copy = size;
	// Actualizo Bloque de Datos
	dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE + block_offset.rem;
	memcpy(dirBlock, buf, size_to_copy);
	msync(dirBlock, OSADA_BLOCK_SIZE, MS_ASYNC);
	copied += size_to_copy;
	//i=0;
	uint32_t new_block;
	while (copied < size ){
		i ++;
		if (fs_tmp.fat_osada[block_start] == 0xFFFFFFFF){
			//printf(" ------------- \n");
			//printf(" -- No deberia haber entrado NUNCA -- \n");
			//printf(" ------------- \n");
			//Inserto nuevo bloque de datos
			sem_wait(&fs_tmp.mux_osada);
			new_block = free_bit_bitmap(fs_tmp.bitmap);
			if (new_block == 0xFFFFFFFF){
				pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);
				return -ENOSPC;
			}
			set_bitmap(fs_tmp.bitmap, new_block + data_offset);
			sem_post(&fs_tmp.mux_osada);
			fs_tmp.fat_osada[block_start] = new_block;
			fs_tmp.fat_osada[new_block] = 0xFFFFFFFF;
			// Actualizo BitMap
			dirBlock = pokedex + OSADA_BLOCK_SIZE ;
			memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
			msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
			// Actualizo FAT
			dirBlock = pokedex + fs_tmp.header.allocations_table_offset * OSADA_BLOCK_SIZE;
			memcpy(dirBlock, fs_tmp.fat_osada, (fs_tmp.header.data_blocks * 4));
			msync(dirBlock, (fs_tmp.header.data_blocks * 4), MS_ASYNC);
		}
		block_start = fs_tmp.fat_osada[block_start];
		//printf("    Block %d:%X\n", i, block_start);
		if ((size - copied) < OSADA_BLOCK_SIZE)
			size_to_copy = (size - copied);
		else
			size_to_copy = OSADA_BLOCK_SIZE;
		// Actualizo Bloque de Datos
		dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE;
		memcpy(dirBlock, buf + copied , size_to_copy);
		msync(dirBlock, size_to_copy, MS_ASYNC);
		copied += size_to_copy;
	}
	//log_trace(fs_tmp.log, "    Write:copied: %d\n",copied);

	
	//pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);
	return copied;
}

int osada_ftruncate (const char * path, off_t offset, int flag)
{
	log_info(fs_tmp.log, "OSADA ftruncate: %s-%d", path, (int)offset);
	//printf("FTRUNCATE: %s - Off:%d - ", path, (int)offset);
	
	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;

	//sem_wait(&fs_tmp.mux_osada);
	uint16_t pos = is_parent(fs_tmp.file_table,(uint8_t *) path);
	if (pos == MAX_FILES || fs_tmp.file_table[pos].state == DELETED){
		log_error(fs_tmp.log, "  %s",strerror(ENOENT));
		//sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}
	//sem_post(&fs_tmp.mux_osada);

	if(offset == fs_tmp.file_table[pos].file_size)
		return 0;

	/*printf("\n    Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
			fs_tmp.mux_files[pos].__data.__writer);*/

	/*if (flag == 0)
		if(pthread_rwlock_trywrlock(&fs_tmp.mux_files[pos]) != 0)
			return -EBUSY;*/

	uint32_t block_start, aux_block, size_file, off_size;

	if (fs_tmp.file_table[pos].file_size == 0)
		size_file = 1;
	else
		size_file = fs_tmp.file_table[pos].file_size;

	if (offset == 0)
		off_size = 1;
	else
		off_size = offset;

	div_t aux_new = div(off_size, OSADA_BLOCK_SIZE);
	div_t aux_old = div(size_file, OSADA_BLOCK_SIZE);
	int32_t block_abm = ((aux_new.rem)?aux_new.quot+1:aux_new.quot) -
						((aux_old.rem)?aux_old.quot+1:aux_old.quot);

	//printf("    Blocks New:%d\n",aux_new.quot);
	//printf("    Blocks Old:%d\n",aux_old.quot);
	if(block_abm > 0){
		uint32_t blocks_free = free_blocks(fs_tmp.bitmap);
		//log_trace(fs_tmp.log, "       Need:%d Free:%d",block_abm, blocks_free);
		if (block_abm > blocks_free){
			//log_error(fs_tmp.log, "No hay espacio suficiente. Need:%d Free:%d Blocks",block_abm, blocks_free);
			/*if(flag ==0)
				pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);*/
			return -EFBIG; //EFBIG - ENOSPC
		}
		while(block_abm){
			block_start = fs_tmp.file_table[pos].first_block;
			//printf("    Block x:%X\n", block_start);
			while (fs_tmp.fat_osada[block_start] != 0xFFFFFFFF){
				block_start = fs_tmp.fat_osada[block_start];
				//printf("    Block x:%X\n", block_start);
			}
			//log_trace(fs_tmp.log, "Insert nuevo bloque");
			//Inserto nuevo bloque de datos
			sem_wait(&fs_tmp.mux_osada);
			aux_block = free_bit_bitmap(fs_tmp.bitmap);
			//printf("    Bit libre: %x\n",aux_block);
			if (aux_block == 0xFFFFFFFF){
				//printf("    NO HAY ESPACIO!!!");
				//log_trace(fs_tmp.log, "No hay bitmap disponible");
				sem_post(&fs_tmp.mux_osada);
				/*if(flag ==0)
					pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);*/
				return -EFBIG;
			}
			set_bitmap(fs_tmp.bitmap, aux_block + data_offset);
			sem_post(&fs_tmp.mux_osada);
			fs_tmp.fat_osada[block_start] = aux_block ;
			fs_tmp.fat_osada[aux_block] = 0xFFFFFFFF;
			//printf("    Block x:%X - %X\n", fs_tmp.fat_osada[block_start], aux_block);
			// Actualizo BitMap
			dirBlock = pokedex + OSADA_BLOCK_SIZE ;
			memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
			msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
			// Actualizo FAT
			dirBlock = pokedex + fs_tmp.header.allocations_table_offset * OSADA_BLOCK_SIZE;
			memcpy(dirBlock, fs_tmp.fat_osada, (fs_tmp.header.data_blocks * 4));
			msync(dirBlock, (fs_tmp.header.data_blocks * 4), MS_ASYNC);

			block_abm--;
		}
	}
	else if(block_abm < 0)
		while(block_abm){
			block_start = fs_tmp.file_table[pos].first_block;
			//log_trace(fs_tmp.log, "    FAT: %08X",block_start );
			while (fs_tmp.fat_osada[block_start] != 0xFFFFFFFF){
				aux_block = block_start;
				block_start = fs_tmp.fat_osada[block_start];
				//log_trace(fs_tmp.log, "    FAT: %08X",block_start );
			}
			if (block_start != fs_tmp.file_table[pos].first_block){
				//log_trace(fs_tmp.log, "Elimino bloque %08X",aux_block + data_offset);
				//Elimino ultimo bloque de datos
				sem_wait(&fs_tmp.mux_osada);
				clean_bitmap(fs_tmp.bitmap, block_start + data_offset);
				sem_post(&fs_tmp.mux_osada);
				fs_tmp.fat_osada[aux_block] = 0xFFFFFFFF;
				// Actualizo BitMap
				dirBlock = pokedex + OSADA_BLOCK_SIZE ;
				memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
				msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
				// Actualizo FAT
				dirBlock = pokedex + fs_tmp.header.allocations_table_offset * OSADA_BLOCK_SIZE;
				memcpy(dirBlock, fs_tmp.fat_osada, (fs_tmp.header.data_blocks * 4));
				msync(dirBlock, (fs_tmp.header.data_blocks * 4), MS_ASYNC);
			}/*else{
				printf("NO Elimino bloque");
			}*/
			block_abm++;
		}
	//log_trace(fs_tmp.log, "Blocks Libres: %d",free_blocks(fs_tmp.bitmap));
	fs_tmp.file_table[pos].file_size = offset;
	// Actualizo File Table
	dirBlock = pokedex + (1+fs_tmp.header.bitmap_blocks)*OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);

	/*printf("\n Antes FTRUNCATE    Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
			fs_tmp.mux_files[pos].__data.__writer);*/
	/*if(flag == 0)
		pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);*/
	/*printf("\n Despues FTRUNCATE    Readers: %d Writer: %d\n", fs_tmp.mux_files[pos].__data.__nr_readers,
			fs_tmp.mux_files[pos].__data.__writer);*/
	//printf("Fin FTRUNCATE\n");
	return 0;
}

int osada_truncate(const char * path, off_t offset) {
	// funcion dummy para que no se queje de "function not implemented"
	//log_info(fs_tmp.log, "DUMMY!!! OSADA truncate: %s", path);
	return 0;
}

int osada_unlink (const char *path)
{
	log_info(fs_tmp.log, "OSADA unlink: %s", path);
	//printf("UNLINK: %s - ",path);
	/*int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;*/
	uint32_t pos;
	//sem_wait(&fs_tmp.mux_osada);
	if(!strcmp((char *)path, "/")){
		pos = 0xFFFF;
		//printf("Raiz - ");
		
	}else{
		pos = is_parent(fs_tmp.file_table,(uint8_t *) path);
		//printf("\nunlink Pos(%x) = %s - State:%d", pos, fs_tmp.file_table[pos].fname, fs_tmp.file_table[pos].state);
		if (pos == MAX_FILES || fs_tmp.file_table[pos].state == DELETED){
			log_error(fs_tmp.log, "  %s",strerror(ENOENT));
			//printf("    Error: %s\n",strerror(ENOENT));
			//sem_post(&fs_tmp.mux_osada);
			return -ENOENT;
		}
	}
	//printf("Modifico State - ");
	/* Modifico datos en la Tabla de Archivos */
	if(pthread_rwlock_trywrlock(&fs_tmp.mux_files[pos]) != 0)
		return -EBUSY;
	fs_tmp.file_table[pos].state = DELETED;
	pthread_rwlock_unlock(&fs_tmp.mux_files[pos]);
	//time((time_t *)&fs_tmp.file_table[pos].lastmod);
	// liberar bloques en bitmap
	uint32_t blocks = fs_tmp.file_table[pos].first_block;
	while (blocks != 0xFFFFFFFF){
		sem_wait(&fs_tmp.mux_osada);
		clean_bitmap(fs_tmp.bitmap, (data_offset + blocks));
		sem_post(&fs_tmp.mux_osada);
		blocks = fs_tmp.fat_osada[blocks];
	}
	//printf("Liberoo Bitmap - ");
// Actualizo BitMap
	dirBlock = pokedex + OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
	msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
// Actualizo File Table
	dirBlock = pokedex + (1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	//sem_post(&fs_tmp.mux_osada);
	//printf("Fin UNLINK\n");
	return 0;
}

int osada_utimens(const char* path, uint32_t lastmod)
{
	log_info(fs_tmp.log, "OSADA UTIMENS: %s", path);
	//printf("  UTIMENS %s\n", path);

	/*int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;*/

	//sem_wait(&fs_tmp.mux_osada);
	uint16_t pos = is_parent(fs_tmp.file_table,(uint8_t *) path);
	if (pos == MAX_FILES || fs_tmp.file_table[pos].state == DELETED){
		log_error(fs_tmp.log, "  %s",strerror(ENOENT));
		//printf("fallo is parent  %s",strerror(ENOENT));
		//sem_post(&fs_tmp.mux_osada);
		return -ENOENT;
	}

	fs_tmp.file_table[pos].lastmod = lastmod;
	//Actualizo File Table
	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	//sem_post(&fs_tmp.mux_osada);
	return 0;
}

int set_bitmap(uint8_t *bitmap, uint32_t pos){
	/*
	 * Agrego validacion para asegurar su funcionamiento
	 */
	if (bit_bitmap(bitmap, pos) == 0)
		bitmap[pos/8] |= 1<<(7-(pos%8));
	else{
		//printf("ERROR - El Bit no esta libre\n");
		log_error(fs_tmp.log, "  El Bit no esta libre");
	}
	return 0;
}

int clean_bitmap(uint8_t *bitmap, uint32_t pos){
	/*
	 * Agrego validacion para asegurar su funcionamiento
	 */
	if (bit_bitmap(bitmap, pos) == 1)
		bitmap[pos/8] &= ~(1<<(7-(pos%8)));
	else{
		//printf("ERROR - El Bit no esta ocupado\n");
		log_error(fs_tmp.log, "  El Bit no esta ocupado");
	}
	return 0;
}

uint32_t bit_bitmap(uint8_t *bitmap, uint32_t pos){
	return ((bitmap[pos/8] & (1<<(7-(pos%8)))) ? 1 : 0);
}

/* VIEJO
int32_t free_bit_bitmap(uint8_t *bitmap, uint32_t size){
	int32_t pos = -1;
	int i=0, j=0;
	while (i < size){
		if (bitmap[i] != 0xFF){
			for(j=0; j< 8; j++){
				if (bit_bitmap(bitmap, i*8 + j) == 0)
					return (i*8+j);
			}
		}
		i++;
	}
	return pos;
}*/
int32_t free_bit_bitmap(uint8_t *bitmap){//, uint32_t size){
	int32_t pos = -1;
	int i=0, j=0;
	/* Saltear bloques Administrativos:
	 * - Data_Offset
	 */
	while (i < fs_tmp.header.data_blocks){
		if (bit_bitmap(bitmap, i+data_offset) == 0)
			return (i);
		i++;
	}
	return pos;
}


int main ( int argc , char * argv []) {
	printf("Pokedex Master!\n");
	char *disco = "Files//hdd.bin";
	fs_tmp.log = log_create("logServidor.txt" , "PokedexServidor" , false, LOG_LEVEL_TRACE);

	FILE*	pokedex1;
	if (NULL==(pokedex1 = fopen(argv[3],"rb"))){
		printf("Error al cargar el disco Pokedex");
		return -EXIT_FAILURE;
	}
	fread(fs_tmp.header.buffer, OSADA_BLOCK_SIZE, 1, pokedex1);
	fclose(pokedex1);

	printf("Datos FS:\n");
	printf("    FS Blocks:%d\n", fs_tmp.header.fs_blocks);
	printf("    BitMap Blocks:%d\n", fs_tmp.header.bitmap_blocks);
	printf("    Inicio FAT:%d\n", fs_tmp.header.allocations_table_offset);
	printf("    Data Blocks:%d\n", fs_tmp.header.data_blocks);

	dirPokedex = open(argv[3], O_RDWR); //O_RDWR
	pokedex = mmap(NULL, fs_tmp.header.fs_blocks * OSADA_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dirPokedex , 0);
	int32_t err = posix_madvise(pokedex, fs_tmp.header.fs_blocks * OSADA_BLOCK_SIZE, POSIX_MADV_RANDOM);
	if( err )
		printf("El madvise ha fallado. Número de error: %d \n", err);

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(&fs_tmp.file_table, dirBlock, MAX_FILES * sizeof(osada_file));

	fs_tmp.bitmap = calloc(fs_tmp.header.bitmap_blocks * OSADA_BLOCK_SIZE, sizeof(uint8_t));
	dirBlock = pokedex + OSADA_BLOCK_SIZE ;
	memcpy(fs_tmp.bitmap , dirBlock, OSADA_BLOCK_SIZE *
			fs_tmp.header.bitmap_blocks);

	fs_tmp.fat_osada = calloc((fs_tmp.header.data_blocks), sizeof(uint32_t));
	dirBlock = pokedex + fs_tmp.header.allocations_table_offset  * OSADA_BLOCK_SIZE ;
	memcpy(fs_tmp.fat_osada , dirBlock, (fs_tmp.header.data_blocks * 4));

	uint32_t fat_size_block = fs_tmp.header.fs_blocks - 1 - 1024 - fs_tmp.header.bitmap_blocks -
			fs_tmp.header.data_blocks;

	sem_init(&fs_tmp.mux_osada,0,1);
	int i;
	for (i=0;i<MAX_FILES;i++)
		pthread_rwlock_init(&fs_tmp.mux_files[i],NULL);

	data_offset  =	fs_tmp.header.allocations_table_offset + fat_size_block;

	printf("\nBloques libres: %d",free_blocks(fs_tmp.bitmap));

	printf("\nInicia SERVIDOR\n");

	osada_socket sockMaster;


	if((sockMaster = create_socket()) < 0) {
		printf("Error al crear el socket: %s", strerror(errno));
		//log_error(fs_tmp.log, "Error al crear el socket: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}

	if(bind_socket(sockMaster, (char *)argv[argc-3], atoi(argv[argc-2]))){
		printf("No se pudo preparar la conexion: %s", strerror(errno));
		//log_error(fs_tmp.log, "No se pudo preparar la conexion: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}

	listen_socket(sockMaster);

	printf("---------------------------------\n");
	printf("--- Socket escucha Master: %d ---\n",sockMaster);
	printf("-----%s--------------%d----------\n",argv[argc-3], atoi(argv[argc-2]));

//	TIPO USO MAPA

	pthread_attr_t attr;
	osada_socket sock_Cli;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	while( (sock_Cli = accept(sockMaster, NULL, NULL)) ) {
		if ( sock_Cli == -1 ) {
			perror("accept");
			break;
		}
		pthread_t nuevoCliente;
		if( pthread_create( &nuevoCliente, &attr, procesar_cliente, (void*) &sock_Cli) ){
			log_error(fs_tmp.log, "Error al crear nuevo hilo Cliente: %s", strerror(errno));
			printf("Error al crear nuevo hilo Cliente: %s\n", strerror(errno));
			perror("pthread_create");
		}
		else{
			log_info(fs_tmp.log, "Nuevo cliente conectado: %d", sock_Cli);
			printf("Nuevo cliente conectado: %d\n", sock_Cli);
		}
	}

	printf("----------------------\n Fin Proceso Servidor \n----------------------");

	return EXIT_SUCCESS;
}

/*
void liberar_cliente_caido(osada_socket sock)
{
	lista_pokeCli *aux_pokeClis, *anterior;
	aux_pokeClis = pokeClis;
	anterior=NULL;
	while(aux_pokeClis != NULL && aux_pokeClis->sock != sock)
	{
		anterior=aux_pokeClis;
		aux_pokeClis=aux_pokeClis->sgte;
	}
	close_socket(aux_pokeClis->sock);
	if(anterior == NULL)
		pokeClis = pokeClis->sgte;
	else
		anterior = anterior->sgte;
	free(aux_pokeClis);
}*/

void *procesar_cliente(void *socket) {
	osada_packet mensaje;
	osada_socket sockCli = *(int *)socket;
	int32_t res, cant;

	printf("Nueva conexion\n");
	if(recv_socket(&mensaje,sockCli)>0)
	{
		if(mensaje.type == OK)
		{
			printf("Nueva conexion Cliente: %d\n",sockCli);
			////log_info
			mensaje.type = OK;
			mensaje.len = 0;
			if((cant = send_socket(&mensaje,(osada_socket)sockCli))<0)
				printf("Error: %s\n", strerror(errno));
			printf("------------------------------\n");
		}
		else
		{
			printf("ATENCION!!! Formato de mensaje no reconocido para iniciar conexion");
			////log_warning
			printf("------------------------------\n");
			return EXIT_FAILURE;
		}
	}
	//char * buf;
	while(1){
		if(recv_socket(&mensaje, sockCli)<=0)
			break;

		switch(mensaje.type){
			case OK:
				printf("BASTA DE PEDIDO CONEXION!!!\n");
			break;
			case OP_GETATTR:
				//printf("GETATTR\n");
				if(mensaje.len > 0)
				{
					res = osada_getattr(&mensaje);
					if(res < 0){
						//printf("   Error: %s\n",strerror(-res));
						mensaje.type = OP_GETATTR;
						mensaje.len = 4;
						mensaje.cod_return = res;
					}
					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_READDIR:
				//printf("READDIR\n");
				if(mensaje.len > 0)
				{
					res = osada_readdir(mensaje, sockCli);
					if(res < 0){
						//printf("   Error: %s\n",strerror(-res));
						mensaje.type = OP_READDIR;
						mensaje.len = 4;
						mensaje.cod_return = res;
						send_socket(&mensaje, sockCli);
					}
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_MKDIR:
				//printf("MKDIR\n");
				if(mensaje.len > 0)
				{
					res = osada_mkdir((char *)mensaje.path);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_MKDIR;
					mensaje.len = 4;
					mensaje.cod_return = res;

					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_RMDIR:
				//printf("RMDIR\n");
				if(mensaje.len > 0)
				{
					res = osada_rmdir((char *)mensaje.path);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_RMDIR;
					mensaje.len = 4;
					mensaje.cod_return = res;

					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_RENAME:
				//printf("RENAME\n");
				if(mensaje.len > 0)
				{
					res = osada_rename((char *)mensaje.path, (char *)mensaje.pathto);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_RENAME;
					mensaje.len = 4;
					mensaje.cod_return = res;

					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_STATFS:
				//printf("STATFS\n");
				osada_statfs(sockCli);
			break;
			case OP_UTIMENS:
				//printf("UTIMENS\n");
				if(mensaje.len > 0)
				{
					res = osada_utimens((char *)mensaje.path, mensaje.lastmod);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_UTIMENS;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_OPEN:
				//printf("OPEN\n");
				if(mensaje.len > 0)
				{
					res = osada_open((char *)mensaje.path, mensaje.offset);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_OPEN;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_READ:
				//printf("READ\n");
				if(mensaje.len > 0)
				{
					res = osada_read((char *)mensaje.path, mensaje.size, mensaje.offset, sockCli);
					if(res < 0)
						printf("   Error: %s\n",strerror(-res));
					if(res < 0){
						mensaje.type = OP_READ;
						mensaje.len = 4;
						mensaje.cod_return = res;
						send_socket(&mensaje, sockCli);
					}
				}else{
					printf("   MANDASTE CUALQUIER COSA\n");
				}
			break;
			case OP_WRITE:
				//printf("WRITE - Size:%d - Off:%d\n",mensaje.size, mensaje.offset);
				if(mensaje.len == 546){
					//res = osada_write(mensaje);
					res = osada_write(mensaje.path, mensaje.size, mensaje.offset, mensaje.pathto);
					if (res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_WRITE;
					mensaje.len = 4;
					mensaje.cod_return = res;
					//printf("    Writed:%d\n",res);
					send_socket(&mensaje, sockCli);
				}
				else{printf("   MANDASTE CUALQUIER COSA len:%d\n",mensaje.len);}
			break;
			case OP_CREATE:
				//printf("CREATE: %s\n", mensaje.path);
				if(mensaje.len > 0){
					res = osada_create((char *)mensaje.path);
					if (res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_CREATE;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}
				else{printf("   MANDASTE CUALQUIER COSA len:%d\n",mensaje.len);}
			break;
			case OP_FTRUNCATE:
				//printf("FTRUNCATE\n");
				if(mensaje.len > 0){
					res = osada_ftruncate(mensaje.path, mensaje.offset, 0);
					if (res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_FTRUNCATE;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}
				else{printf("   MANDASTE CUALQUIER COSA\n");}
			break;
			case OP_RELEASE:
				//printf("RELEASE\n");
				if(mensaje.len > 0){
					res = osada_release(mensaje.path);
					if (res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_RELEASE;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}
				else{printf("   MANDASTE CUALQUIER COSA\n");}
			break;
			case OP_UNLINK:
				//printf("UNLINK\n");
				if(mensaje.len > 0){
					res = osada_unlink(mensaje.path);
					if (res < 0)
						printf("   Error: %s\n",strerror(-res));
					mensaje.type = OP_UNLINK;
					mensaje.len = 4;
					mensaje.cod_return = res;
					send_socket(&mensaje, sockCli);
				}
				else{printf("   MANDASTE CUALQUIER COSA\n");}
			break;
			default :
				printf("Funcion no implementada! Codigo:%d\n",mensaje.type);
			break;
		}
	}
	printf("-----------------\n Fin Hilo Cliente %d\n-----------------",sockCli);
	close_socket(sockCli);
	return EXIT_SUCCESS;
}
void check_state(){
	int32_t i=0, cantBlock=0, blockSize=0;
	uint32_t block;
	/*for (i=0 ; i<15 ; i++){
		printf("File[%d]: %d - %s - %X\n", i, fs_tmp.file_table[i].state, fs_tmp.file_table[i].fname, fs_tmp.file_table[i].parent_directory);
	}	*/
	for (i=0 ; i<2048 ; i++){
		//printf("File[%d]: %d - %s\n", i, fs_tmp.file_table[i].state, fs_tmp.file_table[i].fname);
		if (fs_tmp.file_table[i].state == REGULAR){
			cantBlock = 0;
			block = fs_tmp.file_table[i].first_block;
			while(block != 0xFFFFFFFF){
				//printf("\n%s - Block:%d - Pos:%08X", fs_tmp.file_table[i].fname, cantBlock, block);
				cantBlock ++;
				block = fs_tmp.fat_osada[block];
			}/*printf("\n%s - Block:%d - Pos:%08X", fs_tmp.file_table[i].fname, cantBlock, block);
			fflush(stdout);*/
			blockSize = fs_tmp.file_table[i].file_size/64;
			if (fs_tmp.file_table[i].file_size %64 != 0 || fs_tmp.file_table[i].file_size == 0)
				blockSize ++;
			if (cantBlock != blockSize){
				printf("Nombre: %s", fs_tmp.file_table[i].fname);
				printf(" Size: %dB BlockSize: %d", fs_tmp.file_table[i].file_size, blockSize );
				printf(" Blocks: %d\n", cantBlock);
			}
		}
	}
}
void check_mutex(){
	int i;
	for (i=0 ; i<15 ; i++ ){
		if (pthread_rwlock_trywrlock(&fs_tmp.mux_files[i]) != 0){
			printf("    File(%d): %s Bloqueado!\n", i, fs_tmp.file_table[i].fname);
		}else{
			pthread_rwlock_unlock(&fs_tmp.mux_files[i]);
		}
	}
}
