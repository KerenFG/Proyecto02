

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index_engine.h"
#include "json_parser.h"
#include "jnx_io.h"

/* Contexto que se pasa al callback del parser. */
typedef struct {
    jnx_writer_t *writer;
    int           count;
    int           error;    /* se pone en 1 si alguna escritura falla */
} build_ctx_t;

/* Recibe cada evento del parser y lo envía al escritor. */
static void on_event(const json_event_t *event, void *userdata)
{
    build_ctx_t *ctx = (build_ctx_t *)userdata;
    if (ctx->error) return;

    jnx_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.path, event->path, JNX_MAX_PATH_LEN - 1);
    e.offset_start = event->offset_start;
    e.offset_end   = event->offset_end;
    e.type         = event->type;

    if (jnx_writer_append(ctx->writer, &e) != 0)
        ctx->error = 1;
    else
        ctx->count++;
}

/* ── API pública ────────────────────────────────────────────────────────── */

int index_engine_build(const char *json_path, const char *jnx_path)
{
    /* Abre el .json para lectura. */
    FILE *json_file = fopen(json_path, "rb");
    if (!json_file) {
        perror(json_path);
        return -1;
    }

    /* Obtiene el tamaño del .json para guardarlo en el header (sin usar stat.h). */
    if (fseek(json_file, 0L, SEEK_END) != 0) {
        perror("index_engine_build: fseek");
        fclose(json_file);
        return -1;
    }
    long source_size = ftell(json_file);
    rewind(json_file);

    if (source_size < 0) {
        fprintf(stderr, "index_engine_build: no se pudo obtener el tamaño de '%s'\n",
                json_path);
        fclose(json_file);
        return -1;
    }

    /* Abre el .jnx para escritura. */
    jnx_writer_t *writer = jnx_writer_open(jnx_path, (uint64_t)source_size);
    if (!writer) {
        fclose(json_file);
        return -1;
    }

    /* Conecta el parser con el escritor. */
    build_ctx_t ctx = { .writer = writer, .count = 0, .error = 0 };

    json_parser_t *parser = json_parser_create(on_event, &ctx);
    if (!parser) {
        jnx_writer_close(writer);
        fclose(json_file);
        return -1;
    }

    json_parser_run(parser, json_file);
    json_parser_free(parser);
    fclose(json_file);

    if (jnx_writer_close(writer) != 0)
        return -1;

    if (ctx.error) {
        fprintf(stderr, "index_engine_build: hubo errores al escribir entradas\n");
        return -1;
    }

    return ctx.count;
}
