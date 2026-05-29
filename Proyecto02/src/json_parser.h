

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdio.h>
#include "../include/libjsonindex.h"

/* Evento emitido una vez por cada nodo indexado. */
typedef struct {
    char    path[JNX_MAX_PATH_LEN]; /* path absoluto del nodo               */
    long    offset_start;           /* offset del primer byte del valor     */
    long    offset_end;             /* offset del byte siguiente al último  */
    uint8_t type;                   /* jnx_value_type_t                     */
} json_event_t;

/* Firma del callback. El evento solo es válido durante la llamada.
   userdata se pasa tal cual desde json_parser_create(). */
typedef void (*json_event_cb)(const json_event_t *event, void *userdata);

/* Contexto opaco del parser. */
typedef struct json_parser json_parser_t;

/* Crea un contexto de parser.
   cb no puede ser NULL. userdata puede ser NULL.
   Retorna el manejador o NULL si falla la asignación. */
json_parser_t *json_parser_create(json_event_cb cb, void *userdata);

/* Recorre el archivo JSON y emite eventos.
   json_file debe estar abierto en modo lectura binaria ("rb")
   y posicionado en el byte 0. Lee hasta EOF.
   Retorna 0 si todo está bien, -1 si hay error de E/S. */
int json_parser_run(json_parser_t *p, FILE *json_file);

/* Libera todos los recursos del parser. */
void json_parser_free(json_parser_t *p);

#endif /* JSON_PARSER_H */
