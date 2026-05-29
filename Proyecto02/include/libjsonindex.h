/* libjsonindex.h — API pública de la librería jqindex. */

#ifndef LIBJSONINDEX_H
#define LIBJSONINDEX_H

#include <stdint.h>
#include <stdio.h>

/* ── Constantes ─────────────────────────────────────────────────────────── */

#define JNX_VERSION      1
#define JNX_MAGIC        "JNX1"    /* identificador del archivo .jnx         */
#define JNX_MAX_PATH_LEN 512       /* longitud máxima de un path JSON         */
#define JNX_STACK_DEPTH  128       /* profundidad máxima de anidamiento       */

/* ── Tipos de valor JSON ────────────────────────────────────────────────── */
/* Un byte por entrada; se usan letras ASCII para facilitar depuración.     */
typedef enum {
    JNX_TYPE_OBJECT = 'O',
    JNX_TYPE_ARRAY  = 'A',
    JNX_TYPE_STRING = 'S',
    JNX_TYPE_NUMBER = 'N',
    JNX_TYPE_BOOL   = 'B',
    JNX_TYPE_NULL   = 'Z',
} jnx_value_type_t;

/* ── Entrada del índice: representa un nodo del árbol JSON ─────────────── */
/*
 * offset_start : posición del primer byte del valor en el .json original.
 * offset_end   : posición del byte siguiente al último byte del valor.
 * longitud del fragmento = offset_end - offset_start
 *
 * Layout binario en x86-64 Linux (verificado con _Static_assert):
 *   path[512]   offset   0   tamaño 512
 *   offset_start offset 512  tamaño   8
 *   offset_end  offset 520   tamaño   8
 *   type        offset 528   tamaño   1
 *   _pad[7]     offset 529   tamaño   7  (relleno explícito)
 *   total                    tamaño 536
 */
typedef struct {
    char    path[JNX_MAX_PATH_LEN]; /* ej. "$.usuario.puntajes[2]"           */
    long    offset_start;           /* offset de inicio del valor en .json   */
    long    offset_end;             /* offset del byte siguiente al final    */
    uint8_t type;                   /* jnx_value_type_t                      */
    uint8_t _pad[7];                /* relleno — no usar                     */
} jnx_entry_t;

/* ── API pública ────────────────────────────────────────────────────────── */

/* Indexa json_path y escribe el índice en jnx_path.
   Retorna cantidad de entradas o -1 si hay error. */
int jnx_build(const char *json_path, const char *jnx_path);

/* Carga jnx_path, busca paths que coincidan con pattern (POSIX ERE),
   extrae los fragmentos del json_path original y los imprime en out.
   Retorna cantidad de coincidencias o -1 si hay error. */
int jnx_search(const char *json_path, const char *jnx_path,
               const char *pattern, FILE *out);

#endif /* LIBJSONINDEX_H */
