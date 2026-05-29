

#ifndef JNX_IO_H
#define JNX_IO_H

#include <stdint.h>
#include <stdio.h>
#include "../include/libjsonindex.h"

/* Cabecera del archivo .jnx — tamaño fijo, se escribe una sola vez. */
typedef struct {
    char     magic[4];        /* "JNX1" — sin terminador nulo               */
    uint32_t version;         /* JNX_VERSION                                */
    uint64_t entry_count;     /* total de entradas en el archivo            */
    uint64_t source_size;     /* tamaño del .json original al momento de indexar */
} jnx_header_t;

/* Índice cargado en memoria para búsquedas. */
typedef struct {
    jnx_entry_t *entries;     /* arreglo en heap, propiedad de esta struct  */
    uint64_t     count;       /* número de entradas válidas                 */
} jnx_index_t;

/* Manejador opaco de escritura. */
typedef struct jnx_writer jnx_writer_t;

/* Abre un .jnx para escritura. Escribe header provisional.
   source_size es el tamaño del .json que se está indexando.
   Retorna el manejador o NULL si hay error. */
jnx_writer_t *jnx_writer_open(const char *jnx_path, uint64_t source_size);

/* Agrega una entrada al .jnx abierto.
   Retorna 0 si todo está bien, -1 si hay error de escritura. */
int jnx_writer_append(jnx_writer_t *w, const jnx_entry_t *entry);

/* Finaliza el archivo: regresa al inicio, sobreescribe el header
   con el conteo real, cierra el archivo y libera el manejador.
   Retorna 0 si todo está bien, -1 si hay error. */
int jnx_writer_close(jnx_writer_t *w);

/* Carga todo el .jnx en memoria.
   El llamador debe liberar con jnx_index_free cuando termine.
   Retorna el índice o NULL si hay error. */
jnx_index_t *jnx_index_load(const char *jnx_path);

/* Libera la memoria del índice. */
void jnx_index_free(jnx_index_t *idx);

#endif /* JNX_IO_H */
