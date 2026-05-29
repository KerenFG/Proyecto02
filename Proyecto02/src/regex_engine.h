

#ifndef REGEX_ENGINE_H
#define REGEX_ENGINE_H

#include <stddef.h>
#include "../include/libjsonindex.h"
#include "jnx_io.h"

/* Resultado de búsqueda: arreglo de punteros al índice original.
   El jnx_index_t debe permanecer válido mientras se use este resultado. */
typedef struct {
    const jnx_entry_t **entries;   /* punteros no propietarios al índice    */
    size_t               count;    /* cantidad de entradas que coincidieron  */
} regex_result_t;

/* Busca en idx todas las entradas cuyo path coincida con pattern (POSIX ERE).
   Retorna el resultado (liberar con regex_result_free) o NULL si el patrón
   es inválido o falla la asignación de memoria. */
regex_result_t *regex_search(const jnx_index_t *idx, const char *pattern);

/* Libera el resultado (no libera las entradas, son punteros al índice). */
void regex_result_free(regex_result_t *r);

#endif 
