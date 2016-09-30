#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fuse.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <commons/log.h>
#include "pokeServ.h"

fs_osada_t fs_tmp;
void * pokedex, *dirBlock;
int32_t dirPokedex;

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
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "\t\tNo existe la ruta indicada");
			return -ENOENT;
		}
	}

	// Busco la posicion en el vector de la tabla de archivos
	i=0;
	if(!strcmp((char *)nombre_dir, "")){
		pos = parent;
	}else {
		while (i<MAX_FILES){
			if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir))
					&& (fs_tmp.file_table[i].parent_directory == parent)
					//&& (fs_tmp.file_table[i].state == DIRECTORY)
				){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			log_error(fs_tmp.log, "\t\tNo existe la carpeta o archivo");
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
    	parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "No existe la ruta indicada");
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
			log_error(fs_tmp.log, "No existe la carpeta");
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

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);

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

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);

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

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);

	return 0;
}

int osada_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();

	log_info(fs_tmp.log, "OSADA create: %s", path);

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
			log_error(fs_tmp.log, "No existe la ruta indicada");
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
					(fs_tmp.file_table[i].state == REGULAR)){
			log_error(fs_tmp.log, "Ya existe un archivo con el nombre \"%s\" en \"%s\".\n",nombre_arch, path2);
			return -1;
		}
		i++;
	}

	// Inserto el nuevo archivo en la Tabla de Archivos
	fs_tmp.file_table[libre].state = REGULAR;
	strcpy((char *)fs_tmp.file_table[libre].fname, (char *)nombre_arch);
	fs_tmp.file_table[libre].parent_directory = parent;
	fs_tmp.file_table[libre].file_size = 0;
	time((time_t *)&fs_tmp.file_table[libre].lastmod);
	fs_tmp.file_table[libre].first_block = 0; // asignar bloque libre segun bitmap
	// poner ocupado el bitmap
	// actualizar file Allocation Table

	//osada_open(path, fi);
	return 0;
}

static int osada_open(const char *path, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();

	log_info(fs_tmp.log, "OSADA open: %s", path);
	log_info(fs_tmp.log, "     Inicio   fi->fh: %d",fi->fh);
	int i=0, pos=0, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	//  Valido Path
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "No existe la ruta indicada");
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
						(fs_tmp.file_table[i].state == REGULAR)){
				break;
			}
			i++;
		}
		if ( i == MAX_FILES){
			log_error(fs_tmp.log, "        No existe el archivo");
			return -1;
		}
		pos = i;
	}

	fi->fh = i;
	log_info(fs_tmp.log, "        fi->fh: %d",fi->fh);
	return 0;
}

int osada_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	log_info(fs_tmp.log, "OSADA ftruncate: %s", path);
	log_info(fs_tmp.log, "\tfi->fh: %d", fi->fh);
	int i=0, pos=0, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;

	if (strcmp((char *)fs_tmp.file_table[fi->fh].fname, nombre_arch) != 0)
		return -EINVAL;

	if(offset == fs_tmp.file_table[fi->fh].file_size)
		 return 0;

	div_t aux_new = div(offset, OSADA_BLOCK_SIZE);
	div_t aux_old = div(fs_tmp.file_table[fi->fh].file_size, OSADA_BLOCK_SIZE);
	int32_t block_abm = ((aux_new.rem)?aux_new.quot+1:aux_new.quot) -
						((aux_old.rem)?aux_old.quot+1:aux_old.quot);

	return 0;
}

int osada_unlink (const char *path)
{
	struct fuse_context* context = fuse_get_context();
	log_info(fs_tmp.log, "OSADA unlink: %s", path);
	int i=0, pos, parent = 0xFFFF;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	/* Valido que exista el path */
	if(!strcmp((char *)path2, "")){
		parent = 0xFFFF;
	}else{
		parent = is_parent(fs_tmp.file_table,path2);
		if (parent == MAX_FILES){
			log_error(fs_tmp.log, "      No existe la ruta indicada");
			return -1;
		}
	}

	/* Busco la posicion en el vector de la tabla de archivos */
	i=0;
	while (i<MAX_FILES){
		if ( (0 == strcmp((char *)fs_tmp.file_table[i].fname, (char *)nombre_dir)) &&
				(fs_tmp.file_table[i].parent_directory == parent) &&
					(fs_tmp.file_table[i].state == REGULAR)){
			break;
		}
		i++;
	}
	if ( i == MAX_FILES){
		log_error(fs_tmp.log, "No existe el archivo a eliminar");
		return -1;
	}
	pos = i;

	/* Modifico datos en la Tabla de Archivos */
	fs_tmp.file_table[pos].state = DELETED;
	time((time_t *)&fs_tmp.file_table[pos].lastmod);
	// liberar bloques en bitmap

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);


	return 0;
}

static struct fuse_operations osada_oper = {
	.getattr = osada_getattr,
	.readdir   = osada_readdir,
	.open 	   = osada_open,
	.create    = osada_create,
	.unlink    = osada_unlink,
	.rmdir     = osada_rmdir,
	//.ftruncate = osada_ftruncate,
	.mkdir     = osada_mkdir,
	.rename    = osada_rename,
};

int main ( int argc , char * argv []) {
	printf("Pokedex Master!\n");

	fs_tmp.log = log_create("logFUSE.txt" , "PokedexServidor" , true , 0);

	FILE*	pokedex1;
	if (NULL==(pokedex1 = fopen("challenge.bin","rb"))){
		printf("Error al cargar el disco Pokedex");
		return EXIT_FAILURE;
	}
	fread(fs_tmp.header.buffer, OSADA_BLOCK_SIZE, 1, pokedex1);
	fclose(pokedex1);

	dirPokedex = open("challenge.bin", O_RDWR);
	pokedex = mmap(NULL, fs_tmp.header.fs_blocks * OSADA_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dirPokedex , 0);
	int32_t err = posix_madvise(pokedex, fs_tmp.header.fs_blocks * OSADA_BLOCK_SIZE, POSIX_MADV_RANDOM);
	if( err )
		printf("El madvise ha fallado. Número de error: %d \n", err);

	dirBlock = pokedex +(1 + fs_tmp.header.bitmap_blocks) * OSADA_BLOCK_SIZE ;
	memcpy(&fs_tmp.file_table, dirBlock, MAX_FILES * sizeof(osada_file));

	printf("Inicia FUSE\n");
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, NULL);
	return ret;
}
