

#ifndef JSON_EXTRACTOR_H
#define JSON_EXTRACTOR_H

#include <stdio.h>
#include <stddef.h>
#include "../include/libjsonindex.h"
#include "jnx_io.h"

/* Lee el fragmento JSON de una entrada del índice.*/

long json_extract_entry(FILE              *json_file,
                        const jnx_entry_t *entry,
                        char              *out_buf,
                        size_t             buf_size);

#endif /* JSON_EXTRACTOR_H */
