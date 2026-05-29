

#include <stdio.h>
#include <stddef.h>
#include "json_extractor.h"

long json_extract_entry(FILE              *json_file,
                        const jnx_entry_t *entry,
                        char              *out_buf,
                        size_t             buf_size)
{
    long len = entry->offset_end - entry->offset_start;

    /* Rango vacío o invertido. */
    if (len <= 0) {
        if (buf_size > 0) out_buf[0] = '\0';
        return 0;
    }

    /* El buffer debe poder contener el fragmento más el terminador nulo. */
    if ((size_t)len + 1 > buf_size) {
        fprintf(stderr,
                "json_extract_entry: buffer muy pequeño "
                "(necesita %ld, tiene %zu) para '%s'\n",
                len + 1, buf_size, entry->path);
        return -1;
    }

    /* Salta al inicio del fragmento. */
    if (fseek(json_file, entry->offset_start, SEEK_SET) != 0) {
        perror("json_extract_entry: fseek");
        return -1;
    }

    /* Lee exactamente len bytes. */
    size_t n = fread(out_buf, 1, (size_t)len, json_file);

    /* Termina en nulo siempre, aunque se hayan leído menos bytes. */
    out_buf[n] = '\0';

    if ((long)n != len) {
        fprintf(stderr,
                "json_extract_entry: se esperaban %ld bytes, se leyeron %zu en '%s'\n",
                len, n, entry->path);
        return -1;
    }

    return (long)n;
}
