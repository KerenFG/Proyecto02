

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/libjsonindex.h"
#include "index_engine.h"
#include "jnx_io.h"
#include "regex_engine.h"
#include "json_extractor.h"

/* ── jnx_build ──────────────────────────────────────────────────────────── */

int jnx_build(const char *json_path, const char *jnx_path)
{
    return index_engine_build(json_path, jnx_path);
}

/* ── jnx_search ─────────────────────────────────────────────────────────── */

/* Tamaño inicial del buffer de extracción (~4 MB).
 * Crece con realloc si un fragmento es más grande. */
#define EXTRACT_BUF_INIT 4096

int jnx_search(const char *json_path, const char *jnx_path,
               const char *pattern, FILE *out)
{
    /* Carga el índice completo en memoria. */
    jnx_index_t *idx = jnx_index_load(jnx_path);
    if (!idx) return -1;

    /* Busca las entradas cuyo path coincida con el patrón. */
    regex_result_t *result = regex_search(idx, pattern);
    if (!result) {
        jnx_index_free(idx);
        return -1;
    }

    /* Abre el .json original para extraer los fragmentos. */
    FILE *json_file = fopen(json_path, "rb");
    if (!json_file) {
        perror(json_path);
        regex_result_free(result);
        jnx_index_free(idx);
        return -1;
    }

    /* Buffer de extracción; crece si algún fragmento lo supera. */
    size_t  buf_cap = EXTRACT_BUF_INIT;
    char   *buf     = malloc(buf_cap);
    if (!buf) {
        fclose(json_file);
        regex_result_free(result);
        jnx_index_free(idx);
        return -1;
    }

    /* Si no hay coincidencias, la salida debe ser un arreglo JSON vacío. */
    if (result->count == 0) {
        fprintf(out, "[]\n");
        free(buf);
        fclose(json_file);
        regex_result_free(result);
        jnx_index_free(idx);
        return 0;
    }

    fprintf(out, "[\n");

    for (size_t i = 0; i < result->count; i++) {
        const jnx_entry_t *e = result->entries[i];

        /* Agranda el buffer si el fragmento no cabe. */
        long frag_len = e->offset_end - e->offset_start;
        if (frag_len > 0 && (size_t)(frag_len + 1) > buf_cap) {
            size_t new_cap = (size_t)(frag_len + 1);
            char *tmp = realloc(buf, new_cap);
            if (!tmp) {
                fprintf(stderr, "jnx_search: sin memoria para el fragmento\n");
                break;
            }
            buf     = tmp;
            buf_cap = new_cap;
        }

        long nread = json_extract_entry(json_file, e, buf, buf_cap);
        if (nread < 0) {
            fprintf(stderr, "jnx_search: error al extraer '%s'\n", e->path);
            continue;
        }

        /* Coma antes de cada entrada excepto la primera. */
        if (i > 0) fprintf(out, ",\n");
        fprintf(out, "  { \"path\": \"%s\", \"value\": %.*s }",
                e->path, (int)nread, buf);
    }

    fprintf(out, "\n]\n");

    free(buf);
    fclose(json_file);

    int matches = (int)result->count;
    regex_result_free(result);
    jnx_index_free(idx);
    return matches;
}
