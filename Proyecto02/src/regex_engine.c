

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "regex_engine.h"

regex_result_t *regex_search(const jnx_index_t *idx, const char *pattern)
{
    /* Compila el patrón una sola vez. */
    regex_t re;
    int rc = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB);
    if (rc != 0) {
        char errbuf[256];
        regerror(rc, &re, errbuf, sizeof(errbuf));
        fprintf(stderr, "regex_search: patrón inválido '%s': %s\n", pattern, errbuf);
        return NULL;
    }

    /* Reserva el contenedor del resultado. */
    regex_result_t *result = malloc(sizeof(*result));
    if (!result) {
        regfree(&re);
        return NULL;
    }
    result->count   = 0;
    result->entries = NULL;

    /* Índice vacío: retorna resultado vacío (no NULL). */
    if (!idx || idx->count == 0) {
        regfree(&re);
        return result;
    }

    /* Reserva el peor caso: todas las entradas podrían coincidir. */
    result->entries = malloc(idx->count * sizeof(const jnx_entry_t *));
    if (!result->entries) {
        regfree(&re);
        free(result);
        return NULL;
    }

    /* Aplica el patrón a cada path del índice. */
    for (uint64_t i = 0; i < idx->count; i++) {
        /* regexec retorna 0 si hay coincidencia, REG_NOMATCH si no. */
        if (regexec(&re, idx->entries[i].path, 0, NULL, 0) == 0)
            result->entries[result->count++] = &idx->entries[i];
    }

    regfree(&re);

    /* Ajusta el arreglo al tamaño real de coincidencias. */
    if (result->count == 0) {
        /* Sin coincidencias: libera el arreglo pero retorna el struct válido. */
        free(result->entries);
        result->entries = NULL;
    } else if (result->count < (size_t)idx->count) {
        /* Reduce el arreglo al tamaño real; si realloc falla no es un error. */
        const jnx_entry_t **shrunk = realloc(
            result->entries,
            result->count * sizeof(const jnx_entry_t *)
        );
        if (shrunk) result->entries = shrunk;
    }

    return result;
}

void regex_result_free(regex_result_t *r)
{
    if (!r) return;
    free(r->entries);
    free(r);
}
