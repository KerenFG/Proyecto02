
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "jnx_io.h"

/* Verifica el tamaño de los structs en tiempo de compilación. */
_Static_assert(sizeof(jnx_header_t) == 24,
               "jnx_header_t debe medir 24 bytes");
_Static_assert(sizeof(jnx_entry_t) == 536,
               "jnx_entry_t debe medir 536 bytes");

/* Estado interno del escritor. */
struct jnx_writer {
    FILE         *file;
    jnx_header_t  header;   /* se mantiene en memoria; se escribe al cerrar */
};

/* ── Escritura ───────────────────────────────────────────────────────────── */

jnx_writer_t *jnx_writer_open(const char *jnx_path, uint64_t source_size)
{
    FILE *f = fopen(jnx_path, "wb");
    if (!f) {
        perror(jnx_path);
        return NULL;
    }

    jnx_writer_t *w = malloc(sizeof(*w));
    if (!w) {
        fclose(f);
        return NULL;
    }

    w->file = f;

    /* Construye el header; entry_count empieza en 0 y se corrige al cerrar. */
    memcpy(w->header.magic, JNX_MAGIC, 4);
    w->header.version     = JNX_VERSION;
    w->header.entry_count = 0;
    w->header.source_size = source_size;

    if (fwrite(&w->header, sizeof(jnx_header_t), 1, f) != 1) {
        perror("jnx_writer_open: fwrite header");
        fclose(f);
        free(w);
        return NULL;
    }

    return w;
}

int jnx_writer_append(jnx_writer_t *w, const jnx_entry_t *entry)
{
    if (fwrite(entry, sizeof(jnx_entry_t), 1, w->file) != 1) {
        perror("jnx_writer_append: fwrite entry");
        return -1;
    }
    w->header.entry_count++;
    return 0;
}

int jnx_writer_close(jnx_writer_t *w)
{
    int rc = 0;

    /* Regresa al inicio y sobreescribe el header con el conteo final. */
    if (fseek(w->file, 0L, SEEK_SET) != 0) {
        perror("jnx_writer_close: fseek");
        rc = -1;
    } else if (fwrite(&w->header, sizeof(jnx_header_t), 1, w->file) != 1) {
        perror("jnx_writer_close: fwrite header final");
        rc = -1;
    }

    fclose(w->file);
    free(w);
    return rc;
}

/* ── Lectura ─────────────────────────────────────────────────────────────── */

jnx_index_t *jnx_index_load(const char *jnx_path)
{
    FILE *f = fopen(jnx_path, "rb");
    if (!f) {
        perror(jnx_path);
        return NULL;
    }

    /* Lee y valida el header. */
    jnx_header_t header;
    if (fread(&header, sizeof(jnx_header_t), 1, f) != 1) {
        fprintf(stderr, "jnx_index_load: header truncado en '%s'\n", jnx_path);
        fclose(f);
        return NULL;
    }

    if (memcmp(header.magic, JNX_MAGIC, 4) != 0) {
        fprintf(stderr, "jnx_index_load: magic inválido en '%s' (no es un .jnx)\n",
                jnx_path);
        fclose(f);
        return NULL;
    }

    if (header.version != JNX_VERSION) {
        fprintf(stderr, "jnx_index_load: versión incorrecta en '%s' "
                "(tiene %u, se esperaba %u)\n",
                jnx_path, header.version, JNX_VERSION);
        fclose(f);
        return NULL;
    }

    /* Reserva el contenedor del índice. */
    jnx_index_t *idx = malloc(sizeof(*idx));
    if (!idx) {
        fclose(f);
        return NULL;
    }
    idx->count   = header.entry_count;
    idx->entries = NULL;

    /* Carga todas las entradas en una sola llamada a fread. */
    if (idx->count > 0) {
        idx->entries = malloc(idx->count * sizeof(jnx_entry_t));
        if (!idx->entries) {
            fclose(f);
            free(idx);
            return NULL;
        }

        size_t n = fread(idx->entries, sizeof(jnx_entry_t), (size_t)idx->count, f);
        if (n != (size_t)idx->count) {
            fprintf(stderr, "jnx_index_load: se esperaban %llu entradas, se leyeron %zu\n",
                    (unsigned long long)idx->count, n);
            fclose(f);
            free(idx->entries);
            free(idx);
            return NULL;
        }
    }

    fclose(f);
    return idx;
}

void jnx_index_free(jnx_index_t *idx)
{
    if (!idx) return;
    free(idx->entries);
    free(idx);
}
