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
uint32_t data_offset;
int32_t dirPokedex;


static int osada_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();

	log_info(fs_tmp.log, "OSADA read: %s-%d-%d", path, (int)offset, (int)size);

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
	nombre_dir[0] = '\0';
	nombre_dir++;

	if (strcmp((char *)fs_tmp.file_table[fi->fh].fname, (char *)nombre_dir) != 0)
		return -EINVAL;

	if (size == 0) return 0;

	if( fs_tmp.file_table[fi->fh].file_size < (offset+size) ){
		size = fs_tmp.file_table[fi->fh].file_size - offset;
	}

	uint32_t block_start = fs_tmp.file_table[fi->fh].first_block;
	div_t block_offset = div(offset, OSADA_BLOCK_SIZE);
	div_t block_to_read = div(size, OSADA_BLOCK_SIZE);
	uint32_t i =0;
	for ( i=0 ; i< block_offset.quot ; i++ ){
		block_start = fs_tmp.fat_osada[block_start];
	}
	//buf = calloc(size,1);
	int32_t copied=0, size_to_copy=0;
	dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE + block_offset.rem;

	size_to_copy = (OSADA_BLOCK_SIZE - block_offset.rem);
	if (size < size_to_copy) size_to_copy = size;
	memcpy(buf, dirBlock , size_to_copy);
	copied += size_to_copy;
	i=0;
	while (copied < size ){
		i += 1;
		if (block_start == 0xFFFFFFFF ) {
			log_error(fs_tmp.log, "--- QUILOMBO ---%d",i);
			return copied;
		}
		block_start = fs_tmp.fat_osada[block_start];
		//log_info(fs_tmp.log, "Size: %d - Copied: %d",(int) size, copied);
		if ((size - copied) < OSADA_BLOCK_SIZE)
			size_to_copy = (size - copied);
		else
			size_to_copy = OSADA_BLOCK_SIZE;

		dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE;
		//log_info(fs_tmp.log, "Paso 6");
		memcpy(buf + copied, dirBlock , size_to_copy);
		//log_info(fs_tmp.log, "Paso 7");
		copied = copied + size_to_copy;
	}
	return copied;
}

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

	int i=0, pos, parent = 0xFFFF;
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
	uint16_t parent;
	log_info(fs_tmp.log, "       PARENT: %s", path);
	uint8_t *path2 = (uint8_t *)calloc(strlen(path) + 1, (size_t) 1);
    strcpy((char *)path2, path);
    int8_t *nombre_dir = (int8_t *)strrchr((char *)path2, '/');
    nombre_dir[0] = '\0';
    nombre_dir++;

	if(!strcmp((char *)path2, "")){
    	while(i<MAX_FILES && 0 != strcmp((char *)table[i].fname, nombre_dir)){
    		i++;
		}
    	log_info(fs_tmp.log, "       Devuelve: %s - %d", path2, i);
    	return i;
	}else{
		parent = is_parent(table, path2);
		if ( parent == MAX_FILES ){
			return MAX_FILES;
		}else{
			while(i<MAX_FILES && (0 != strcmp((char *)table[i].fname, nombre_dir) ||
					parent != table[i].parent_directory))
				i++;
			log_info(fs_tmp.log, "       Devuelve: %s - %d", nombre_dir, i);
			return i;
		}
	}
}

static int osada_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	struct fuse_context* context = fuse_get_context();

	log_info(fs_tmp.log, "OSADA readdir: %s", path);

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
			return -ENOENT;
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
			return -ENOENT;
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
	fs_tmp.file_table[libre].first_block = 0xFFFFFFFF;

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

static int osada_open(const char *path, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();

	log_info(fs_tmp.log, "OSADA open: %s", path);
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
	uint32_t first_block = free_bit_bitmap(fs_tmp.bitmap,
			fs_tmp.header.bitmap_blocks * OSADA_BLOCK_SIZE);
	set_bitmap(fs_tmp.bitmap, first_block);
	fs_tmp.file_table[libre].first_block = first_block - data_offset;
	fs_tmp.fat_osada[first_block - data_offset] = 0xFFFFFFFF;

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

	//osada_open(path, fi);
	return 0;
}

int osada_statfs(const char *path, struct statvfs *statvfs)
{
	log_info(fs_tmp.log,"OSADA statfs: %s", path);
	statvfs->f_bsize = OSADA_BLOCK_SIZE;
	statvfs->f_blocks = fs_tmp.header.fs_blocks;
	statvfs->f_bavail = statvfs->f_bfree = fs_tmp.header.data_blocks;
	return 0;
}


int osada_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	log_info(fs_tmp.log, "OSADA write: %s-%d-%d", path, (int) size, (int)offset);

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;

	if (strcmp((char *)fs_tmp.file_table[fi->fh].fname, nombre_arch) != 0)
		return -EINVAL;

	if(fs_tmp.file_table[fi->fh].file_size < offset+size)
		osada_ftruncate (path, offset+size, fi);

	uint32_t block_start = fs_tmp.file_table[fi->fh].first_block;
	div_t block_offset = div(offset, OSADA_BLOCK_SIZE);
	div_t block_to_read = div(size, OSADA_BLOCK_SIZE);
	uint32_t i =0;
	//while (i< block_offset.quot && fs_tmp.fat_osada[block_start]!=0xFFFFFFFF)
	//	block_start = fs_tmp.fat_osada[block_start];
	for ( i=0 ; i< block_offset.quot ; i++ ){
		block_start = fs_tmp.fat_osada[block_start];
	}

	size_t copied=0, size_to_copy=0;
	dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE + block_offset.rem;

	size_to_copy = (OSADA_BLOCK_SIZE - block_offset.rem);
	if (size < size_to_copy) size_to_copy = size;
	memcpy(dirBlock, buf, size_to_copy);
	// Actualizo Bloque de Datos
	msync(dirBlock, OSADA_BLOCK_SIZE, MS_ASYNC);
	copied += size_to_copy;
	i=0;
	uint32_t new_block;
	while (copied < size ){
		i += 1;
		if (fs_tmp.fat_osada[block_start] == 0xFFFFFFFF){
			//Inserto nuevo bloque de datos
			new_block = free_bit_bitmap(fs_tmp.bitmap, fs_tmp.header.bitmap_blocks
					* OSADA_BLOCK_SIZE);
			set_bitmap(fs_tmp.bitmap, new_block);
			fs_tmp.fat_osada[block_start] = new_block - data_offset;
			fs_tmp.fat_osada[new_block - data_offset] = 0xFFFFFFFF;
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
		if ((size - copied) < OSADA_BLOCK_SIZE)
			size_to_copy = (size - copied);
		else
			size_to_copy = OSADA_BLOCK_SIZE;
		dirBlock = pokedex + (data_offset + block_start) * OSADA_BLOCK_SIZE;
		memcpy(dirBlock, buf + copied , size_to_copy);
		// Actualizo Bloque de Datos
		msync(dirBlock, size_to_copy, MS_ASYNC);
		copied = copied + size_to_copy;
	}
	fs_tmp.file_table[fi->fh].file_size = offset + copied;
	// Actualizo File Table
	dirBlock = pokedex + (1+fs_tmp.header.bitmap_blocks)*OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	return copied;
}

int osada_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	log_info(fs_tmp.log, "OSADA ftruncate: %s", path);
	int i=0;

	int8_t *path2 = calloc(strlen(path) + 1, 1);
	strcpy((char *)path2, path);
	int8_t *nombre_arch = (int8_t *)strrchr((char *)path2, '/');
	nombre_arch[0] = '\0';
	nombre_arch++;

	if (strcmp((char *)fs_tmp.file_table[fi->fh].fname, nombre_arch) != 0)
		return -EINVAL;

	if(offset == fs_tmp.file_table[fi->fh].file_size)
		 return 0;

	uint32_t block_start, aux_block;
	div_t aux_new = div(offset, OSADA_BLOCK_SIZE);
	div_t aux_old = div(fs_tmp.file_table[fi->fh].file_size, OSADA_BLOCK_SIZE);
	int32_t block_abm = ((aux_new.rem)?aux_new.quot+1:aux_new.quot) -
						((aux_old.rem)?aux_old.quot+1:aux_old.quot);


	if(block_abm > 0)
		while(block_abm){
			//add_block(fs_tmp->file_table[fi->fh].first_block);
			block_start = fs_tmp.file_table[fi->fh].first_block;
			while (fs_tmp.fat_osada[block_start] != 0xFFFFFFFF)
				block_start = fs_tmp.fat_osada[block_start];

			//Inserto nuevo bloque de datos
			aux_block = free_bit_bitmap(fs_tmp.bitmap, fs_tmp.header.bitmap_blocks
					* OSADA_BLOCK_SIZE);
			set_bitmap(fs_tmp.bitmap, aux_block);
			fs_tmp.fat_osada[block_start] = aux_block - data_offset;
			fs_tmp.fat_osada[aux_block - data_offset] = 0xFFFFFFFF;
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
	else if(block_abm < 0)
		while(block_abm){
			block_start = fs_tmp.file_table[fi->fh].first_block;
			while (fs_tmp.fat_osada[block_start] != 0xFFFFFFFF){
				aux_block = block_start;
				block_start = fs_tmp.fat_osada[block_start];
			}
			if (block_start != fs_tmp.file_table[fi->fh].first_block){
				//Elimino ultimo bloque de datos
				clean_bitmap(fs_tmp.bitmap, aux_block + data_offset);
				fs_tmp.fat_osada[aux_block] = 0xFFFFFFFF;
				// Actualizo BitMap
				dirBlock = pokedex + OSADA_BLOCK_SIZE ;
				memcpy(dirBlock, fs_tmp.bitmap, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks);
				msync(dirBlock, OSADA_BLOCK_SIZE * fs_tmp.header.bitmap_blocks, MS_ASYNC);
				// Actualizo FAT
				dirBlock = pokedex + fs_tmp.header.allocations_table_offset * OSADA_BLOCK_SIZE;
				memcpy(dirBlock, fs_tmp.fat_osada, (fs_tmp.header.data_blocks * 4));
				msync(dirBlock, (fs_tmp.header.data_blocks * 4), MS_ASYNC);
			}
			block_abm++;
		}
	fs_tmp.file_table[fi->fh].file_size = offset;
	// Actualizo File Table
	dirBlock = pokedex + (1+fs_tmp.header.bitmap_blocks)*OSADA_BLOCK_SIZE ;
	memcpy(dirBlock, fs_tmp.file_table, MAX_FILES * sizeof(osada_file));
	msync(dirBlock, MAX_FILES * sizeof(osada_file), MS_ASYNC);
	return 0;
}

int osada_truncate(const char * path, off_t offset) {
	// funcion dummy para que no se queje de "function not implemented"
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

set_bitmap(uint8_t *bitmap, uint32_t pos){
	bitmap[pos/8] |= 1<<(7-(pos%8));
	return 0;
}

clean_bitmap(uint8_t *bitmap, uint32_t pos){
	bitmap[pos/8] &= ~(1<<(7-(pos%8)));
	return 0;
}

uint32_t bit_bitmap(uint8_t *bitmap, uint32_t pos){
	return ((bitmap[pos/8] & (1<<(7-(pos%8)))) ? 1 : 0);
}

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
}

static struct fuse_operations osada_oper = {
	.getattr   = osada_getattr,
	.readdir   = osada_readdir,
	.open 	   = osada_open,
	.create    = osada_create,
	.read      = osada_read,
	.truncate  = osada_truncate,
	.write     = osada_write,
	.unlink    = osada_unlink,
	.rmdir     = osada_rmdir,
	.ftruncate = osada_ftruncate,
	.mkdir     = osada_mkdir,
	.rename    = osada_rename,
	.statfs    = osada_statfs
};


int main ( int argc , char * argv []) {
	printf("Pokedex Master!\n");
	char *disco = "challenge.bin";
	fs_tmp.log = log_create("logFUSE.txt" , "PokedexServidor" , true , 0);

	FILE*	pokedex1;
	if (NULL==(pokedex1 = fopen(disco,"rb"))){
		printf("Error al cargar el disco Pokedex");
		return EXIT_FAILURE;
	}
	fread(fs_tmp.header.buffer, OSADA_BLOCK_SIZE, 1, pokedex1);
	fclose(pokedex1);

	printf("Datos FS:\n");
	printf("    FS Blocks:%d\n", fs_tmp.header.fs_blocks);
	printf("    BitMap Blocks:%d\n", fs_tmp.header.bitmap_blocks);
	printf("    Inicio FAT:%d\n", fs_tmp.header.allocations_table_offset);
	printf("    Data Blocks:%d\n", fs_tmp.header.data_blocks);

	dirPokedex = open(disco, O_RDWR); //O_RDWR
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

	data_offset  =	fs_tmp.header.allocations_table_offset + fat_size_block;

	/*
	uint32_t freeB = free_bit_bitmap(fs_tmp.bitmap,
			fs_tmp.header.bitmap_blocks * OSADA_BLOCK_SIZE);
	printf("\n Bit libre: %08X", freeB);
	int i;
	for(i=0 ; i<MAX_FILES ; i++)
		if (fs_tmp.file_table[i].first_block == freeB)
			printf("\nPertenece al archivo: %s\n", fs_tmp.file_table[i].fname);

	for (i=0 ; i < fs_tmp.header.data_blocks ; i++)
		if (fs_tmp.fat_osada[i] == (freeB-data_offset))
				printf("\nPertenece al archivo: %d\n", fs_tmp.fat_osada[i]);
	i=0;

	i=0;
	while(fs_tmp.bitmap[i] == 0xFF){
		i++;
	}
	printf("\nprimer libre en byte: %d - Valor: %X\n", i*8,fs_tmp.bitmap[i]*8);

	show_bitmap(fs_tmp.bitmap, i, i+5);
	printf("\nPrimer bloque libre: %d\n", free_bit_bitmap(fs_tmp.bitmap,
			fs_tmp.header.bitmap_blocks*OSADA_BLOCK_SIZE) );*/

	printf("\nInicia FUSE\n");
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, NULL);
	return ret;
}
