#include <stdio.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <commons/log.h>
#include "pokeServ.h"

fs_osada_t fs_tmp;

static int osada_getattr(const char *path, struct stat *stbuf)
{
	struct fuse_context* context = fuse_get_context();

	memset(stbuf, 0, sizeof(struct stat));

	log_info(fs_tmp.log, "OSADA getattr: %s", path);

	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje

	if(memcmp(path, "/", 2) == 0) {
	        stbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	        stbuf->st_nlink = 2;
	        stbuf->st_uid = context->uid;
	        stbuf->st_gid = context->gid;
	        return 0;
	}

	int i=0, pos=0, parent = 0xFFFF;
	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	//  Valido Path
	if(!strcmp((char *)path2, "")){
		log_info(fs_tmp.log, "Carpeta en el raiz");
		parent = 0xFFFF;
	}else{
		log_info(fs_tmp.log, "Sub-Carpeta");
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "No existe la ruta indicada");
			return -ENOENT;
		}
	}

	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){
			if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir)) && (fs_tmp.file_table[i].parent_directory == parent) && (fs_tmp.file_table[i].state == DIRECTORY)){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			log_error(fs_tmp.log, "No existe la carpeta");
			return -ENOENT;
		}
		pos = i;
	}
	stbuf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	stbuf->st_nlink = 1;
	stbuf->st_uid = context->uid;
	stbuf->st_gid = context->gid;
	stbuf->st_size = fs_tmp.file_table[i].file_size;
	stbuf->st_blksize = OSADA_BLOCK_SIZE;
	stbuf->st_blocks = (fs_tmp.file_table[i].file_size / OSADA_BLOCK_SIZE) + 1;

    if (fs_tmp.file_table[i].state == DIRECTORY)
        stbuf->st_mode |= S_IFDIR;
    else stbuf->st_mode |= S_IFREG;

    log_info(fs_tmp.log, "Fin getattr");
	return 0;

}

int is_parent(osada_file table[], char *path){
	int i=0;
	uint8_t *path2 = (uint8_t *)calloc(strlen(path) + 1, (size_t) 1);
    strcpy((char *)path2, path);
    int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
    nombre_dir[0] = '\0';
    nombre_dir++;

	if(!strcmp((char *)path2, "")){
    	while(i<MAX_FILES && 0 != strcmp((char *)table[i].fname, nombre_dir)){
    		i++;
		}
    	return i;
	}else{
		if ( is_parent(table, path2) == MAX_FILES ){
			return MAX_FILES;
		}else{
			while(i<MAX_FILES && 0 != strcmp((char *)table[i].fname, nombre_dir))
				i++;
			return i;
		}
	}
}

static int osada_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	struct fuse_context* context = fuse_get_context();
	int i=0, pos=0, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
    strcpy((char *)path2, path);
    int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
    nombre_dir[0] = '\0';
    nombre_dir++;

    //  Valido Path
    if(!strcmp((char *)path2, "")){
    	//printf("Carpeta en el raiz\n");
    	parent = 0xFFFF;
	}else{
		//printf("Sub-Carpeta\n");
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			//printf("\t No existe la ruta indicada.\n");
			return -1;
		}
	}

	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){
			if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir)) &&
					(fs_tmp.file_table[i].parent_directory == parent) &&
						(fs_tmp.file_table[i].state == DIRECTORY)){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			//printf("No existe la carpeta.\n");
			return -1;
		}
		pos = i;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, ".", NULL, 0);
	i=0;
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].parent_directory == pos && fs_tmp.file_table[i].state != DELETED){
			struct stat var_stat = {
				.st_mode = S_IRWXU | S_IRWXG | S_IRWXO,
				.st_nlink = 1,
				.st_uid = context->uid,
				.st_gid = context->gid,
				.st_size = fs_tmp.file_table[i].file_size,
				.st_blksize = OSADA_BLOCK_SIZE,
				.st_blocks = (fs_tmp.file_table[i].file_size / OSADA_BLOCK_SIZE) + 1
			};
			if (fs_tmp.file_table[i].state == DIRECTORY)
			            var_stat.st_mode |= S_IFDIR;
			else var_stat.st_mode |= S_IFREG;
			filler(buf,(char *) fs_tmp.file_table[i].fname, &var_stat, 0 );
		}
		i++;
	}

	return 0;

}

int osada_mkdir (const char *path, mode_t mode)
{
	log_info(fs_tmp.log, "OSADA mkdir: %s", path);

	int i=0, libre, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;
	if(strlen((char *)nombre_arch) > OSADA_FILENAME_LENGTH)
	{
		log_error(fs_tmp.log, "El nombre '%s' excede el limite de %d caracteres impuesto por el enunciado.", nombre_arch, OSADA_FILENAME_LENGTH);
		free(path2);
		return -EINVAL;
	}
	 //  Busco ubicacion del padre
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			//printf("\t No existe la ruta indicada.\n");
			return -1;
		}
	}
	// Busco posicion libre en la Tabla de Archivos
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].state == DELETED){
			libre = i;
			break;
		}
		i++;
	}
	if (i == MAX_FILES){
		log_error(fs_tmp.log, "No Hay espacio disponible en la Tabla de Archivos");
		return -1;
	}
	// Valido que no se repita el nombre
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_arch)) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == DIRECTORY)){
			log_error(fs_tmp.log, "Ya existe una carpeta con el nombre \"%s\" en \"%s\".\n",nombre_arch, path2);
			return -1;
		}
		i++;
	}

	// Inserto el nuevo directorio en la Tabla de Archivos
	fs_tmp.file_table[libre].state = DIRECTORY;
	strcpy(fs_tmp.file_table[libre].fname, nombre_arch);
	fs_tmp.file_table[libre].parent_directory = parent;
	fs_tmp.file_table[libre].file_size = 0;
	time((time_t *)&fs_tmp.file_table[libre].lastmod);
	fs_tmp.file_table[libre].first_block = 0;

	return 0;
}

int osada_rmdir (const char *path)
{
	log_info(fs_tmp.log, "OSADA rmdir: %s", path);
	int i=0, pos, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	/* Valido que exista el path */
	if(!strcmp((char *)path2, "")){
		log_info(fs_tmp.log, "Carpeta en el raiz");
		parent = 0xFFFF;
	}else{
		log_info(fs_tmp.log, "Sub-carpeta");
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "No existe la ruta indicada");
			return -1;
		}
	}

	/* Busco la posicion en el vector de la tabla de archivos */
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir)) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == DIRECTORY)){
			break;
		}
		i++;
	}
	if ( i == MAX_FILES){
		log_error(fs_tmp.log, "No existe la carpeta a eliminar");
		return -1;
	}
	pos = i;

	/* Valido que el direcrorio a eliminar este vacio */
	i=0;
	while (i<MAX_FILES){
		if (fs_tmp.file_table[i].parent_directory == pos && fs_tmp.file_table[i].state != DELETED){
			log_error(fs_tmp.log, "La carpeta no esta vacia %d",pos);
			return -1;
		}
		i++;
	}

	/* Modifico datos en la Tabla de Archivos */
	fs_tmp.file_table[pos].state = DELETED;
	time((time_t *)&fs_tmp.file_table[pos].lastmod);
	log_info(fs_tmp.log,"Fin rmdir");
	return 0;
}

int osada_rename (const char *from, const char *to)
{
	log_info(fs_tmp.log, "OSADA rename: %s - %s", from, to);
	int i=0, pos, parent = 0xFFFF;

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

	/* Valido que exista el path */
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "No existe la ruta indicada");
			return -1;
		}
	}

	/* Busco la posicion en el vector de la tabla de archivos */
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, nombre_dir)) &&
				(fs_tmp.file_table[i].parent_directory == parent) ){
			break;
		}
		i++;
	}
	if ( i == MAX_FILES){
		log_error(fs_tmp.log, "No existe el archivo %d",i);
		return -1;
	}
	pos = i;

	strcpy((char *)fs_tmp.file_table[i].fname, nombre_new);
	time((time_t *)&fs_tmp.file_table[pos].lastmod);
	return 0;

}

static struct fuse_operations osada_oper = {
	.getattr = osada_getattr,
	.readdir   = osada_readdir,
	.rmdir     = osada_rmdir,
	.mkdir     = osada_mkdir,
	.rename    = osada_rename,
};

int main ( int argc , char * argv []) {
	printf("Pokedex Master!\n");

	fs_tmp.log = log_create("Debug//logFUSE.txt" , "PokedexServidor" , true , 0);

	FILE*	pokedex;
	if (NULL==(pokedex = fopen("pokedex2M","rb"))){
		printf("Error al cargar el disco Pokedex");
		return EXIT_FAILURE;
	}
	fread(fs_tmp.header.buffer, OSADA_BLOCK_SIZE, 1, pokedex);

	//init_file_table(fs_tmp.file_table);
	int i;
	for (i=0 ; i<MAX_FILES ; i++)
		fs_tmp.file_table[i].state = DELETED;
	// carpeta manual
	fs_tmp.file_table[0].state = DIRECTORY;
	strcpy(fs_tmp.file_table[0].fname, "manual");
	fs_tmp.file_table[0].parent_directory = 0xFFFF;
	fs_tmp.file_table[0].file_size = 0;
	time((time_t *)&fs_tmp.file_table[0].lastmod);
	fs_tmp.file_table[0].first_block = 0;
	// fin carpeta manual
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, NULL);
	return ret;
}
